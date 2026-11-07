/*
 * POSIX message queue for Windows
 * https://oss.onl/mqueue-win32/
 *
 * (C) 2018 HIRUKAWA Ryo
 *
 * This software is licensed under the MIT License.
 * https://oss.onl/mqueue-win32/LICENSE.txt
 */
#include <windows.h>
#include <stdio.h>
#include <time.h>

#include <errno.h>
#include <fcntl.h>

#include "cutils/mqueue.h"

#ifndef O_CLOEXEC
#define O_CLOEXEC	02000000
#endif

#define ALIGN(x,a) ( ( (x)+(a)-1)&~( (a)-1) )
#define SIZEOF_MQ_MSG_T(attr) ( sizeof(mq_msg_t) + ALIGN((attr)->mq_msgsize, 16) )
#define GET_MQ_MSG_T(mqdes, index) ( (mq_msg_t*)&mqdes->mq_hdr->msgs[index * SIZEOF_MQ_MSG_T(&mqdes->mq_hdr->mq_attr)] )

mq_attr_t def_attr = { 0, 10, 8192, 0 };

static long long current_time_millis(void)
{
    SYSTEMTIME unix_epoch;
    FILETIME unix_epoch_ft_from_1601;
    ULARGE_INTEGER unix_epoch_from_1601;

    FILETIME current_ft_from_1601;
    ULARGE_INTEGER current_from_1601;

    unix_epoch.wYear = 1970;
	unix_epoch.wMonth = 1;
	unix_epoch.wDayOfWeek = 4;
	unix_epoch.wDay = 1;
	unix_epoch.wHour = 0;
	unix_epoch.wMinute = 0;
	unix_epoch.wSecond = 0;
	unix_epoch.wMilliseconds = 0;

	SystemTimeToFileTime(&unix_epoch, &unix_epoch_ft_from_1601);
	unix_epoch_from_1601.HighPart = unix_epoch_ft_from_1601.dwHighDateTime;
	unix_epoch_from_1601.LowPart = unix_epoch_ft_from_1601.dwLowDateTime;

	GetSystemTimeAsFileTime(&current_ft_from_1601);
	current_from_1601.HighPart = current_ft_from_1601.dwHighDateTime;
	current_from_1601.LowPart = current_ft_from_1601.dwLowDateTime;

	//return (current_from_1601.QuadPart - unix_epoch_from_1601.QuadPart) / 10000;
	//unix_epoch_from_1601.QuadPart = 116444736000000000LL;
	return (current_from_1601.QuadPart - 116444736000000000LL) / 10000;
}

static int lock(mqd_t mqdes)
{
	DWORD ret = WaitForSingleObject(mqdes->mutex, INFINITE);
	switch(ret)
	{
	case WAIT_OBJECT_0:
		errno = 0;
		break;
	case WAIT_ABANDONED:
		errno = ECANCELED;
		break;
	default:   
		errno = EINVAL;
	}
	return ret;
}

static void unlock(mqd_t mqdes)
{
	ReleaseMutex(mqdes->mutex);
}

static int wait(mqd_t mqdes, DWORD timeout)
{
	ResetEvent(mqdes->event);
	SignalObjectAndWait(mqdes->mutex, mqdes->event, timeout, FALSE);
	return lock(mqdes);
}

static void notify_all(mqd_t mqdes)
{
	PulseEvent(mqdes->event);
}

mqd_t mq_open(const char* name, int oflag, ...)
{
	mq_des_t*  mqdes = NULL;
	va_list    arg_ptr;
	int        mode;
	mq_attr_t* attr  = NULL;
	char       mqueue_path[MAX_PATH + 1];
	DWORD      file_size;
	HANDLE     file_handle = NULL;
	BOOL       already_exists;
	HANDLE     map_handle = NULL;
	char       mutex_name[MAX_PATH + 1];
	HANDLE     mutex = NULL;
	char       event_name[MAX_PATH + 1];
	HANDLE     event = NULL;
	LPVOID     map_view_ptr = NULL;

    if ((oflag & ~(O_RDONLY | O_WRONLY | O_RDWR | O_CLOEXEC | O_CREAT | O_EXCL | O_NONBLOCK)) == 0)
	{
    	if(oflag & O_CREAT)
    	{
        	va_start(arg_ptr, oflag);
        	mode = va_arg(arg_ptr, int);
        	attr = va_arg(arg_ptr, mq_attr_t*);
        	va_end(arg_ptr);
    	}
    }

	if (attr == NULL)
	{
		attr = &def_attr;
	}
	if(oflag & O_NONBLOCK)
	{
		attr->mq_flags = O_NONBLOCK;
	}

	if (name == NULL || strlen(name) <= 1 || name[0] != '/')
	{
		errno = ENOENT;
		return (mqd_t)-1;
	}
	if (strlen(name) > 128)
	{
		errno = ENAMETOOLONG;
		return (mqd_t)-1;
	}
	if(strchr(name + 1, '/') != NULL)
	{
		errno = EACCES;
		return (mqd_t)-1;
	}

	GetTempPathA(MAX_PATH + 1, mqueue_path);
	strcat_s(mqueue_path, MAX_PATH + 1, "mqueue\\");
	CreateDirectoryA(mqueue_path, NULL);
	strcat_s(mqueue_path, MAX_PATH + 1, name + 1);

	file_size = sizeof(struct mq_hdr) + SIZEOF_MQ_MSG_T(attr) * attr->mq_maxmsg;
	file_handle = CreateFileA(mqueue_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
	if (file_handle == NULL)
	{
		errno = EACCES;
		goto EXIT_ERROR;
	}
	already_exists = (GetLastError() == ERROR_ALREADY_EXISTS);

	if(!(oflag & O_CREAT) && !already_exists)
	{
		DeleteFileA(mqueue_path);
		errno = ENOENT;
		goto EXIT_ERROR;
	}
	if((oflag & O_CREAT) && (oflag & O_EXCL) && already_exists)
	{
		errno = EEXIST;
		goto EXIT_ERROR;
	}

	map_handle = CreateFileMapping(file_handle, NULL, PAGE_READWRITE, 0, file_size, name);
	if (map_handle == NULL)
	{
		errno = EACCES;
		goto EXIT_ERROR;
	}

	strcpy_s(mutex_name, MAX_PATH + 1, "mqueue-mutex-");
	strcat_s(mutex_name, MAX_PATH + 1, name + 1);
	mutex = CreateMutexA(NULL, FALSE, mutex_name);
	if (mutex == NULL)
	{
		errno = EACCES;
		goto EXIT_ERROR;
	}

	strcpy_s(event_name, MAX_PATH + 1, "mqueue-event-");
	strcat_s(event_name, MAX_PATH + 1, name + 1);
	event = CreateEventA(NULL, TRUE, FALSE, event_name);
	if (event == NULL)
	{
		errno = EACCES;
		goto EXIT_ERROR;
	}

	map_view_ptr = MapViewOfFile(map_handle, FILE_MAP_WRITE, 0, 0, file_size);
	if (map_view_ptr == NULL)
	{
		errno = EACCES;
		goto EXIT_ERROR;
	}

	mqdes = (mq_des_t*)malloc(sizeof(mq_des_t));
	mqdes->mq_magic = MQ_MAGIC;
	mqdes->mq_flags = attr->mq_flags & O_NONBLOCK;
	mqdes->mq_access = oflag & (O_RDONLY | O_WRONLY | O_RDWR);
	mqdes->file_handle = file_handle;
	mqdes->map_handle = map_handle;
	mqdes->mutex = mutex;
	mqdes->event = event;
	mqdes->mq_hdr = (mq_hdr_t*)map_view_ptr;
	mqdes->in_send = FALSE;
	mqdes->in_receive = FALSE;
	if (!already_exists || mqdes->mq_hdr->mq_magic != MQ_MAGIC)
	{
		int i;

		mqdes->mq_hdr->mq_magic = MQ_MAGIC;
		mqdes->mq_hdr->mq_attr.mq_flags = 0;
		mqdes->mq_hdr->mq_attr.mq_maxmsg = attr->mq_maxmsg;
		mqdes->mq_hdr->mq_attr.mq_msgsize = attr->mq_msgsize;
		mqdes->mq_hdr->mq_attr.mq_curmsgs = 0;

		mqdes->mq_hdr->head_index = -1;
		mqdes->mq_hdr->free_index = 0;

		for (i = 0; i < attr->mq_maxmsg; i++)
		{
			mq_msg_t* msg = GET_MQ_MSG_T(mqdes, i);
			msg->next_index = -1;
			msg->msg_len = 0;
			msg->msg_prio = 0;
		}
	}

	return mqdes;

EXIT_ERROR:
	if (map_view_ptr != NULL)
	{
		UnmapViewOfFile(map_view_ptr);
	}
	if (event != NULL)
	{
		CloseHandle(event);
	}
	if (mutex != NULL)
	{
		CloseHandle(mutex);
	}
	if (map_handle != NULL)
	{
		CloseHandle(map_handle);
	}
	if (file_handle != NULL)
	{
		CloseHandle(file_handle);
	}
	return (mqd_t)-1;
}

int mq_close(mqd_t mqdes)
{
	if (mqdes == NULL || mqdes->mq_magic != MQ_MAGIC)
	{
		errno = EBADF;
		return -1;
	}

	if (lock(mqdes) != WAIT_OBJECT_0)
	{
		return -1;
	}

	mqdes->mq_magic = 0;
	while (mqdes->in_send || mqdes->in_receive)
	{
		notify_all(mqdes);
		wait(mqdes, INFINITE);
	}
	unlock(mqdes);

	UnmapViewOfFile(mqdes->mq_hdr);
	CloseHandle(mqdes->event);
	CloseHandle(mqdes->map_handle);
	CloseHandle(mqdes->file_handle);
	CloseHandle(mqdes->mutex);
	free(mqdes);

	return 0;
}

int mq_unlink(const char* name)
{
	char       mqueue_path[MAX_PATH + 1];

	if (name == NULL || strlen(name) <= 1 || name[0] != '/')
	{
		errno = ENOENT;
		return -1;
	}
	if (strlen(name) > 128)
	{
		errno = ENAMETOOLONG;
		return -1;
	}

	GetTempPathA(MAX_PATH + 1, mqueue_path);
	strcat_s(mqueue_path, MAX_PATH + 1, "mqueue\\");
	CreateDirectoryA(mqueue_path, NULL);
	strcat_s(mqueue_path, MAX_PATH + 1, name + 1);

	if (DeleteFile(mqueue_path))
	{
		return 0;
	}

	switch (GetLastError())
	{
	case ERROR_FILE_NOT_FOUND:
		errno = ENOENT;
		break;
	default:
		errno = EACCES;
	}
	return -1;
}

int mq_send(mqd_t mqdes, const char* msg_ptr, size_t msg_len, unsigned int msg_prio)
{
	return mq_timedsend(mqdes, msg_ptr, msg_len, msg_prio, NULL);
}

int mq_timedsend(mqd_t mqdes, const char* msg_ptr, size_t msg_len, unsigned int msg_prio, const struct timespec* abs_timeout)
{
	mq_msg_t* new_msg;

	if (mqdes == NULL || mqdes->mq_magic != MQ_MAGIC)
	{
		errno = EBADF;
		return -1;
	}

	if(mqdes->mq_access != O_RDONLY && mqdes->mq_access != O_RDWR)
	{
		errno = EACCES;
		return -1;
	}

	if (abs_timeout != NULL)
	{
		if (abs_timeout->tv_sec < 0 || abs_timeout->tv_nsec < 0 || abs_timeout->tv_nsec >= 1000000000)
		{
			errno = EINVAL;
			return -1;
		}
	}

	mqdes->in_send = TRUE;

	if (lock(mqdes) != WAIT_OBJECT_0)
	{
		mqdes->in_send = FALSE;
		return -1;
	}
	if (mqdes->mq_magic == 0)
	{
		errno = EINTR;
		goto EXIT_ERROR;
	}

	if (msg_len > (size_t)mqdes->mq_hdr->mq_attr.mq_msgsize)
	{
		errno = EMSGSIZE;
		goto EXIT_ERROR;
	}

	if (mqdes->mq_hdr->mq_attr.mq_curmsgs >= mqdes->mq_hdr->mq_attr.mq_maxmsg)
	{
		if (mqdes->mq_flags & O_NONBLOCK)
		{
			errno = EAGAIN;
			goto EXIT_ERROR;
		}

		while (mqdes->mq_hdr->mq_attr.mq_curmsgs >= mqdes->mq_hdr->mq_attr.mq_maxmsg)
		{
			if (abs_timeout == NULL)
			{
				if (wait(mqdes, INFINITE) != WAIT_OBJECT_0)
				{
					return -1;
				}
			}
			else
			{
				int timeout = (int)((abs_timeout->tv_sec * 1000) + (abs_timeout->tv_nsec / 1000000) - current_time_millis());
				if (timeout > 0)
				{
					if (wait(mqdes, timeout) != WAIT_OBJECT_0)
					{
						return -1;
					}
				}
				else
				{
					errno = ETIMEDOUT;
					goto EXIT_ERROR;
				}
			}

			if (mqdes->mq_magic == 0)
			{
				errno = EINTR;
				goto EXIT_ERROR;
			}
		}
	}
	
	new_msg = GET_MQ_MSG_T(mqdes, mqdes->mq_hdr->free_index);
	new_msg->msg_prio = msg_prio;
	new_msg->msg_len = msg_len;
	memcpy_s(new_msg->msg_ptr, mqdes->mq_hdr->mq_attr.mq_msgsize, msg_ptr, msg_len);

	if (mqdes->mq_hdr->mq_attr.mq_curmsgs == 0)
	{
		new_msg->next_index = -1;
		mqdes->mq_hdr->head_index = mqdes->mq_hdr->free_index;
	}
	else
	{
		mq_msg_t* prev_msg = NULL;
		int index = mqdes->mq_hdr->head_index;
		while (index != -1)
		{
			mq_msg_t* msg = GET_MQ_MSG_T(mqdes, index);
			if (new_msg->msg_prio > msg->msg_prio)
			{
				new_msg->next_index = index;
				if (prev_msg != NULL)
				{
					prev_msg->next_index = mqdes->mq_hdr->free_index;
				}
				else
				{
					mqdes->mq_hdr->head_index = mqdes->mq_hdr->free_index;
				}
				break;
			}
			prev_msg = msg;
			index = msg->next_index;
		}
		if (index == -1)
		{
			new_msg->next_index = -1;
			prev_msg->next_index = mqdes->mq_hdr->free_index;
		}
	}

	mqdes->mq_hdr->free_index = ++mqdes->mq_hdr->free_index % mqdes->mq_hdr->mq_attr.mq_maxmsg;
	mqdes->mq_hdr->mq_attr.mq_curmsgs++;

	mqdes->in_send = FALSE;
	notify_all(mqdes);
	unlock(mqdes);
	return 0;

EXIT_ERROR:
	mqdes->in_send = FALSE;
	notify_all(mqdes);
	unlock(mqdes);
	return -1;
}

ssize_t mq_receive(mqd_t mqdes, char* msg_ptr, size_t msg_len, unsigned int* msg_prio)
{
	return mq_timedreceive(mqdes, msg_ptr, msg_len, msg_prio, NULL);
}

ssize_t mq_timedreceive(mqd_t mqdes, char* msg_ptr, size_t msg_len, unsigned int* msg_prio, const struct timespec* abs_timeout)
{
	mq_msg_t* msg;

	if (mqdes == NULL || mqdes->mq_magic != MQ_MAGIC)
	{
		errno = EBADF;
		return -1;
	}

	if(mqdes->mq_access != O_WRONLY && mqdes->mq_access != O_RDWR)
	{
		errno = EACCES;
		return -1;
	}

	if (abs_timeout != NULL)
	{
		if (abs_timeout->tv_sec < 0 || abs_timeout->tv_nsec < 0 || abs_timeout->tv_nsec >= 1000000000)
		{
			errno = EINVAL;
			return -1;
		}
	}

	mqdes->in_receive = TRUE;

	if (lock(mqdes) != WAIT_OBJECT_0)
	{
		mqdes->in_receive = FALSE;
		return -1;
	}
	if (mqdes->mq_magic == 0)
	{
		errno = EINTR;
		goto EXIT_ERROR;
	}

	if (mqdes->mq_hdr->mq_attr.mq_curmsgs == 0)
	{
		if (mqdes->mq_flags & O_NONBLOCK)
		{
			errno = EAGAIN;
			goto EXIT_ERROR;
		}

		while (mqdes->mq_hdr->mq_attr.mq_curmsgs == 0)
		{
			if (abs_timeout == NULL)
			{
				if (wait(mqdes, INFINITE) != WAIT_OBJECT_0)
				{
					return -1;
				}
			}
			else
			{
				int timeout = (int)((abs_timeout->tv_sec * 1000) + (abs_timeout->tv_nsec / 1000000) - current_time_millis());
				if (timeout > 0)
				{
					if (wait(mqdes, timeout) != WAIT_OBJECT_0)
					{
						return -1;
					}
				}
				else
				{
					errno = ETIMEDOUT;
					goto EXIT_ERROR;
				}
			}

			if (mqdes->mq_magic == 0)
			{
				errno = EINTR;
				goto EXIT_ERROR;
			}
		}
	}

	if (mqdes->mq_hdr->mq_attr.mq_curmsgs == 0)
	{
		errno = EAGAIN;
		goto EXIT_ERROR;
	}

	msg = GET_MQ_MSG_T(mqdes, mqdes->mq_hdr->head_index);
	if (msg_len < msg->msg_len)
	{
		errno = EMSGSIZE;
		goto EXIT_ERROR;
	}
	if (msg_prio != NULL)
	{
		*msg_prio = msg->msg_prio;
	}
	memcpy_s(msg_ptr, msg_len, msg->msg_ptr, msg->msg_len);
	mqdes->mq_hdr->head_index = msg->next_index;
	mqdes->mq_hdr->mq_attr.mq_curmsgs--;


	mqdes->in_receive = FALSE;
	notify_all(mqdes);
	unlock(mqdes);
	return msg->msg_len;

EXIT_ERROR:
	mqdes->in_receive = FALSE;
	notify_all(mqdes);
	unlock(mqdes);
	return -1;
}

int mq_getattr(mqd_t mqdes, struct mq_attr* attr)
{
	if (mqdes == NULL || mqdes->mq_magic != MQ_MAGIC)
	{
		errno = EBADF;
		return -1;
	}

	if (attr == NULL)
	{
		errno = ENOMEM;
		return -1;
	}

	if (lock(mqdes) != WAIT_OBJECT_0)
	{
		mqdes->in_receive = FALSE;
		return -1;
	}
	if (mqdes->mq_magic == 0)
	{
		errno = EINTR;
		goto EXIT_ERROR;
	}

	attr->mq_flags = mqdes->mq_flags & O_NONBLOCK;
	attr->mq_maxmsg = mqdes->mq_hdr->mq_attr.mq_maxmsg;
	attr->mq_msgsize = mqdes->mq_hdr->mq_attr.mq_msgsize;
	attr->mq_curmsgs = mqdes->mq_hdr->mq_attr.mq_curmsgs;

	unlock(mqdes);
	return 0;

EXIT_ERROR:
	unlock(mqdes);
	return -1;
}
int mq_setattr(mqd_t mqdes, const struct mq_attr* newattr, struct mq_attr* oldattr)
{
	if (mqdes == NULL || mqdes->mq_magic != MQ_MAGIC)
	{
		errno = EBADF;
		return -1;
	}

	if (newattr == NULL)
	{
		errno = ENOMEM;
		return -1;
	}
	if (newattr->mq_flags != 0 && newattr->mq_flags != O_NONBLOCK)
	{
		errno = EINVAL;
		return -1;
	}

	if (lock(mqdes) != WAIT_OBJECT_0)
	{
		mqdes->in_receive = FALSE;
		return -1;
	}
	if (mqdes->mq_magic == 0)
	{
		errno = EINTR;
		goto EXIT_ERROR;
	}

	if (oldattr != NULL)
	{
		oldattr->mq_flags = mqdes->mq_flags;
		oldattr->mq_maxmsg = mqdes->mq_hdr->mq_attr.mq_maxmsg;
		oldattr->mq_msgsize = mqdes->mq_hdr->mq_attr.mq_msgsize;
		oldattr->mq_curmsgs = mqdes->mq_hdr->mq_attr.mq_curmsgs;
	}

	mqdes->mq_flags |= (newattr->mq_flags & O_NONBLOCK) ? O_NONBLOCK : 0;

	unlock(mqdes);
	return 0;

EXIT_ERROR:
	unlock(mqdes);
	return -1;
}

int mq_notify(mqd_t mqdes, const struct sigevent* notification)
{
	if (mqdes == NULL || mqdes->mq_magic != MQ_MAGIC)
	{
		errno = EBADF;
		return -1;
	}

	errno = EINVAL;
	return -1;
}
