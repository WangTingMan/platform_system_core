#pragma once

#if defined(WIN32) || defined(_MSC_VER)

#if defined(UTILS_IMPLEMENTATION)
#define UTILS_EXPORT __declspec(dllexport)
#else
#define UTILS_EXPORT __declspec(dllimport)
#endif  // defined(UTILS_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(UTILS_IMPLEMENTATION)
#define UTILS_EXPORT __attribute__((visibility("default")))
#else
#define UTILS_EXPORT
#endif  // defined(UTILS_IMPLEMENTATION)
#endif


