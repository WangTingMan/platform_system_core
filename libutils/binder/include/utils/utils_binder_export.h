#pragma once

#if defined(WIN32) || defined(_MSC_VER)

#if defined(LIBUTILSBINDERSDK_EXPORTS)
#define LIBUTILSBINDERSDK_API __declspec(dllexport)
#else
#define LIBUTILSBINDERSDK_API __declspec(dllimport)
#endif  // defined(LIBUTILSBINDERSDK_EXPORTS)

#else  // defined(WIN32)
#if defined(LIBUTILSBINDERSDK_EXPORTS)
#define LIBUTILSBINDERSDK_API __attribute__((visibility("default")))
#else
#define LIBUTILSBINDERSDK_API
#endif  // defined(LIBUTILSBINDERSDK_EXPORTS)
#endif

