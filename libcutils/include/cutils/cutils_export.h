#pragma once

#if defined(WIN32) || defined(_MSC_VER)

#if defined(CUTILS_IMPLEMENTATION)
#define CUTILS_EXPORT __declspec(dllexport)
#else
#define CUTILS_EXPORT __declspec(dllimport)
#endif  // defined(CUTILS_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(CUTILS_IMPLEMENTATION)
#define CUTILS_EXPORT __attribute__((visibility("default")))
#else
#define CUTILS_EXPORT
#endif  // defined(CUTILS_IMPLEMENTATION)
#endif

#ifdef __cplusplus
#ifndef __BEGIN_DECLS
#define __BEGIN_DECLS extern "C" {
#endif
#else
#define __BEGIN_DECLS
#endif

#ifdef __cplusplus
#ifndef __END_DECLS
#define __END_DECLS }
#endif
#else
#define __END_DECLS
#endif

#ifndef ssize_t
#define ssize_t int64_t
#endif
