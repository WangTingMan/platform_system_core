#pragma once
#include <stdint.h>
#include <utils/utils_export.h>

#ifdef __cplusplus
#include <string>
#endif

#ifdef _MSC_VER
#include <sys\stat.h>
#endif

#if defined( __LP64__ )
#define __DIRENT64_INO_T ino_t
#else
#define __DIRENT64_INO_T uint64_t /* Historical accident. */
#endif

#ifndef DT_UNKNOWN
#define DT_UNKNOWN 0
#define DT_FIFO S_IFIFO
#define DT_CHR S_IFCHR
#define DT_DIR S_IFDIR
#define DT_BLK S_IFBLK
#define DT_REG S_IFREG
#define DT_LNK S_IFLNK
#define DT_SOCK S_IFSOCK
#define DT_WHT 14
#endif

#if !defined(PATH_MAX)
#define PATH_MAX 260
#endif

/* Multi-byte character version */
struct dirent
{
    /* Always zero */
    long d_ino;

    /* File position within stream */
    long d_off;

    /* Structure size */
    unsigned short d_reclen;

    /* Length of name without \0 */
    size_t d_namlen;

    /* File type */
    int d_type;

    /* File name */
    char d_name[PATH_MAX + 1];
};

typedef struct dirent dirent;

#ifndef mode_t
#define mode_t int
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DIR DIR;

UTILS_EXPORT DIR* opendir( const char* );
UTILS_EXPORT int closedir( DIR* );
UTILS_EXPORT struct dirent* readdir( DIR* );
UTILS_EXPORT void rewinddir( DIR* );
UTILS_EXPORT int scandir(
    const char* dirname,
    struct dirent*** namelist,
    int ( *filter )( const struct dirent* ),
    int ( *compare )( const struct dirent**, const struct dirent** ) );
UTILS_EXPORT int lstat( const char*, struct stat*);
#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
UTILS_EXPORT std::string GetCurrentDir();
UTILS_EXPORT int mkdir( const char* path, mode_t mode );
UTILS_EXPORT DIR* opendir( const wchar_t* );
#endif

