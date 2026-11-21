/* Nexus Mods, from inside the launcher: the list of Silent Hill 4 mods, and a download into mods/.
 *
 * This talks to the official API (api.nexusmods.com) with the player's own personal API key, which the launcher
 * reads from `nexus_key=` in sh4.ini. Nothing is scraped: the site's terms forbid it, and the API is the only
 * supported way in.
 *
 * Automatic downloads are a Premium feature of the API. A free key can list mods and open their page, but
 * download_link.json answers 403 unless the file has direct downloads enabled, so `nexus_fetch_main` says so
 * instead of trying to work around it.
 *
 * Windows only: the launcher is a Windows program and this uses WinHTTP. */
#pragma once
#include <stddef.h>

#ifdef _WIN32
#define NEXUS_MAX_MODS 80

typedef struct {
    int id;
    char name[110];
    char author[56];
} NexusMod;

/* The key from sh4.ini (or SH4_NEXUS_KEY in the environment, for tests). 1 when there is one. */
int nexus_set_key(const char *key);
int nexus_have_key(void);

/* 1 premium (automatic downloads work), 0 free, -1 could not tell. `who` gets the account name. */
int nexus_account(char *who, size_t n, char *err, size_t errn);

/* Fills up to max mods, newest first as the API returns them. 1 on success. */
int nexus_list(NexusMod *out, int max, char *err, size_t errn);

/* Downloads the mod's main file (the first MAIN .zip) to dest_zip. 1 on success. `progress` is 0..100. */
extern volatile int nexus_progress;
int nexus_fetch_main(int mod_id, const char *dest_zip, char *err, size_t errn);

/* https://www.nexusmods.com/silenthill4/mods/<id> */
void nexus_page_url(int mod_id, char *out, size_t n);
#endif
