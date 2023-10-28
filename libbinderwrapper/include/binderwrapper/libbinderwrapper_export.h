#pragma once

#if defined(WIN32) || defined(_MSC_VER)

#if defined(LIBBINDERWRAPPER_EXPORTS)
#define LIBBINDERWRAPPER_API __declspec(dllexport)
#else
#define LIBBINDERWRAPPER_API __declspec(dllimport)
#endif  // defined(LIBBINDERWRAPPER_EXPORTS)

#else  // defined(WIN32)
#if defined(LIBBINDERWRAPPER_EXPORTS)
#define LIBBINDERWRAPPER_API __attribute__((visibility("default")))
#else
#define LIBBINDERWRAPPER_API
#endif  // defined(LIBBINDERWRAPPER_EXPORTS)
#endif


