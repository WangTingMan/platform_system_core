/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#define NOMINMAX

#include <sys/types.h>

#if defined(__GLIBC__) || defined(_WIN32)

#include <string.h>
#include <string>
#include <cutils/memory.h>
#include <cstdlib>
#include <cstdarg>
#include <algorithm>

#include <windows.h>
#include <corecrt_io.h>

#ifdef __cplusplus
extern "C" {
#endif

char* strsep( char** stringp, const char* delim )
{
    char* s;
    const char* spanp;
    int c, sc;
    char* tok;

    if( ( s = *stringp ) == NULL )
        return ( NULL );
    for( tok = s;;)
    {
        c = *s++;
        spanp = delim;
        do
        {
            if( ( sc = *spanp++ ) == c )
            {
                if( c == 0 )
                    s = NULL;
                else
                    s[-1] = 0;
                *stringp = s;
                return ( tok );
            }
        } while( sc != 0 );
    }
    /* NOTREACHED */
    return NULL;
}

char* strndup(const char* str, size_t maxlen)
{
    char* copy;
    size_t len;

    len = strnlen(str, maxlen);
    copy = (char*)malloc(len + 1);
    if (copy != NULL) {
        (void)memcpy(copy, str, len);
        copy[len] = '\0';
    }

    return copy;
}

/* Implementation of strlcpy() for platforms that don't already have it. */

/*
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
size_t
strlcpy(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0) {
		while (--n != 0) {
			if ((*d++ = *s++) == '\0')
				break;
		}
  }

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}

char* strtok_r(char* str, const char* delim, char** saveptr)
{
    return strtok_s(str, delim, saveptr);
}

size_t strlcat(char* dest, const char* src, size_t destsz)
{
	std::string dest_str(dest);
	dest_str.append(src);
	return strlcpy(dest, dest_str.c_str(), destsz);
}

void bzero( void* ptr, size_t a_size)
{
    memset( ptr, 0x00, a_size );
}

char* strcasestr( const char* s, const char* find )
{
    char c, sc;
    size_t len;

    if( ( c = *find++ ) != 0 )
    {
        c = (char)tolower( (unsigned char)c );
        len = strlen( find );
        do
        {
            do
            {
                if( ( sc = *s++ ) == 0 )
                    return ( NULL );
            } while( (char)tolower( (unsigned char)sc ) != c );
        } while( _strnicmp( s, find, len ) != 0 );
        s--;
    }
    return ( (char*)s );
}

int __dump_to_file_descriptor
    (
    int fd,
    const char* fmt,
    ...
    )
{
#define BUFFER_SIZE 2048

    char buf[BUFFER_SIZE] = { 0x00 };
    if( fmt )
    {
        va_list vaList;
        va_start( vaList, fmt );
        vsnprintf( buf, BUFFER_SIZE - 1, fmt, vaList );
        va_end( vaList );
    }
    return 0;
}

int util_asprintf(char** str, const char* fmt, ...)
{
    int ret;
    va_list args;
    va_start(args, fmt);
    ret = vasprintf(str, fmt, args);
    va_end(args);
    return ret;
}

int util_vasprintf(char** ret, const char* format, va_list ap)
{
    va_list ap_copy;

    /* Compute length of output string first */
    va_copy(ap_copy, ap);
    int r = vsnprintf(NULL, 0, format, ap_copy);
    va_end(ap_copy);

    if (r < 0)
        return -1;

    *ret = (char*)malloc(r + 1);
    if (!*ret)
        return -1;

    /* Print to buffer */
    return vsnprintf(*ret, r + 1, format, ap);
}

static std::string program_name;
void setprogname( const char* p )
{
    if( !p ) p = "(unknown)";
    program_name.assign( p );
}

const char* getprogname( void )
{
    return program_name.c_str();
}

int setenv( const char* name, const char* value, int overwrite )
{
    if( !name || !value )
    {
        errno = EINVAL;
        return -1;
    }

    if( !overwrite && getenv( name ) )
        return 0;
    int err = _putenv_s( name, value );
    if( err != 0 )
    {
        errno = err;
        return -1;
    }
    return 0;
}

/*
 * 返回值：0 成功，-1 失败并设置 errno
 * mode = 0 ：稀疏预占（最常用，无需特权）
 * mode = 1 ：立即占物理簇（需管理员，性能最高）
 */
inline int win_fallocate( int fd, int64_t offset, int64_t len, int mode = 0 )
{
    if( offset < 0 || len <= 0 )
    {
        errno = EINVAL;
        return -1;
    }

    HANDLE h = reinterpret_cast< HANDLE >( _get_osfhandle( fd ) );
    if( h == INVALID_HANDLE_VALUE )
    {
        errno = EBADF;
        return -1;
    }

    /* 一、稀疏打洞 (PUNCH_HOLE) ---------------------------- */
    if( mode & FALLOC_FL_PUNCH_HOLE )
    {
        FILE_ZERO_DATA_INFORMATION zdi;
        zdi.FileOffset.QuadPart = offset;
        zdi.BeyondFinalZero.QuadPart = offset + len;

        if( !DeviceIoControl( h, FSCTL_SET_ZERO_DATA,
            &zdi, sizeof( zdi ), nullptr, 0,
            nullptr, nullptr ) )
        {
            errno = ( GetLastError() == ERROR_INVALID_FUNCTION ||
                GetLastError() == ERROR_NOT_SUPPORTED )
                ? EOPNOTSUPP : EIO;
            return -1;
        }
        /* Windows 打洞会扩张文件；若需要 KEEP_SIZE 再截回 */
        if( mode & FALLOC_FL_KEEP_SIZE )
        {
            FILE_END_OF_FILE_INFO eofi;
            eofi.EndOfFile.QuadPart = offset; // 保守：截到原来 offset
            SetFileInformationByHandle( h, FileEndOfFileInfo,
                &eofi, sizeof( eofi ) );
        }
        return 0;
    }

    /* 二、纯 KEEP_SIZE 无 PUNCH -> 暂无底层支持 ------------- */
    if( mode & FALLOC_FL_KEEP_SIZE )
    {
        errno = EOPNOTSUPP;
        return -1;
    }

    /* 三、普通预分配 (mode == 0) -----------------------------
     * 策略：先 SetEndOfFile 到 offset+len，再按需 SetFileValidData
     *       做“物理占簇”加速；失败则退化为稀疏文件。
     */
    LARGE_INTEGER off;
    off.QuadPart = offset + len;
    if( !SetFilePointerEx( h, off, nullptr, FILE_BEGIN ) ||
        !SetEndOfFile( h ) )
    {
        errno = EIO;
        return -1;
    }

    /* 可选：尝试物理占簇（需 SeManageVolumePrivilege）*/
    TOKEN_PRIVILEGES tp{};
    LUID luid;
    HANDLE tok = nullptr;
    if( OpenProcessToken( GetCurrentProcess(),
        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok ) &&
        LookupPrivilegeValueW( nullptr, L"SeManageVolumePrivilege", &luid ) )
    {
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        AdjustTokenPrivileges( tok, FALSE, &tp, sizeof( tp ), nullptr, nullptr );
    }
    if( tok ) CloseHandle( tok );

    if( !SetFileValidData( h, off.QuadPart ) ) {
        /* 失败也无所谓，文件已经是稀疏预占状态 */
    }
    return 0;
}

int fallocate( int fd, int mode, int64_t offset, int64_t len )
{
    int result = win_fallocate( fd, offset, len, mode );
    return result;
}

uint32_t getpagesize()
{
    static int pagesize = 0;
    if( pagesize == 0 )
    {
        SYSTEM_INFO system_info;
        GetSystemInfo( &system_info );
        pagesize = std::max( system_info.dwPageSize,
            system_info.dwAllocationGranularity );
    }
    return pagesize;
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif
