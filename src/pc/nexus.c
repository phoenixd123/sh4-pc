/* Nexus Mods API client: the mod list, the main file of one mod, and the download. See nexus.h.
 *
 * Only the official API is used, with the player's own key: https://api.nexusmods.com/v1. The base address can be
 * pointed at a local server with SH4_NEXUS_API for tests, and the key with SH4_NEXUS_KEY.
 */
#include "nexus.h"

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

volatile int nexus_progress;

static char g_key[128];
static char g_base[256] = "https://api.nexusmods.com";

int nexus_set_key(const char *key) {
    const char *env = getenv("SH4_NEXUS_API");
    if (env && *env) snprintf(g_base, sizeof g_base, "%s", env);
    env = getenv("SH4_NEXUS_KEY");
    if (env && *env) key = env;
    snprintf(g_key, sizeof g_key, "%s", key ? key : "");
    size_t n = strlen(g_key);
    while (n && (g_key[n - 1] == ' ' || g_key[n - 1] == '\r' || g_key[n - 1] == '\n' || g_key[n - 1] == '\t'))
        g_key[--n] = 0;
    return g_key[0] != 0;
}

int nexus_have_key(void) { return g_key[0] != 0; }

/* ---- HTTP ------------------------------------------------------------------------------------------- */

typedef struct { char *p; size_t n, cap; } Mem;
typedef struct { FILE *f; long long got, total; } Out;

static void mem_free(Mem *m) { free(m->p); m->p = NULL; m->n = m->cap = 0; }

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

/* One GET. `sink` receives the body as it arrives; NULL means "read and throw away" (used for downloads into
 * a file through `out`). Returns 1 for 200, 0 otherwise with the reason in `err`. */
/* memmem is a GNU extension mingw does not ship. */
static const char *find_sub(const char *hay, size_t hn, const char *needle, size_t nn) {
    if (nn > hn) return NULL;
    for (size_t i = 0; i + nn <= hn; i++)
        if (!memcmp(hay + i, needle, nn)) return hay + i;
    return NULL;
}

static int http_get(const char *url, const char *key, int (*sink)(void *, const char *, DWORD), void *ctx,
                    long long *total, char *err, size_t errn) {
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
    ses = WinHttpOpen(L"SH4-PC", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) goto done;
    WinHttpSetTimeouts(ses, 15000, 15000, 30000, 30000);
    con = WinHttpConnect(ses, host, uc.nPort, 0);
    if (!con) goto done;
    req = WinHttpOpenRequest(con, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                             uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (!req) goto done;
    if (key && *key) {
        wchar_t hdr[256];
        swprintf(hdr, 256, L"apikey: %ls", widen(key));
        WinHttpAddRequestHeaders(req, hdr, (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);
    }
    WinHttpAddRequestHeaders(req, L"Accept: application/json", (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);
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
            if (code == 401) snprintf(err, errn, "Nexus did not accept the API key (401). Check nexus_key= in sh4.ini.");
            else if (code == 403) snprintf(err, errn, "Nexus refused this request (403): the key is valid but the "
                                                      "account cannot do this, usually because automatic downloads "
                                                      "need Premium.");
            else snprintf(err, errn, "Nexus answered %lu.", (unsigned long)code);
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
        if (ctx && total && *total > 0) {
            Out *o = ctx;
            if (o->f) nexus_progress = (int)(o->got * 100 / *total);
        }
    }
    ok = 1;
done:
    if (!ok && err[0] == 0) snprintf(err, errn, "Could not reach Nexus (WinHTTP error %lu).", (unsigned long)GetLastError());
    if (req) WinHttpCloseHandle(req);
    if (con) WinHttpCloseHandle(con);
    if (ses) WinHttpCloseHandle(ses);
    return ok;
}

static int mem_sink(void *ctx, const char *d, DWORD n) { return mem_add((Mem *)ctx, d, n); }
static int file_sink(void *ctx, const char *d, DWORD n) {
    Out *o = ctx;
    if (fwrite(d, 1, n, o->f) != n) return 0;
    o->got += n;
    return 1;
}

/* GET into memory. 1 on success; the caller frees `m`. */
static int get_json(const char *path, Mem *m, char *err, size_t errn) {
    char url[1200];
    snprintf(url, sizeof url, "%s%s", g_base, path);
    memset(m, 0, sizeof *m);
    return http_get(url, g_key, mem_sink, m, NULL, err, errn);
}

/* ---- tiny JSON reader -------------------------------------------------------------------------------- */

/* The value of "key" in obj[0..len): a string with its escapes decoded, or a bare token copied as it is. */
static int jstr(const char *obj, size_t len, const char *key, char *out, size_t n) {
    char pat[64];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    size_t kl = strlen(pat);
    const char *p = obj, *end = obj + len;
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
                    else if (c == 'u') {  /* not needed for names: keep a placeholder rather than a wrong glyph */
                        q += 4;
                        c = '?';
                    }
                    out[o++] = c;
                    q++;
                } else
                    out[o++] = *q++;
            }
        } else {
            while (q < end && *q != ',' && *q != '}' && *q != ']' && o + 1 < n) out[o++] = *q++;
            while (o && (out[o - 1] == ' ' || out[o - 1] == '\r' || out[o - 1] == '\n')) o--;
        }
        out[o] = 0;
        return 1;
    }
    return 0;
}

static long jint(const char *obj, size_t len, const char *key, long def) {
    char v[32];
    if (!jstr(obj, len, key, v, sizeof v) || !v[0]) return def;
    return atol(v);
}

/* The next complete object inside a JSON array, quote-aware so braces in strings do not count. */
static const char *obj_next(const char *p, const char *stop, const char **oend) {
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

/* ---- API --------------------------------------------------------------------------------------------- */

int nexus_account(char *who, size_t n, char *err, size_t errn) {
    Mem m = {0};
    if (!nexus_have_key()) {
        snprintf(err, errn, "No API key yet: put your personal key from\nnexusmods.com/users/myaccount?tab=api "
                            "into `nexus_key=` in sh4.ini.");
        return -1;
    }
    if (who && n) who[0] = 0;
    if (!get_json("/v1/users/validate.json", &m, err, errn)) {
        mem_free(&m);
        return -1;
    }
    char name[64] = "", prem[16] = "";
    jstr(m.p, m.n, "name", name, sizeof name);
    jstr(m.p, m.n, "is_premium", prem, sizeof prem);
    if (who && n) snprintf(who, n, "%s", name);
    mem_free(&m);
    return !strcmp(prem, "true") ? 1 : 0;
}

int nexus_list(NexusMod *out, int max, char *err, size_t errn) {
    Mem m = {0};
    if (!nexus_have_key()) {
        snprintf(err, errn, "No API key yet: put your personal key from\nnexusmods.com/users/myaccount?tab=api "
                            "into `nexus_key=` in sh4.ini.");
        return 0;
    }
    if (!get_json("/v1/games/silenthill4/mods.json", &m, err, errn)) {
        mem_free(&m);
        return 0;
    }
    int n = 0;
    const char *p = m.p, *stop = m.p + m.n, *oe;
    while (n < max && (p = obj_next(p, stop, &oe)) != NULL) {
        NexusMod *d = &out[n];
        long id = jint(p, (size_t)(oe - p), "mod_id", 0);
        if (!id) { p = oe; continue; }
        d->id = (int)id;
        if (!jstr(p, (size_t)(oe - p), "name", d->name, sizeof d->name) || !d->name[0])
            snprintf(d->name, sizeof d->name, "Mod %ld", id);
        if (!jstr(p, (size_t)(oe - p), "author", d->author, sizeof d->author) || !d->author[0])
            jstr(p, (size_t)(oe - p), "uploaded_by", d->author, sizeof d->author);
        n++;
        p = oe;
    }
    mem_free(&m);
    if (!n) {
        snprintf(err, errn, "Nexus returned no mods for Silent Hill 4.");
        return 0;
    }
    return n;
}

/* The newest MAIN zip in files.json: the file a player would download first. */
static long pick_main_file(const char *json, size_t len, char *name, size_t nn) {
    const char *p = json, *stop = json + len, *oe;
    long any_zip = 0;
    char any_name[128] = "";
    while ((p = obj_next(p, stop, &oe)) != NULL) {
        size_t ol = (size_t)(oe - p);
        char cat[48] = "", fn[128] = "";
        long id = jint(p, ol, "file_id", 0);
        jstr(p, ol, "category_name", cat, sizeof cat);
        jstr(p, ol, "file_name", fn, sizeof fn);
        int is_zip = 0;
        for (size_t i = strlen(fn); i > 3; i--)
            if (fn[i - 4] == '.' && (!strncasecmp(fn + i - 3, "zip", 3))) { is_zip = 1; break; }
        if (id && is_zip) {
            if (!strcasecmp(cat, "MAIN") || !strcasecmp(cat, "MAIN FILES")) {
                snprintf(name, nn, "%s", fn);
                return id;
            }
            if (!any_zip) { any_zip = id; snprintf(any_name, sizeof any_name, "%s", fn); }
        }
        p = oe;
    }
    if (any_zip) snprintf(name, nn, "%s", any_name);
    return any_zip;
}

int nexus_fetch_main(int mod_id, const char *dest_zip, char *err, size_t errn) {
    char path[256], file_name[160] = "";
    Mem m = {0};
    nexus_progress = 0;
    if (!nexus_have_key()) {
        snprintf(err, errn, "No API key yet: put your personal key from\nnexusmods.com/users/myaccount?tab=api "
                            "into `nexus_key=` in sh4.ini.");
        return 0;
    }
    snprintf(path, sizeof path, "/v1/games/silenthill4/mods/%d/files.json", mod_id);
    if (!get_json(path, &m, err, errn)) { mem_free(&m); return 0; }
    long fid = pick_main_file(m.p, m.n, file_name, sizeof file_name);
    mem_free(&m);
    if (!fid) {
        snprintf(err, errn, "That mod has no .zip file to install. Its page has the file list.");
        return 0;
    }
    snprintf(path, sizeof path, "/v1/games/silenthill4/mods/%d/files/%ld/download_link.json", mod_id, fid);
    if (!get_json(path, &m, err, errn)) { mem_free(&m); return 0; }
    char uri[1024] = "";
    jstr(m.p, m.n, "URI", uri, sizeof uri);
    mem_free(&m);
    if (!uri[0]) {
        snprintf(err, errn, "Nexus did not return a download address for that file.");
        return 0;
    }
    {
        Out o;
        FILE *f = fopen(dest_zip, "wb");
        if (!f) {
            snprintf(err, errn, "The download could not be saved to\n\n%s", dest_zip);
            return 0;
        }
        memset(&o, 0, sizeof o);
        o.f = f;
        long long total = 0;
        int ok = http_get(uri, NULL, file_sink, &o, &total, err, errn);
        fclose(f);
        if (!ok || o.got <= 0) {
            DeleteFileA(dest_zip);
            if (err[0] == 0) snprintf(err, errn, "The download was empty.");
            return 0;
        }
        nexus_progress = 100;
    }
    return 1;
}

void nexus_page_url(int mod_id, char *out, size_t n) {
    snprintf(out, n, "https://www.nexusmods.com/silenthill4/mods/%d", mod_id);
}
#endif
