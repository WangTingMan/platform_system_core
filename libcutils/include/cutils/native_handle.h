/*
 * Copyright (C) 2009 The Android Open Source Project
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

#ifndef NATIVE_HANDLE_H_
#define NATIVE_HANDLE_H_

#include <cutils/cutils_export.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NATIVE_HANDLE_MAX_FDS 1024
#define NATIVE_HANDLE_MAX_INTS 1024

/* Declare a char array for use with native_handle_init */
#define NATIVE_HANDLE_DECLARE_STORAGE(name, maxFds, maxInts) \
    alignas(native_handle_t) char (name)[                            \
      sizeof(native_handle_t) + sizeof(int) * ((maxFds) + (maxInts))]

typedef struct native_handle
{
    int version;        /* sizeof(native_handle_t) */
    int numFds;         /* number of file-descriptors at &data[0] */
    int numInts;        /* number of ints at &data[numFds] */
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wzero-length-array"
#endif
#ifdef _MSC_VER
#define NATIVE_HANDLE_DATA_SIZE 6
    void* data[NATIVE_HANDLE_DATA_SIZE];
#else
    int data[0];        /* numFds + numInts ints */
#endif
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
} native_handle_t;

typedef const native_handle_t* buffer_handle_t;

/*
 * native_handle_close
 * 
 * closes the file descriptors contained in this native_handle_t
 * 
 * return 0 on success, or a negative error code on failure
 * 
 */
CUTILS_EXPORT int native_handle_close(const native_handle_t* h);

/*
 * native_handle_init
 *
 * Initializes a native_handle_t from storage.  storage must be declared with
 * NATIVE_HANDLE_DECLARE_STORAGE.  numFds and numInts must not respectively
 * exceed maxFds and maxInts used to declare the storage.
 */
CUTILS_EXPORT native_handle_t* native_handle_init(char* storage, int numFds, int numInts);

/*
 * native_handle_create
 *
 * creates a native_handle_t and initializes it. must be destroyed with
 * native_handle_delete(). Note that numFds must be <= NATIVE_HANDLE_MAX_FDS,
 * numInts must be <= NATIVE_HANDLE_MAX_INTS, and both must be >= 0.
 *
 */
CUTILS_EXPORT native_handle_t* native_handle_create(int numFds, int numInts);

/*
 * native_handle_clone
 *
 * creates a native_handle_t and initializes it from another native_handle_t.
 * Must be destroyed with native_handle_delete().
 *
 */
CUTILS_EXPORT native_handle_t* native_handle_clone(const native_handle_t* handle);

/*
 * native_handle_delete
 * 
 * frees a native_handle_t allocated with native_handle_create().
 * This ONLY frees the memory allocated for the native_handle_t, but doesn't
 * close the file descriptors; which can be achieved with native_handle_close().
 * 
 * return 0 on success, or a negative error code on failure
 * 
 */
CUTILS_EXPORT int native_handle_delete(native_handle_t* h);

#define RTLD_NOW 0x00002 /* flag used by dlopen*/
CUTILS_EXPORT void* dlopen( const char* filename, int flag );

#define RTLD_DEFAULT 0x00

typedef void* ( *local_symbol_finder )( const char* p_name );
CUTILS_EXPORT void* symbol_looper( void* p_lib, const char* p_name );
CUTILS_EXPORT void register_local_symbol_finder( local_symbol_finder a_finder );
CUTILS_EXPORT void register_local_symbol( void* symbol, const char* p_name );
CUTILS_EXPORT void register_local_const_symbol( void const* symbol, const char* p_name );
CUTILS_EXPORT int free_library( void* p_lib );

#define dlclose(p_lib) free_library((p_lib))
#define dlsym(p_lib, p_name) symbol_looper(p_lib, p_name)
#define dlerror() "error in dynamic linking" 

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_HANDLE_H_ */
