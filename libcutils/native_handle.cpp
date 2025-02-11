/*
 * Copyright (C) 2007 The Android Open Source Project
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
#define _CRT_NONSTDC_NO_WARNINGS

#include <cutils/native_handle.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <corecrt_io.h>
#else
#include <unistd.h>
#endif

#include <shared_mutex>
#include <vector>
#include <map>

#ifdef __cplusplus
extern "C" {
#endif

// Needs to come after stdlib includes to capture the __BIONIC__ definition
#ifdef __BIONIC__
#include <android/fdsan.h>
#endif

namespace {

#if !defined(__BIONIC__)
// fdsan stubs when not linked against bionic
#define ANDROID_FDSAN_OWNER_TYPE_NATIVE_HANDLE 0

uint64_t android_fdsan_create_owner_tag(int /*type*/, uint64_t /*tag*/) {
    return 0;
}
uint64_t android_fdsan_get_owner_tag(int /*fd*/) {
    return 0;
}
int android_fdsan_close_with_tag(int fd, uint64_t /*tag*/) {
    return close(fd);
}
void android_fdsan_exchange_owner_tag(int /*fd*/, uint64_t /*expected_tag*/, uint64_t /*tag*/) {}
#endif  // !__BIONIC__

uint64_t get_fdsan_tag(const native_handle_t* handle) {
    return android_fdsan_create_owner_tag(ANDROID_FDSAN_OWNER_TYPE_NATIVE_HANDLE,
                                          reinterpret_cast<uint64_t>(handle));
}

int close_internal(const native_handle_t* h, bool allowUntagged) {
    if (!h) return 0;

    if (h->version != sizeof(native_handle_t)) return -EINVAL;

    const int numFds = h->numFds;
    uint64_t tag;
    if (allowUntagged && numFds > 0 && android_fdsan_get_owner_tag(h->data[0]) == 0) {
        tag = 0;
    } else {
        tag = get_fdsan_tag(h);
    }
    int saved_errno = errno;
    for (int i = 0; i < numFds; ++i) {
        android_fdsan_close_with_tag(h->data[i], tag);
    }
    errno = saved_errno;
    return 0;
}

void swap_fdsan_tags(const native_handle_t* handle, uint64_t expected_tag, uint64_t new_tag) {
    if (!handle || handle->version != sizeof(native_handle_t)) return;

    for (int i = 0; i < handle->numFds; i++) {
        // allow for idempotence to make the APIs easier to use
        if (android_fdsan_get_owner_tag(handle->data[i]) != new_tag) {
            android_fdsan_exchange_owner_tag(handle->data[i], expected_tag, new_tag);
        }
    }
}

}  // anonymous namespace

native_handle_t* native_handle_init(char* storage, int numFds, int numInts) {
    if ((uintptr_t)storage % alignof(native_handle_t)) {
        errno = EINVAL;
        return NULL;
    }

    native_handle_t* handle = (native_handle_t*)storage;
    handle->version = sizeof(native_handle_t);
    handle->numFds = numFds;
    handle->numInts = numInts;
#ifdef _MSC_VER
    for( int i = 0; i < NATIVE_HANDLE_DATA_SIZE; ++i )
    {
        handle->data[i] = INVALID_HANDLE_VALUE;
    }
#endif
    return handle;
}

native_handle_t* native_handle_create(int numFds, int numInts) {
    if (numFds < 0 || numInts < 0 || numFds > NATIVE_HANDLE_MAX_FDS ||
        numInts > NATIVE_HANDLE_MAX_INTS) {
        errno = EINVAL;
        return NULL;
    }

    size_t mallocSize = sizeof(native_handle_t) + (sizeof(int) * (numFds + numInts));
    native_handle_t* h = static_cast<native_handle_t*>(malloc(mallocSize));
    if (h) {
        h->version = sizeof(native_handle_t);
        h->numFds = numFds;
        h->numInts = numInts;
#ifdef _MSC_VER
        for( int i = 0; i < NATIVE_HANDLE_DATA_SIZE; ++i )
        {
            h->data[i] = INVALID_HANDLE_VALUE;
        }
#endif
    }
    return h;
}

void native_handle_set_fdsan_tag(const native_handle_t* handle) {
    swap_fdsan_tags(handle, 0, get_fdsan_tag(handle));
}

void native_handle_unset_fdsan_tag(const native_handle_t* handle) {
    swap_fdsan_tags(handle, get_fdsan_tag(handle), 0);
}

native_handle_t* native_handle_clone(const native_handle_t* handle) {
    native_handle_t* clone = native_handle_create(handle->numFds, handle->numInts);
    if (clone == NULL) return NULL;
#ifndef _MSC_VER
    for (int i = 0; i < handle->numFds; i++) {
        clone->data[i] = dup(handle->data[i]);
        if (clone->data[i] == -1) {
            clone->numFds = i;
            native_handle_close(clone);
            native_handle_delete(clone);
            return NULL;
        }
    }
#endif
    memcpy(&clone->data[handle->numFds], &handle->data[handle->numFds],
           sizeof(int) * handle->numInts);

    return clone;
}

int native_handle_delete(native_handle_t* h) {
    if (h) {
        if (h->version != sizeof(native_handle_t)) return -EINVAL;
        free(h);
    }
    return 0;
}

int native_handle_close(const native_handle_t* h) {
<<<<<<< HEAD
    if (!h) return 0;

    if (h->version != sizeof(native_handle_t)) return -EINVAL;

    int saved_errno = errno;
    const int numFds = h->numFds;
    for (int i = 0; i < numFds; ++i) {
#ifdef _MSC_VER
        CloseHandle( h->data[i] );
#else
        close(h->data[i]);
#endif
    }
    errno = saved_errno;
    return 0;
=======
    return close_internal(h, /*allowUntagged=*/true);
}

int native_handle_close_with_tag(const native_handle_t* h) {
    return close_internal(h, /*allowUntagged=*/false);
>>>>>>> 64d68e1d6
}

/**
 * Safe function used to open dynamic libs. This attempts to improve program security
 * by removing the current directory from the dll search path. Only dll's found in the
 * executable or system directory are allowed to be loaded.
 * @param name  The dynamic lib name.
 * @return A handle to the opened lib.
 * 
 * This function is part of FFmpeg.
 */
static inline HMODULE win32_dlopen( const char* name )
{
#if _WIN32_WINNT < 0x0602
    // Need to check if KB2533623 is available
    if( !GetProcAddress( GetModuleHandleW( L"kernel32.dll" ), "SetDefaultDllDirectories" ) )
    {
        HMODULE module = NULL;
        wchar_t* path = NULL, * name_w = NULL;
        DWORD pathlen;
        if( utf8towchar( name, &name_w ) )
            goto exit;
        path = (wchar_t*)av_mallocz_array( MAX_PATH, sizeof( wchar_t ) );
        // Try local directory first
        pathlen = GetModuleFileNameW( NULL, path, MAX_PATH );
        pathlen = wcsrchr( path, '\\' ) - path;
        if( pathlen == 0 || pathlen + wcslen( name_w ) + 2 > MAX_PATH )
            goto exit;
        path[pathlen] = '\\';
        wcscpy( path + pathlen + 1, name_w );
        module = LoadLibraryExW( path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH );
        if( module == NULL )
        {
            // Next try System32 directory
            pathlen = GetSystemDirectoryW( path, MAX_PATH );
            if( pathlen == 0 || pathlen + wcslen( name_w ) + 2 > MAX_PATH )
                goto exit;
            path[pathlen] = '\\';
            wcscpy( path + pathlen + 1, name_w );
            module = LoadLibraryExW( path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH );
        }
    exit:
        av_free( path );
        av_free( name_w );
        return module;
    }
#endif
#ifndef LOAD_LIBRARY_SEARCH_APPLICATION_DIR
#   define LOAD_LIBRARY_SEARCH_APPLICATION_DIR 0x00000200
#endif
#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#   define LOAD_LIBRARY_SEARCH_SYSTEM32        0x00000800
#endif
#if HAVE_WINRT
    wchar_t* name_w = NULL;
    int ret;
    if( utf8towchar( name, &name_w ) )
        return NULL;
    ret = LoadPackagedLibrary( name_w, 0 );
    av_free( name_w );
    return ret;
#else
    return LoadLibraryExA( name, NULL, LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32 );
#endif
}

void* dlopen( const char* filename, int flag )
{
    return win32_dlopen( filename );
}

static std::shared_mutex symbol_finders_mutex;
static std::vector<local_symbol_finder> s_symbol_finders;
static std::map<std::string, void*> s_registered_symbols;

void register_local_symbol_finder( local_symbol_finder a_finder )
{
    std::lock_guard<std::shared_mutex> locker( symbol_finders_mutex );
    s_symbol_finders.emplace_back( a_finder );
}

int free_library( void* p_lib )
{
    FreeLibrary( (HMODULE)( p_lib ) );
    return 0;
}

void register_local_symbol( void* symbol, const char* p_name )
{
    std::lock_guard<std::shared_mutex> locker( symbol_finders_mutex );
    s_registered_symbols[p_name] = symbol;
}

void register_local_const_symbol(void const* symbol, const char* p_name)
{
    std::lock_guard<std::shared_mutex> locker(symbol_finders_mutex);
    s_registered_symbols[p_name] = const_cast<void*>( symbol );
}

void* symbol_looper( void* p_lib, const char* p_name )
{
    if( p_lib )
    {
        return GetProcAddress( (HMODULE)( p_lib ), p_name );
    }

    HMODULE mod = GetModuleHandle( nullptr );
    FARPROC proc = GetProcAddress( mod, p_name );
    if( proc )
    {
        return proc;
    }

    std::vector<local_symbol_finder> symbol_finders;
    std::shared_lock<std::shared_mutex> lcker( symbol_finders_mutex );
    symbol_finders = s_symbol_finders;
    lcker.unlock();

    for( auto& ele : symbol_finders )
    {
        if( ele )
        {
            void* p = ele( p_name );
            if( p )
            {
                return p;
            }
        }
    }

    lcker.lock();
    auto it = s_registered_symbols.find(p_name);
    if( it != s_registered_symbols.end() )
    {
        return it->second;
    }
    return nullptr;
}

#ifdef __cplusplus
}
#endif
