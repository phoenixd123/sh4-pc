/* The mod system without a game: the path mapping, the load order and the installer, against a folder built here.
 * tools/test-mods.sh makes the fixtures and runs this. */
#include "../src/mods/mods.h"
#include "../src/mods/zip.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/fs.h"

void rt_log(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}

static int failures;
static void check(int ok, const char *what) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

static void mkdirs(const char *p) {
    char tmp[600];
    snprintf(tmp, sizeof tmp, "%s", p);
    for (char *s = tmp + 1; *s; s++)
        if (*s == '/') {
            *s = 0;
            mkdir(tmp, 0755);
            *s = '/';
        }
    mkdir(tmp, 0755);
}

static void put(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        printf("cannot write %s\n", path);
        exit(2);
    }
    fputs(text, f);
    fclose(f);
}

int main(void) {
    char out[600];
    if (chdir("build/modtest")) {
        printf("run through tools/test-mods.sh\n");
        return 2;
    }

    mkdirs("mods/first/files/data/tex");
    mkdirs("mods/second/files/data/tex");
    mkdirs("mods/disabled/files/data");
    mkdirs("mods/notamod");
    put("mods/first/files/data/tex/wall.sh4", "first");
    put("mods/second/files/data/tex/wall.sh4", "second");
    put("mods/first/files/data/only-first.bin", "x");
    put("mods/second/mod.ini", "name=Second mod\npriority=5\n");
    put("mods/disabled/mod.ini", "enabled=0\n");
    put("mods/disabled/files/data/disabled.bin", "x");

    mods_init();
    check(mods_count() == 3, "three mods found; a folder with neither files/ nor mod.ini ignored");
    check(mods_override("data/tex/wall.sh4", out, sizeof out) != NULL, "a mod file is found");
    check(!strcmp(out, "mods/second/files/data/tex/wall.sh4"), "priority decides between unlisted mods");
    check(mods_override("data/only-first.bin", out, sizeof out) != NULL, "a file only one mod has");
    check(mods_override("data/disabled.bin", out, sizeof out) == NULL, "a disabled mod is skipped");
    check(mods_override("data/absent.bin", out, sizeof out) == NULL, "a file no mod has stays the game's");

    put("mods/load_order.txt", "second\nfirst\n");
    mods_init();
    check(mods_override("data/tex/wall.sh4", out, sizeof out) != NULL && strstr(out, "mods/second/") != NULL,
          "load_order.txt puts second first");

    {
        char name[128], err[600] = "";
        int ok = mods_install("fixture-mod.zip", name, sizeof name, err, sizeof err);
        check(ok, ok ? "install a wrapped archive" : err);
        if (ok) check(!strcmp(name, "fixture-mod"), "the mod is named after the archive");
        mods_init();   /* the launcher re-reads mods/ after installing, which is the contract */
        check(ok && mods_override("data/inside.bin", out, sizeof out) != NULL, "the installed mod's files are live");
        check(!mods_install("fixture-mod.zip", name, sizeof name, err, sizeof err), "a taken name is refused");
    }
    {
        char name[128], err[600] = "";
        int ok = mods_install("fixture-loose.zip", name, sizeof name, err, sizeof err);
        check(ok, ok ? "install an archive of loose game paths" : err);
        mods_init();
        check(ok && mods_override("data/loose.bin", out, sizeof out) != NULL, "loose paths are wrapped in files/");
    }
    {
        char err[600] = "";
        check(zip_archive_has_game("fixture-game.zip"), "a game download is recognised by its file list");
        check(!zip_archive_has_game("fixture-mod.zip"), "a mod archive is not one");
        mkdir("game-extract", 0755);
        int n = zip_extract_file("fixture-game.zip", "game-extract", err, sizeof err);
        check(n == 2, n == 2 ? "a game download is streamed out of the archive" : err);
        FILE *f = fopen("game-extract/data/room.bin", "rb");
        check(f != NULL, "the streamed files are where the archive put them");
        if (f) fclose(f);
    }
    printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
