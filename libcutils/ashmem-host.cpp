/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cutils/ashmem.h>

#define NOMINMAX

/*
 * Implementation of the user-space ashmem API for the simulator, which lacks
 * an ashmem-enabled kernel. See ashmem-dev.c for the real ashmem-based version.
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <windows.h>

#include <atomic>
#include <memory>
#include <vector>

#include <base/memory/shared_memory.h>
#include <base/logging.h>
#include <base/strings/sys_string_conversions.h>
#include <base/rand_util.h>
#include <base/strings/utf_string_conversions.h>
#include <base/strings/stringprintf.h>

static bool ashmem_validate_stat(int fd, struct stat* buf) {
    int result = fstat(fd, buf);
    if (result == -1) {
        return false;
    }
#ifndef _MSC_VER
    /*
     * Check if this is an "ashmem" region.
     * TODO: This is very hacky, and can easily break.
     * We need some reliable indicator.
     */
    if (!(buf->st_nlink == 0 && S_ISREG(buf->st_mode))) {
        errno = ENOTTY;
        return false;
    }
#endif
    return true;
}

#ifdef __cplusplus
extern "C" {
#endif

int ashmem_valid( ASHMEM_HANDLE fd) {
#ifdef _MSC_VER
    return INVALID_HANDLE_VALUE == fd;
#else
    struct stat buf;
    return ashmem_validate_stat(fd, &buf);
#endif
}

ASHMEM_HANDLE ashmem_create_region(const char* a_name, size_t size) {
#ifndef _MSC_VER
    char pattern[PATH_MAX];
    snprintf(pattern, sizeof(pattern), "/tmp/android-ashmem-%d-XXXXXXXXX", getpid());
    int fd = mkstemp(pattern);
    if (fd == -1) return -1;

    unlink(pattern);

    if (TEMP_FAILURE_RETRY(ftruncate(fd, size)) == -1) {
      close(fd);
      return -1;
    }
    return fd;
#else
    HANDLE sh_hdl = INVALID_HANDLE_VALUE;
    // Check maximum accounting for overflow.
    constexpr size_t kSectionMask = 65536 - 1;
    if( size >
        static_cast<size_t>( std::numeric_limits<int>::max() ) - kSectionMask )
    {
        LOG( ERROR ) << "Size to large!";
        return sh_hdl;
    }

    std::wstring name =  base::SysNativeMBToWide( std::string{ a_name } );
    sh_hdl = CreateFileMapping( INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
                                  static_cast<DWORD>( size ), name.c_str() );
    if( !sh_hdl )
    {
        LOG( ERROR ) << "CREATE_FILE_MAPPING_FAILURE, code = " << GetLastError();
        return sh_hdl;
    }
    HANDLE h2;
    BOOL success = ::DuplicateHandle(
        GetCurrentProcess(), sh_hdl, GetCurrentProcess(), &h2,
        FILE_MAP_READ | FILE_MAP_WRITE | SECTION_QUERY, FALSE, 0 );
    BOOL rv = ::CloseHandle( sh_hdl );
    if( !success )
    {
        LOG( ERROR ) << "REDUCE_PERMISSIONS_FAILURE, code = " << GetLastError();
        return INVALID_HANDLE_VALUE;
    }

    sh_hdl = h2;

    // If the shared memory already exists, something has gone wrong.
    if( GetLastError() == ERROR_ALREADY_EXISTS )
    {
        rv = ::CloseHandle( sh_hdl );
        sh_hdl = INVALID_HANDLE_VALUE;
        LOG( ERROR ) << "ALREADY_EXISTS";
        return sh_hdl;
    }

    return sh_hdl;
#endif
}

/**
 * The permission already added in ashmem_create_region. So just ignore.
 */
int ashmem_set_prot_region( ASHMEM_HANDLE /*fd*/, int /*prot*/) {
    return 0;
}

int ashmem_pin_region( ASHMEM_HANDLE /*fd*/, size_t /*offset*/, size_t /*len*/) {
    return 0 /*ASHMEM_NOT_PURGED*/;
}

int ashmem_unpin_region( ASHMEM_HANDLE /*fd*/, size_t /*offset*/, size_t /*len*/) {
    return 0 /*ASHMEM_IS_UNPINNED*/;
}

int ashmem_get_size_region( ASHMEM_HANDLE fd)
{
#ifdef _MSC_VER
    return 0;
#else
    struct stat buf;
    if (!ashmem_validate_stat(fd, &buf)) {
        return -1;
    }

    return buf.st_size;
#endif
}

#ifdef __cplusplus
}
#endif

