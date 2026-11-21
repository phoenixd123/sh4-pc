/* Updating the launcher from its own GitHub releases: the newest tag, its win64 zip, and putting that zip's files
 * next to the running executable. See updater.h.
 *
 * The GitHub API is read-only here and needs no key: /repos/<owner>/<repo>/releases/latest answers the newest
 * published release with its assets. Only the asset named like the release's launcher zip is used.
 *
 * Nothing is unpacked over the game: the release holds the launcher, SDL2.dll and README.txt, so a copy that
 * lives beside a game folder keeps its sh4.ini, savedata/ and mods/ - the install only overwrites the files the
 * release itself carries.
 */
#include "updater.h"

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"

volatile int updater_progress;

const char *updater_version(void) { return PORT_VERSION; }

/* ---- versions ---------------------------------------------------------------------------------------- */

/* The three numbers of a version, ignoring a leading v and anything after the third one. */
static void ver3(const char *s, long v[3]) {
    v[0] = v[1] = v[2] = 0;
    while (*s && (*s < '0' || *s > '9')) s++;
    for (int i = 0; i < 3 && *s; i++) {
        v[i] = strtol(s, (char **)&s, 10);
        if (*s != '.') break;
        s++;
    }
}

int updater_version_newer(const char *a, const char *b) {
    long x[3], y[3];
    ver3(a, x);
    ver3(b, y);
    for (int i = 0; i < 3; i++)
        if (x[i] != y[i]) return x[i] > y[i];
    return 0;
}

/* ---- HTTP -------------------------------------------------------------------------------------------- */

typedef struct { char *p; size_t n, cap; } Mem;
typedef struct { FILE *f; long long got; } Out;

static void mem_free(Mem *m) {
    free(m->p);
    m->p = NULL;
    m->n = m->cap = 0;
}

static int mem_add(Mem *m, const char *d, DWORD n) {
    if (m->n + n + 1 > m->cap) {
        size_t cap = m->cap ? m->cap * 2 : 65536;
        while (cap < m->n + n + 1) cap *= 2;
        char *p = realloc(m->p, cap);
        if (!p) return 0;
        m->p = p;
        m->cap = cap;
    }
    memcpy(m->p + m->n, d, n);
    m->n += n;
    m->p[m->n] = 0;
    return 1;
}

static const wchar_t *widen(const char *s) {
    static wchar_t buf[2048];
    MultiByteToWideChar(CP_UTF8, 0, s, -1, buf, 2048);
    return buf;
}

static int mem_sink(void *ctx, const char *d, DWORD n) { return mem_add((Mem *)ctx, d, n); }

static int file_sink(void *ctx, const char *d, DWORD n) {
    Out *o = ctx;
    if (fwrite(d, 1, n, o->f) != n) return 0;
    o->got += n;
    return 1;
}

/* One GET with the two headers GitHub asks for. `sink` receives the body as it arrives. 1 for 200, else 0 with
 * the reason in err. */
static int http_get(const char *url, int (*sink)(void *, const char *, DWORD), void *ctx, long long *total,
                    char *err, size_t errn) {
    URL_COMPONENTS uc;
    wchar_t host[256], path[1600];
    HINTERNET ses = NULL, con = NULL, req = NULL;
    int ok = 0;
    memset(&uc, 0, sizeof uc);
    uc.dwStructSize = sizeof uc;
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 1600;
    if (!WinHttpCrackUrl(widen(url), 0, 0, &uc)) {
        snprintf(err, errn, "That address could not be read: %s", url);
        return 0;
    }
    ses = WinHttpOpen(L"SH3-PC", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) goto done;
    WinHttpSetTimeouts(ses, 15000, 15000, 30000, 30000);
    con = WinHttpConnect(ses, host, uc.nPort, 0);
    if (!con) goto done;
    req = WinHttpOpenRequest(con, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                             uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (!req) goto done;
    /* GitHub answers 403 to a request with no User-Agent, so one is always sent. */
    WinHttpAddRequestHeaders(req,
                             L"User-Agent: sh4-pc-launcher\r\nAccept: application/vnd.github+json",
                             (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);
    if (!WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) goto done;
    if (!WinHttpReceiveResponse(req, NULL)) goto done;
    {
        DWORD code = 0, len = sizeof code;
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                            &code, &len, WINHTTP_NO_HEADER_INDEX);
        if (total) {
            wchar_t cl[64];
            DWORD n = sizeof cl;
            *total = 0;
            if (WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, cl, &n,
                                    WINHTTP_NO_HEADER_INDEX))
                *total = wcstol(cl, NULL, 10);
        }
        if (code != 200) {
            if (code == 404) snprintf(err, errn, "There is no release to update to yet.");
            else if (code == 403) snprintf(err, errn, "GitHub is limiting how often this is asked (403). Try again later.");
            else snprintf(err, errn, "GitHub answered %lu.", (unsigned long)code);
            goto done;
        }
    }
    for (;;) {
        DWORD have = 0;
        if (!WinHttpQueryDataAvailable(req, &have)) goto done;
        if (!have) break;
        char buf[65536];
        DWORD got = 0;
        if (!WinHttpReadData(req, buf, have > sizeof buf ? (DWORD)sizeof buf : have, &got)) goto done;
        if (!got) break;
        if (sink && !sink(ctx, buf, got)) goto done;
        if (ctx && total && *total > 0) updater_progress = (int)(((Out *)ctx)->got * 100 / *total);
    }
    ok = 1;
done:
    if (!ok && err[0] == 0) snprintf(err, errn, "Could not reach GitHub (WinHTTP error %lu).", (unsigned long)GetLastError());
    if (req) WinHttpCloseHandle(req);
    if (con) WinHttpCloseHandle(con);
    if (ses) WinHttpCloseHandle(ses);
    return ok;
}

/* ---- the release's answers --------------------------------------------------------------------------- */

#define MAX_MATCHES 12

static const char *find_sub(const char *hay, size_t hn, const char *needle, size_t nn) {
    if (nn > hn) return NULL;
    for (size_t i = 0; i + nn <= hn; i++)
        if (!memcmp(hay + i, needle, nn)) return hay + i;
    return NULL;
}

/* The string value of "key" at or after p, decoded into out, with the position after it in *after (may be NULL).
 * Returns 1 when the key and a string were found before end. */
static int json_str(const char *p, const char *end, const char *key, char *out, size_t n, const char **after) {
    char pat[80];
    size_t kl;
    snprintf(pat, sizeof pat, "\"%s\"", key);
    kl = strlen(pat);
    out[0] = 0;
    while (p + kl < end) {
        p = find_sub(p, (size_t)(end - p), pat, kl);
        if (!p) return 0;
        const char *q = p + kl;
        while (q < end && (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n')) q++;
        if (q >= end || *q != ':') { p++; continue; }
        q++;
        while (q < end && (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n')) q++;
        size_t o = 0;
        if (q < end && *q == '"') {
            q++;
            while (q < end && *q != '"' && o + 1 < n) {
                if (*q == '\\' && q + 1 < end) {
                    q++;
                    char c = *q;
                    if (c == 'n') c = '\n';
                    else if (c == 't') c = '\t';
                    else if (c == 'r') c = '\r';
                    else if (c == 'u') { q += 4; c = '?'; }
                    out[o++] = c;
                    q++;
                } else
                    out[o++] = *q++;
            }
            out[o] = 0;
            if (after) *after = q;
            return 1;
        }
        /* Not a string: keep looking for the next place the key appears. */
        p = q;
    }
    return 0;
}

/* A bare number (sizes are not quoted in the API's answers). */
static long long json_int(const char *p, const char *end, const char *key, long long def) {
    char pat[80];
    size_t kl;
    snprintf(pat, sizeof pat, "\"%s\"", key);
    kl = strlen(pat);
    while (p + kl < end) {
        p = find_sub(p, (size_t)(end - p), pat, kl);
        if (!p) return def;
        const char *q = p + kl;
        while (q < end && (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n')) q++;
        if (q >= end || *q != ':') { p++; continue; }
        q++;
        while (q < end && (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n')) q++;
        if (q < end && (*q == '-' || (*q >= '0' && *q <= '9'))) return atoll(q);
        p = q;
    }
    return def;
}

/* The end of the array that starts at or after p: the matching ] , quote-aware. */
static const char *array_end(const char *p, const char *stop) {
    int depth = 0, in_str = 0;
    for (; p < stop; p++) {
        char c = *p;
        if (in_str) {
            if (c == '\\') p++;
            else if (c == '"') in_str = 0;
            continue;
        }
        if (c == '"') in_str = 1;
        else if (c == '[') depth++;
        else if (c == ']' && --depth == 0) return p + 1;
    }
    return NULL;
}

/* The next { .. } inside [p, stop), quote-aware, with its end in *oend. */
static const char *object_next(const char *p, const char *stop, const char **oend) {
    while (p < stop && *p != '{') p++;
    if (p >= stop) return NULL;
    const char *s = p;
    int depth = 0, in_str = 0;
    for (; p < stop; p++) {
        char c = *p;
        if (in_str) {
            if (c == '\\') p++;
            else if (c == '"') in_str = 0;
            continue;
        }
        if (c == '"') in_str = 1;
        else if (c == '{') depth++;
        else if (c == '}' && --depth == 0) {
            *oend = p + 1;
            return s;
        }
    }
    return NULL;
}

static int ends_with_zip(const char *name) {
    size_t n = strlen(name);
    return n > 4 && !strcasecmp(name + n - 4, ".zip");
}

/* The launcher zip among the release's assets: the win64 one if it is there, else the first .zip. */
static int pick_asset(const char *json, size_t len, UpdaterRelease *r) {
    const char *stop = json + len, *oe, *p = NULL;
    char first_url[700] = "";
    long long first_size = 0;
    const char *a = find_sub(json, len, "\"assets\"", 8);
    while (a && a < stop && *a != '[') a++;
    if (!a || a >= stop) return 0;
    {
        const char *aend = array_end(a, stop);
        if (!aend) return 0;
        while ((p = object_next(p ? p : a + 1, aend, &oe)) != NULL) {
            char name[140] = "", url[700] = "";
            long long size;
            json_str(p, oe, "name", name, sizeof name, NULL);
            json_str(p, oe, "browser_download_url", url, sizeof url, NULL);
            size = json_int(p, oe, "size", 0);
            if (url[0] && ends_with_zip(name)) {
                if (strstr(name, "win64")) {  /* the release's own launcher asset */
                    snprintf(r->url, sizeof r->url, "%s", url);
                    r->size = size;
                    return 1;
                }
                if (!first_url[0]) {
                    snprintf(first_url, sizeof first_url, "%s", url);
                    first_size = size;
                }
            }
            p = oe;
        }
    }
    if (first_url[0]) {
        snprintf(r->url, sizeof r->url, "%s", first_url);
        r->size = first_size;
        return 1;
    }
    return 0;
}

int updater_check(UpdaterRelease *r, char *err, size_t errn) {
    char url[700];
    Mem m = {0};
    memset(r, 0, sizeof *r);
    snprintf(url, sizeof url, "%s/repos/%s/releases/latest",
             getenv("SH4_UPDATE_API") && *getenv("SH4_UPDATE_API") ? getenv("SH4_UPDATE_API") : "https://api.github.com",
             UPDATER_REPO);
    if (!http_get(url, mem_sink, &m, NULL, err, errn)) {
        mem_free(&m);
        return -1;
    }
    if (!json_str(m.p, m.p + m.n, "tag_name", r->tag, sizeof r->tag, NULL) || !r->tag[0]) {
        snprintf(err, errn, "The release has no version to compare with.");
        mem_free(&m);
        return -1;
    }
    const char *v = r->tag;
    while (*v == 'v' || *v == 'V' || *v == ' ') v++;
    snprintf(r->version, sizeof r->version, "%s", v);
    json_str(m.p, m.p + m.n, "name", r->name, sizeof r->name, NULL);
    if (!r->name[0]) snprintf(r->name, sizeof r->name, "%s", r->tag);
    if (!pick_asset(m.p, m.n, r)) {
        snprintf(err, errn, "Release %s has no launcher zip to download.", r->tag);
        mem_free(&m);
        return -1;
    }
    mem_free(&m);
    return updater_version_newer(r->version, PORT_VERSION) ? 1 : 0;
}

/* ---- download and install ---------------------------------------------------------------------------- */

int updater_download(const char *url, char *zip, size_t n, char *err, size_t errn) {
    char tmp[700];
    DWORD got = GetTempPathA(sizeof tmp, tmp);
    Out o;
    FILE *f;
    long long total = 0;
    updater_progress = 0;
    if (!got) {
        snprintf(err, errn, "Windows did not name a temporary folder.");
        return 0;
    }
    snprintf(zip, n, "%ssh4-pc-update.zip", tmp);
    f = fopen(zip, "wb");
    if (!f) {
        snprintf(err, errn, "The update could not be saved to\n\n%s", zip);
        return 0;
    }
    memset(&o, 0, sizeof o);
    o.f = f;
    int ok = http_get(url, file_sink, &o, &total, err, errn);
    fclose(f);
    if (!ok || o.got <= 0) {
        DeleteFileA(zip);
        if (err[0] == 0) snprintf(err, errn, "The download was empty.");
        return 0;
    }
    updater_progress = 100;
    return 1;
}

static int is_folder(const char *p) {
    DWORD a = GetFileAttributesA(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

/* The release's own files, unpacked flat into stage: the zip has no folders in it. */
static int unzip_flat(const char *zip, const char *stage, char *err, size_t errn) {
    mz_zip_archive z;
    int got = 0;
    memset(&z, 0, sizeof z);
    if (!mz_zip_reader_init_file(&z, zip, 0)) {
        snprintf(err, errn, "The update could not be read as a zip. If the download was interrupted, try again.");
        return 0;
    }
    int count = (int)mz_zip_reader_get_num_files(&z);
    for (int i = 0; i < count; i++) {
        mz_zip_archive_file_stat st;
        char out[1000];
        if (!mz_zip_reader_file_stat(&z, i, &st)) continue;
        if (st.m_is_directory) continue;
        if (strchr(st.m_filename, '/') || strchr(st.m_filename, '\\')) continue;  /* never into a folder */
        if (strstr(st.m_filename, "..")) continue;
        if (!strcasecmp(st.m_filename, "install.cmd") || !strcasecmp(st.m_filename, "install.bat")) continue;
        snprintf(out, sizeof out, "%s\\%s", stage, st.m_filename);
        if (!mz_zip_reader_extract_to_file(&z, i, out, 0)) {
            snprintf(err, errn, "Unpacking stopped at\n\n%s\n\nThe update may be incomplete.", st.m_filename);
            mz_zip_reader_end(&z);
            return 0;
        }
        got++;
    }
    mz_zip_reader_end(&z);
    if (!got) {
        snprintf(err, errn, "The update archive had no files in it.");
        return 0;
    }
    return 1;
}

int updater_can_install(void) { return 1; }

int updater_install(const char *zip, char *err, size_t errn) {
    char tmp[700], stage[900], cmd[1000], exe[900], app[900], name[260];
    DWORD got = GetTempPathA(sizeof tmp, tmp), len;
    FILE *f;
    char *slash;
    if (!got) {
        snprintf(err, errn, "Windows did not name a temporary folder.");
        return 0;
    }
    snprintf(stage, sizeof stage, "%ssh4-pc-update", tmp);
    if (!is_folder(stage) && !CreateDirectoryA(stage, NULL)) {
        snprintf(err, errn, "The update could not be unpacked: %s", stage);
        return 0;
    }
    /* An older attempt may have left files here; the new ones overwrite them. */
    if (!unzip_flat(zip, stage, err, errn)) return 0;
    len = GetModuleFileNameA(NULL, exe, sizeof exe);
    if (!len || len >= sizeof exe) {
        snprintf(err, errn, "The launcher could not find its own path.");
        return 0;
    }
    snprintf(app, sizeof app, "%s", exe);
    slash = strrchr(app, '\\');
    if (!slash) {
        snprintf(err, errn, "The launcher's own folder could not be worked out.");
        return 0;
    }
    snprintf(name, sizeof name, "%s", slash + 1);
    *slash = 0;
    snprintf(cmd, sizeof cmd, "%ssh4-pc-update.cmd", tmp);
    f = fopen(cmd, "wb");
    if (!f) {
        snprintf(err, errn, "The update script could not be written to\n\n%s", cmd);
        return 0;
    }
    /* The executable and SDL2.dll are in use until this process ends, so copy until it works; then start the
     * launcher again. `start` needs an empty title, or a quoted path would be taken for one. */
    fprintf(f,
            "@echo off\r\n"
            "setlocal\r\n"
            "set T=0\r\n"
            ":retry\r\n"
            "set /a T+=1\r\n"
            "copy /y \"%s\\*.*\" \"%s\" >nul 2>&1\r\n"
            "if errorlevel 1 if %%T%% lss 90 (\r\n"
            "  ping -n 2 127.0.0.1 >nul\r\n"
            "  goto retry\r\n"
            ")\r\n"
            "start \"\" \"%s\"\r\n"
            "del \"%%~f0\"\r\n",
            stage, app, exe);
    fclose(f);
    if (getenv("SH4_UPDATE_DRYRUN")) return 1;  /* tests: the script is there to read, nothing is replaced */
    {
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        char run[1200];
        memset(&si, 0, sizeof si);
        si.cb = sizeof si;
        memset(&pi, 0, sizeof pi);
        snprintf(run, sizeof run, "cmd.exe /c \"%s\"", cmd);
        if (!CreateProcessA(NULL, run, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            snprintf(err, errn, "The update script could not be started.");
            return 0;
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    return 1;
}
#endif
