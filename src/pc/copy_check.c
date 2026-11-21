/* What a picked path is. The port recompiles one executable, so a folder is a playable copy only when it holds the
 * GOG 1.0 gog v5 SILENT HILL 4.exe; anything else is named instead of started. Windows dialogs and the hand-over to
 * a copy's own game live here too, so the launcher itself stays free of the shell's API. */
#include "copy_check.h"
#include "rt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifndef _WIN32
#include <strings.h>
#define strcasecmp SDL_strcasecmp
#endif
#include "../fs.h"
#include "../mods/zip.h"
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <SDL.h>
#endif

/* The release the recompilation is written against (sh4vita pins the same one). */
#define GOG_V5_SHA256 "cf7081a6c78dbe51f6dc2e1c9bd978d8486f1d7b59d7a3b88e027c9b39f21dd0"

static int is_dir(const char *p) {
    struct stat st;
    return p && stat(p, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR;
}
static int is_file(const char *p) {
    struct stat st;
    return p && stat(p, &st) == 0 && (st.st_mode & S_IFMT) != S_IFDIR;
}

static const char *base_name(const char *p) {
    const char *b = p;
    for (const char *s = p; *s; s++)
        if (*s == '/' || *s == '\\') b = s + 1;
    return b;
}

/* The folder's copy of a name, whatever its case: Windows installs write "SILENT HILL 4.exe", and a rip may not. */
static int find_in(const char *dir, const char *name, char *out, size_t n) {
    DIR *d = opendir(dir);
    struct dirent *e;
    int found = 0;
    if (!d) return 0;
    while ((e = readdir(d)))
        if (!strcasecmp(e->d_name, name)) {
            snprintf(out, n, "%s", e->d_name);
            found = 1;
            break;
        }
    closedir(d);
    return found;
}

/* ---- SHA-256 (FIPS 180-4): the executable is identified by its hash, not by its size ---- */

typedef struct { uint32_t h[8]; uint64_t len; uint8_t buf[64]; size_t n; } Sha256;

static const uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static uint32_t ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static void sha256_block(Sha256 *s, const uint8_t *p) {
    uint32_t w[64], a, b, c, d, e, f, g, h;
    for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3], e = s->h[4], f = s->h[5], g = s->h[6], h = s->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ror(e, 6) ^ ror(e, 11) ^ ror(e, 25), ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + K256[i] + w[i];
        uint32_t S0 = ror(a, 2) ^ ror(a, 13) ^ ror(a, 22), maj = (a & b) ^ (a & c) ^ (b & c);
        h = g, g = f, f = e, e = d + t1, d = c, c = b, b = a, a = t1 + S0 + maj;
    }
    s->h[0] += a, s->h[1] += b, s->h[2] += c, s->h[3] += d;
    s->h[4] += e, s->h[5] += f, s->h[6] += g, s->h[7] += h;
}

static void sha256_init(Sha256 *s) {
    static const uint32_t iv[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(s->h, iv, sizeof iv);
    s->len = 0;
    s->n = 0;
}

static void sha256_update(Sha256 *s, const uint8_t *p, size_t n) {
    s->len += (uint64_t)n << 3;
    while (n) {
        size_t take = 64 - s->n;
        if (take > n) take = n;
        memcpy(s->buf + s->n, p, take);
        s->n += take, p += take, n -= take;
        if (s->n == 64) sha256_block(s, s->buf), s->n = 0;
    }
}

static void sha256_final(Sha256 *s, uint8_t out[32]) {
    uint64_t bits = s->len;
    uint8_t pad = 0x80;
    sha256_update(s, &pad, 1);
    pad = 0;
    while (s->n != 56) sha256_update(s, &pad, 1);
    uint8_t lenbuf[8];
    for (int i = 0; i < 8; i++) lenbuf[i] = (uint8_t)(bits >> (56 - 8 * i));
    sha256_update(s, lenbuf, 8);
    for (int i = 0; i < 8; i++) {
        out[4 * i] = (uint8_t)(s->h[i] >> 24), out[4 * i + 1] = (uint8_t)(s->h[i] >> 16);
        out[4 * i + 2] = (uint8_t)(s->h[i] >> 8), out[4 * i + 3] = (uint8_t)s->h[i];
    }
}

void sh4_sha256_file(const char *path, char hex[65]) {
    Sha256 s;
    uint8_t buf[64 * 1024], sum[32];
    FILE *f = fopen(path, "rb");
    hex[0] = 0;
    if (!f) return;
    sha256_init(&s);
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) sha256_update(&s, buf, n);
    fclose(f);
    sha256_final(&s, sum);
    for (int i = 0; i < 32; i++) snprintf(hex + 2 * i, 3, "%02x", sum[i]);
}

/* A disc image: ISO 9660 names its boot program in SYSTEM.CNF. Reading the first megabyte is enough to tell a
 * console disc from anything else, and naming the program is what makes the refusal useful. */
static int disc_program(const char *path, char *out, size_t n) {
    FILE *f = fopen(path, "rb");
    uint8_t *buf;
    long size;
    out[0] = 0;
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0 || size > (800L << 20)) {
        fclose(f);
        return 0;
    }
    buf = malloc((size_t)size);
    if (!buf) {
        fclose(f);
        return 0;
    }
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
        free(buf);
        fclose(f);
        return 0;
    }
    fclose(f);
    for (long i = 0; i + 10 < size && i < (1L << 20); i++)
        if (!memcmp(buf + i, "SLUS_", 5) || !memcmp(buf + i, "SLES_", 5) || !memcmp(buf + i, "SLPM_", 5)) {
            size_t k = 0;
            while (k < 10 && k + 1 < n && buf[i + k] >= 32 && buf[i + k] < 127) out[k] = (char)buf[i + k], k++;
            out[k] = 0;
            break;
        }
    free(buf);
    return out[0] != 0;
}

int sh4_check_copy(const char *path, char *dir, size_t dn, char *msg, size_t mn) {
    char exe[700];
    if (msg && mn) msg[0] = 0;
    if (dir && dn) dir[0] = 0;
    if (!path || !*path) return SH4_COPY_NONE;

    if (is_dir(path)) {
        char name[260], full[700], data[700];
        if (!find_in(path, "SILENT HILL 4.exe", name, sizeof name)) {
            if (msg) snprintf(msg, mn, "There is no SILENT HILL 4.exe in that folder.\n\nPick the folder that "
                                       "holds the executable beside data/, demo/, movie/ and sound/.");
            return SH4_COPY_NONE;
        }
        snprintf(full, sizeof full, "%s/%s", path, name);
        snprintf(data, sizeof data, "%s/data", path);
        if (!is_dir(data)) {
            if (msg) snprintf(msg, mn, "%s is there, but data/ is not.\n\nPick the installation folder itself, "
                                       "not the folder above it.", name);
            return SH4_COPY_NONE;
        }
        sh4_sha256_file(full, exe);
        if (dir) snprintf(dir, dn, "%s", path);
        if (strcasecmp(exe, GOG_V5_SHA256)) {
            if (msg) snprintf(msg, mn, "This is Silent Hill 4, but not the release this port recompiles.\n\n"
                                       "SILENT HILL 4.exe has SHA-256\n%s\n\n"
                                       "The port needs the GOG version, 1.0 gog v5:\n%s", exe, GOG_V5_SHA256);
            return SH4_COPY_WRONG_EXE;
        }
        if (msg) snprintf(msg, mn, "Silent Hill 4: The Room, GOG 1.0 gog v5.\n\nSILENT HILL 4.exe matches the "
                                   "executable the port was written against.");
        return SH4_COPY_GAME;
    }

    if (!is_file(path)) return SH4_COPY_NONE;
    {
        const char *b = base_name(path), *dot = strrchr(b, '.');
        if (dot && !strcasecmp(dot, ".exe")) {
            char parent[700];
            size_t k = (size_t)(b - path);
            if (k) {
                snprintf(parent, sizeof parent, "%.*s", (int)(k - (k && (path[k - 1] == '/' || path[k - 1] == '\\'))), path);
                return sh4_check_copy(parent, dir, dn, msg, mn);
            }
        }
        if (dot && (!strcasecmp(dot, ".zip"))) {
            if (zip_archive_has_game(path)) {
                if (msg) snprintf(msg, mn, "That zip is a pre-installed copy of Silent Hill 4: it holds "
                                           "SILENT HILL 4.exe and data/.\n\nOptions > Import a .zip download "
                                           "unpacks it beside itself and makes the folder inside the game.");
                return SH4_COPY_DOWNLOAD;
            }
            if (msg) snprintf(msg, mn, "That is a .zip with no game in it. If it is a mod, Options > Install a mod "
                                       "puts it in mods/.");
            return SH4_COPY_MOD;
        }
        if (dot && (!strcasecmp(dot, ".iso") || !strcasecmp(dot, ".bin") || !strcasecmp(dot, ".img") ||
                    !strcasecmp(dot, ".cue"))) {
            char prog[16];
            if (dot && !strcasecmp(dot, ".cue")) {
                if (msg) snprintf(msg, mn, "That is a .cue sheet.\n\nThe port reads the PC release: point it at the "
                                           "folder holding SILENT HILL 4.exe instead. The PlayStation 2 disc it names "
                                           "is MIPS code with the console's own data formats.");
                return SH4_COPY_PS2;
            }
            if (disc_program(path, prog, sizeof prog))
                snprintf(msg, mn, "That is the PlayStation 2 disc: it starts %s.\n\nThe port recompiles the GOG PC "
                                  "release, whose executable is x86 code; a PS2 disc cannot run here.", prog);
            else
                snprintf(msg, mn, "That disc does not look like the PC release.\n\nThe port recompiles the GOG 1.0 gog "
                                  "v5 SILENT HILL 4.exe; install that, then pick its folder.");
            return SH4_COPY_PS2;
        }
    }
    if (msg) snprintf(msg, mn, "That is not Silent Hill 4.\n\nPick the folder that holds SILENT HILL 4.exe, or "
                               "a mod archive (.zip).");
    return SH4_COPY_NONE;
}

/* ---- a game download, unpacked beside itself --------------------------------------------------------- */

static int rm_tree(const char *path) {
    struct stat st;
    if (!path || !*path || stat(path, &st)) return 0;
    if ((st.st_mode & S_IFMT) != S_IFDIR) return unlink(path) == 0;
    {
        DIR *d = opendir(path);
        struct dirent *e;
        if (d) {
            while ((e = readdir(d))) {
                char child[700];
                if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
                snprintf(child, sizeof child, "%s/%s", path, e->d_name);
                rm_tree(child);
            }
            closedir(d);
        }
    }
    return rmdir(path) == 0;
}

/* A folder is the installation when SILENT HILL 4.exe and data/ are directly in it. */
static int game_here(const char *dir) {
    char exe[260], data[260], full[700];
    if (!find_in(dir, "SILENT HILL 4.exe", exe, sizeof exe)) return 0;
    if (!find_in(dir, "data", data, sizeof data)) return 0;
    snprintf(full, sizeof full, "%s/%s", dir, data);
    return is_dir(full);
}

/* The installation may sit inside the archive's own wrapper folder; two levels is what rips use. */
static int find_game_below(const char *root, int depth, char *out, size_t n) {
    DIR *d;
    struct dirent *e;
    if (game_here(root)) {
        snprintf(out, n, "%s", root);
        return 1;
    }
    if (depth <= 0) return 0;
    if (!(d = opendir(root))) return 0;
    while ((e = readdir(d))) {
        char child[700];
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        snprintf(child, sizeof child, "%s/%s", root, e->d_name);
        if (find_game_below(child, depth - 1, out, n)) {
            closedir(d);
            return 1;
        }
    }
    closedir(d);
    return 0;
}

int sh4_import_zip(const char *zip, char *dir, size_t dn, char *msg, size_t mn) {
    char stem[260] = "", dest[700], found[700], err[700] = "";
    const char *base = base_name(zip), *dot;
    size_t prefix, k = 0;
    if (dir && dn) dir[0] = 0;
    if (msg && mn) msg[0] = 0;
    if (!zip_archive_has_game(zip)) {
        if (msg) snprintf(msg, mn, "That archive has no SILENT HILL 4.exe and data/ in it, so it is not a game "
                                   "download. A mod archive is installed with Options > Install a mod.");
        return 0;
    }
    dot = strrchr(base, '.');
    for (const char *p = base; *p && (!dot || p < dot) && k + 1 < sizeof stem; p++) {
        char c = *p;
        if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*')
            c = '_';
        stem[k++] = c;
    }
    stem[k] = 0;
    if (!stem[0]) snprintf(stem, sizeof stem, "Silent Hill 4");
    prefix = (size_t)(base - zip);
    snprintf(dest, sizeof dest, "%.*s%s", (int)prefix, zip, stem);
    if (!game_here(dest)) {
        mkdir(dest, 0755);
        if (zip_extract_file(zip, dest, err, sizeof err) < 0) {
            rm_tree(dest);
            if (msg) snprintf(msg, mn, "%s", err);
            return 0;
        }
    }
    if (!find_game_below(dest, 2, found, sizeof found)) {
        if (msg) snprintf(msg, mn, "The archive was unpacked to\n\n%s\n\nbut no folder in it holds "
                                   "SILENT HILL 4.exe beside data/.", dest);
        return 0;
    }
    if (dir) snprintf(dir, dn, "%s", found);
    if (msg) snprintf(msg, mn, "Imported the download into\n\n%s\n\nThe game code still has to be built from "
                               "this copy; the launcher-only build plays its own executable.", found);
    return 1;
}

#ifdef _WIN32
int sh4_start_original(const char *dir) {
    char exe[700], name[260];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    if (!dir || !*dir || !find_in(dir, "SILENT HILL 4.exe", name, sizeof name)) return 0;
    snprintf(exe, sizeof exe, "%s/%s", dir, name);
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    if (!CreateProcessA(exe, NULL, NULL, NULL, FALSE, 0, NULL, dir, &si, &pi)) return 0;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    rt_log("launcher: started the copy's own %s", exe);
    return 1;
}

/* ---- PCSX2: where the PlayStation 2 release is actually playable -------------------------------------- */

static int pcsx2_path(char *out, size_t n) {
    static const char *const rel[] = {"Programs\\PCSX2\\pcsx2-qt.exe", "PCSX2\\pcsx2-qt.exe",
                                      "PCSX2\\pcsx2.exe"};
    static const char *const envs[] = {"LOCALAPPDATA", "ProgramFiles", "ProgramFiles(x86)"};
    for (size_t e = 0; e < sizeof envs / sizeof *envs; e++) {
        const char *root = getenv(envs[e]);
        if (!root || !*root) continue;
        for (size_t r = 0; r < sizeof rel / sizeof *rel; r++) {
            snprintf(out, n, "%s\\%s", root, rel[r]);
            if (is_file(out)) return 1;
        }
    }
    {
        char found[700];
        if (SearchPathA(NULL, "pcsx2-qt.exe", NULL, (DWORD)sizeof found, found, NULL) && is_file(found)) {
            snprintf(out, n, "%s", found);
            return 1;
        }
        if (SearchPathA(NULL, "pcsx2.exe", NULL, (DWORD)sizeof found, found, NULL) && is_file(found)) {
            snprintf(out, n, "%s", found);
            return 1;
        }
    }
    out[0] = 0;
    return 0;
}

int sh4_pcsx2_found(void) {
    char exe[700];
    return pcsx2_path(exe, sizeof exe);
}

int sh4_open_ps2(const char *path) {
    char exe[700], cmd[1500];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    if (!pcsx2_path(exe, sizeof exe)) return 0;
    snprintf(cmd, sizeof cmd, "\"%s\" \"%s\"", exe, path);
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    memset(&pi, 0, sizeof pi);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return 0;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    rt_log("launcher: opened the PlayStation 2 disc in %s", exe);
    return 1;
}
#else
int sh4_start_original(const char *dir) { (void)dir; return 0; }
int sh4_pcsx2_found(void) { return 0; }
int sh4_open_ps2(const char *path) { (void)path; return 0; }
#endif

/* ---- the shell's dialogs ---- */

#ifdef _WIN32
static void wide_from(const char *in, wchar_t *out, size_t n) { MultiByteToWideChar(CP_UTF8, 0, in, -1, out, (int)n); }
static void narrow_from(const wchar_t *in, char *out, size_t n) { WideCharToMultiByte(CP_UTF8, 0, in, -1, out, (int)n, NULL, NULL); }

int sh4_pick_folder(const char *start, char *out, size_t n) {
    wchar_t wstart[700], wout[700];
    BROWSEINFOW bi;
    LPITEMIDLIST id;
    int got = 0;
    out[0] = 0;
    CoInitialize(NULL);
    wide_from(start && *start ? start : "", wstart, 700);
    memset(&bi, 0, sizeof bi);
    bi.hwndOwner = NULL;
    bi.pszDisplayName = wout;
    bi.lpszTitle = L"Pick the folder that holds SILENT HILL 4.exe";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpfn = NULL;
    id = SHBrowseForFolderW(&bi);
    if (id && SHGetPathFromIDListW(id, wout)) {
        narrow_from(wout, out, n);
        got = 1;
    }
    if (id) CoTaskMemFree(id);
    CoUninitialize();
    return got;
}

static int open_dialog(const char *title, const char *filter, char *out, size_t n) {
    OPENFILENAMEA ofn;
    char file[700] = "";
    out[0] = 0;
    memset(&ofn, 0, sizeof ofn);
    ofn.lStructSize = sizeof ofn;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = file;
    ofn.nMaxFile = sizeof file;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameA(&ofn)) return 0;
    snprintf(out, n, "%s", file);
    return 1;
}

int sh4_pick_game_file(const char *start, char *out, size_t n) {
    (void)start;
    return open_dialog("SILENT HILL 4.exe, or a disc image",
                       "Silent Hill 4 (SILENT HILL 4.exe)\0*.exe\0Disc images (*.iso;*.bin;*.cue)\0*.iso;*.bin;*.cue\0"
                       "All files\0*.*\0\0", out, n);
}

int sh4_pick_game_archive(const char *start, char *out, size_t n) {
    (void)start;
    return open_dialog("A pre-installed game download (.zip)", "Game downloads (*.zip)\0*.zip\0All files\0*.*\0\0",
                       out, n);
}

int sh4_pick_archive(const char *start, char *out, size_t n) {
    (void)start;
    return open_dialog("A mod archive", "Mod archives (*.zip)\0*.zip\0All files\0*.*\0\0", out, n);
}

int sh4_confirm(const char *title, const char *text) {
    return MessageBoxA(NULL, text, title, MB_ICONQUESTION | MB_YESNO) == IDYES;
}
#else
int sh4_pick_folder(const char *start, char *out, size_t n) { (void)start; (void)out; (void)n; return 0; }
int sh4_pick_game_file(const char *start, char *out, size_t n) { (void)start; (void)out; (void)n; return 0; }
int sh4_pick_game_archive(const char *start, char *out, size_t n) { (void)start; (void)out; (void)n; return 0; }
int sh4_pick_archive(const char *start, char *out, size_t n) { (void)start; (void)out; (void)n; return 0; }
int sh4_confirm(const char *title, const char *text) { (void)title; (void)text; return 0; }
#endif
