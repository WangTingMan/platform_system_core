#ifndef ANDROID_LOG_H_HEADER
#define ANDROID_LOG_H_HEADER

#ifdef __cplusplus
extern "C" {
#endif

enum android_LogPriority
{
    /** For internal use only.  */
    ANDROID_LOG_UNKNOWN = 0,
    /** The default priority, for internal use only.  */
    ANDROID_LOG_DEFAULT, /* only for SetMinPriority() */
    /** Verbose logging. Should typically be disabled for a release apk. */
    ANDROID_LOG_VERBOSE,
    /** Debug logging. Should typically be disabled for a release apk. */
    ANDROID_LOG_DEBUG,
    /** Informational logging. Should typically be disabled for a release apk. */
    ANDROID_LOG_INFO,
    /** Warning logging. For use with recoverable failures. */
    ANDROID_LOG_WARN,
    /** Error logging. For use with unrecoverable failures. */
    ANDROID_LOG_ERROR,
    /** Fatal logging. For use when aborting. */
    ANDROID_LOG_FATAL,
    /** For internal use only.  */
    ANDROID_LOG_SILENT, /* only for SetMinPriority(); must be last */
};

#define LOG_VERBOSE ANDROID_LOG_VERBOSE

#ifndef ALOGV
#define ALOGV(...)
#endif

#ifndef ALOGW
#define ALOGW(...)
#endif

#ifndef ALOGE
#define ALOGE(...)
#endif

#ifndef ALOGD
#define ALOGD(...)
#endif

#define ALOG(...)

inline int __android_log_print( int prio, const char* tag, const char* fmt, ... )
{
    return 0;
}

#define android_errorWriteLog(...)
#define ALOG_ASSERT(...)
#define LOG_ALWAYS_FATAL(...)
#define LOG_ALWAYS_FATAL_IF(...)
#define ALOGW_IF(...)

#define ALOG_ASSERT(...)
#define LOG_ALWAYS_FATAL_IF(...)
	
#ifdef __cplusplus
}
#endif

#endif

