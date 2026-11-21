/* The file-system calls the mod system and the copy checker share. */
#pragma once
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

typedef struct stat stat_t;

#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#ifdef _WIN32
#include <direct.h>
/* mingw's sys/stat.h has both spellings; _mkdir takes no mode. */
#undef mkdir
#define mkdir(p, m) _mkdir(p)
#endif
