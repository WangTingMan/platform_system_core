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

#pragma once

#include <stddef.h>

#if defined(__BIONIC__)
#include <linux/ashmem.h>
#endif

#include "cutils/cutils_export.h"

#ifdef __cplusplus
extern "C" {
#endif

CUTILS_EXPORT int ashmem_valid(int fd);
CUTILS_EXPORT int ashmem_create_region(const char *name, size_t size);
CUTILS_EXPORT int ashmem_set_prot_region(int fd, int prot);
CUTILS_EXPORT int ashmem_pin_region(int fd, size_t offset, size_t len);
CUTILS_EXPORT int ashmem_unpin_region(int fd, size_t offset, size_t len);
CUTILS_EXPORT int ashmem_get_size_region(int fd);

#ifdef __cplusplus
}
#endif
