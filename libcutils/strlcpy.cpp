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

#include <sys/types.h>

#if defined(__GLIBC__) || defined(_WIN32)

#include <string.h>
#include <string>
#include <cutils/memory.h>
#include <cstdlib>
#include <cstdarg>

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

#ifdef __cplusplus
} // extern "C"
#endif

#endif
