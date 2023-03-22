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


