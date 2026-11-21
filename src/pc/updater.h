/* Updating the launcher from its own GitHub releases. See updater.c.
 *
 * The check asks github.com for the newest release of phoenixd123/sh4-pc, which needs no key and no account, and
 * compares its tag with the version this binary was built as. A newer one is downloaded and the files in the
 * release's zip (the launcher, SDL2.dll, README.txt) are put in place: the running executable cannot be replaced
 * from inside itself on Windows, so a one-shot script waits for this process to go, copies them and starts the
 * launcher again.
 *
 * Windows only: the launcher is a Windows program and this uses WinHTTP and miniz. */
#pragma once
#include <stddef.h>

#ifndef PORT_VERSION
#define PORT_VERSION "0.2.0"
#endif

#define UPDATER_REPO "phoenixd123/sh4-pc"

#ifdef _WIN32
typedef struct {
    char tag[40];      /* the release's tag, e.g. "v1.11.0" */
    char version[40];  /* the same without the v */
    char name[140];    /* the release's title */
    char url[700];     /* the win64 launcher asset's download address */
    long long size;
} UpdaterRelease;

/* The version this launcher was built as ("1.11.0"). */
const char *updater_version(void);

/* 1 when a is a later version than b (either may carry a leading "v"). */
int updater_version_newer(const char *a, const char *b);

/* Asks GitHub for the newest release. 1 when it is newer than this build, 0 when this build is current, -1 when
 * the answer could not be had (no network, rate limit, no release yet), with a sentence for the player in err.
 * SH4_UPDATE_API points the request at another address, for tests. */
int updater_check(UpdaterRelease *r, char *err, size_t errn);

/* 0..100 while updater_download runs. */
extern volatile int updater_progress;

/* Downloads url into a file in %TEMP%, whose path is copied to zip. 1 on success. */
int updater_download(const char *url, char *zip, size_t n, char *err, size_t errn);

/* Unpacks the release's files into %TEMP%\sh4-pc-update, writes the script that puts them beside this executable
 * and starts this executable again, and runs that script once this process has exited. Returns 1 when it did,
 * so the caller exits; 0 with a sentence in err. SH4_UPDATE_DRYRUN=1 writes the script but does not run it. */
int updater_install(const char *zip, char *err, size_t errn);

/* 1 when the update can be applied at all (this build came from the release layout, so the files it replaces
 * are next to its executable). A build from source says no rather than overwriting somebody's tree. */
int updater_can_install(void);
#endif
