#ifndef __MQUEUE_H
#define __MQUEUE_H

#include <windows.h>
#include <time.h>

#include <cutils/cutils_export.h>

#define MQ_PRIO_MAX  32768
#define MQ_MAGIC     0x98765432

#ifndef O_NONBLOCK
#define O_NONBLOCK  0200000
#endif

union sigval {
	int   sival_int;
	void* sival_ptr;
};

struct sigevent {
	int          sigev_notify;
	int          sigev_signo;
	union sigval sigev_value;
};


#ifdef _WIN64
typedef __int64    ssize_t;
#else
typedef signed int ssize_t;
#endif

typedef struct mq_attr {
	long mq_flags;
	long mq_maxmsg;
	long mq_msgsize;
	long mq_curmsgs;
} mq_attr_t;

typedef struct mq_msg {
	int          next_index;
	size_t       msg_len;
	unsigned int msg_prio;
	char         msg_ptr[];
} mq_msg_t;

typedef struct mq_hdr {
	int       mq_magic;
	mq_attr_t mq_attr;
	int       head_index;
	int       free_index;
	char      msgs[];
} mq_hdr_t;

typedef struct mq_des {
	int       mq_magic;
	int       mq_flags;
	int       mq_access;
	HANDLE    file_handle;
	HANDLE    map_handle;
	HANDLE    mutex;
	HANDLE    event;
	mq_hdr_t* mq_hdr;
	BOOL      in_send;
	BOOL      in_receive;
} mq_des_t, *mqd_t;

#ifdef __cplusplus
extern "C" {
#endif

CUTILS_EXPORT mqd_t   mq_open(const char* name, int oflag, ...);
CUTILS_EXPORT int     mq_close(mqd_t mqdes);
CUTILS_EXPORT int     mq_unlink(const char* name);

CUTILS_EXPORT int     mq_send(mqd_t mqdes, const char* msg_ptr, size_t msg_len, unsigned int msg_prio);
CUTILS_EXPORT int     mq_timedsend(mqd_t mqdes, const char* msg_ptr, size_t msg_len, unsigned int msg_prio, const struct timespec* abs_timeout);
CUTILS_EXPORT ssize_t mq_receive(mqd_t mqdes, char* msg_ptr, size_t msg_len, unsigned int* msg_prio);
CUTILS_EXPORT ssize_t mq_timedreceive(mqd_t mqdes, char* msg_ptr, size_t msg_len, unsigned int* msg_prio, const struct timespec* abs_timeout);

CUTILS_EXPORT int     mq_getattr(mqd_t mqdes, struct mq_attr* attr);
CUTILS_EXPORT int     mq_setattr(mqd_t mqdes, const struct mq_attr* newattr, struct mq_attr* oldattr);

CUTILS_EXPORT int     mq_notify(mqd_t mqdes, const struct sigevent* notification);

CUTILS_EXPORT void mq_dump(mqd_t mqdes);

#ifdef __cplusplus
}
#endif

#endif
