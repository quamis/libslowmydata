/* BEGIN LICENSE
 * Copyright (C) 2008-2014 Stewart Smith <stewart@flamingspork.com>
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 * END LICENSE */


/**
 * Use as:
 *
 * 	LD_PRELOAD=./.libs/libeatmydata.so LIBSLOWMYDATA_ON_OPEN_SLEEP=1.0 LIBSLOWMYDATA_ON_OPEN_SLEEP_IF_FNMATCH="*.*" cat ./conf
 *
 * */


#include "config.h"
#include "libeatmydata/portability.h"
#include "libeatmydata/visibility.h"

#undef _FILE_OFFSET_BITS // Hack to get open and open64 on 32bit
#undef __USE_FILE_OFFSET64
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <fnmatch.h>
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

/*
#define CHECK_FILE "/tmp/eatmydata"
*/

typedef int (*libc_open_t)(const char*, int, ...);
#ifdef HAVE_OPEN64
typedef int (*libc_open64_t)(const char*, int, ...);
#endif
typedef int (*libc_fsync_t)(int);
typedef int (*libc_sync_t)(void);
typedef int (*libc_fdatasync_t)(int);
typedef int (*libc_msync_t)(void*, size_t, int);
#ifdef HAVE_SYNC_FILE_RANGE
typedef int (*libc_sync_file_range_t)(int, off64_t, off64_t, unsigned int);
#endif
#if defined(F_FULLFSYNC) && defined(__APPLE__)
typedef int (*libc_fcntl_t)(int, int, ...);
#endif

/* All the following are thread-local, to avoid initialization races between
 * threads. */
static TLS int init_running = 0;
static TLS int init_complete = 0;
static TLS libc_open_t libc_open= NULL;
#ifdef HAVE_OPEN64
static TLS libc_open64_t libc_open64= NULL;
#endif

#define ASSIGN_DLSYM_OR_DIE(name)			\
        libc_##name = (libc_##name##_##t)(intptr_t)dlsym(RTLD_NEXT, #name);			\
        if (!libc_##name)                       \
        {                                       \
                const char *dlerror_str = dlerror();                          \
                fprintf(stderr, "libeatmydata init error for %s: %s\n", #name,\
                        dlerror_str ? dlerror_str : "(null)");                \
                _exit(1);                       \
        }

#define ASSIGN_DLSYM_IF_EXIST(name)			\
        libc_##name = (libc_##name##_##t)(intptr_t)dlsym(RTLD_NEXT, #name);			\
						   dlerror();

#pragma weak pthread_testcancel

float LIBSLOWMYDATA_ON_OPEN_SLEEP = 0.25;
char* LIBSLOWMYDATA_ON_OPEN_SLEEP_IF_FNMATCH = "*.*";

/**
 * @see https://stackoverflow.com/a/33412960/11301
 * */
int mssleep(long miliseconds)
{
   struct timespec rem;
   struct timespec req= {
       (int)(miliseconds / 1000),     /* secs (Must be Non-Negative) */
       (miliseconds % 1000) * 1000000 /* nano (Must be in range of 0 to 999999999) */
   };

   return nanosleep(&req , &rem);
}

int LIBEATMYDATA_API msync(void *addr, size_t length, int flags);

void __attribute__ ((constructor)) eatmydata_init(void);

void __attribute__ ((constructor)) eatmydata_init(void)
{
	init_running++;
	ASSIGN_DLSYM_OR_DIE(open);
#ifdef HAVE_OPEN64
	ASSIGN_DLSYM_OR_DIE(open64);
#endif
#if defined(F_FULLFSYNC) && defined(__APPLE__)
	ASSIGN_DLSYM_OR_DIE(fcntl);
#endif
	init_running--;
	init_complete++;

	LIBSLOWMYDATA_ON_OPEN_SLEEP = (getenv("LIBSLOWMYDATA_ON_OPEN_SLEEP")?atof(getenv("LIBSLOWMYDATA_ON_OPEN_SLEEP")):LIBSLOWMYDATA_ON_OPEN_SLEEP);
	LIBSLOWMYDATA_ON_OPEN_SLEEP_IF_FNMATCH = (getenv("LIBSLOWMYDATA_ON_OPEN_SLEEP_IF_FNMATCH")?getenv("LIBSLOWMYDATA_ON_OPEN_SLEEP_IF_FNMATCH"):LIBSLOWMYDATA_ON_OPEN_SLEEP_IF_FNMATCH);
}

int LIBEATMYDATA_API open(const char* pathname, int flags, ...)
{
	va_list ap;
	mode_t mode;

	va_start(ap, flags);
#if SIZEOF_MODE_T < SIZEOF_INT
	mode= (mode_t) va_arg(ap, int);
#else
	mode= va_arg(ap, mode_t);
#endif
	va_end(ap);

	/* If we get called recursively during initialization (which should
	 * be rare but might happen), just fail. */
	if (init_running > 0) {
		errno = EFAULT;
		return -1;
	}

	if(!init_complete)
		eatmydata_init();

	if (fnmatch(LIBSLOWMYDATA_ON_OPEN_SLEEP_IF_FNMATCH, pathname, 0)==0) {
		// printf("\n-> open(%s), [%s->%f]", pathname, LIBSLOWMYDATA_ON_OPEN_SLEEP_IF_FNMATCH, LIBSLOWMYDATA_ON_OPEN_SLEEP);
		mssleep(LIBSLOWMYDATA_ON_OPEN_SLEEP*1000);
	}

	return (*libc_open)(pathname,flags,mode);
}

#if !defined(__USE_FILE_OFFSET64) && defined(HAVE_OPEN64)

/*
 * Musl libc does this in `fcntl.h`:
 *
 *    #define open64 open
 *
 * It's hard to detect this situation, but we can avoid a compile failure
 * by undefining it.
 */
#undef open64

int LIBEATMYDATA_API open64(const char* pathname, int flags, ...)
{
	va_list ap;
	mode_t mode;

	va_start(ap, flags);
#if SIZEOF_MODE_T < SIZEOF_INT
	mode= (mode_t) va_arg(ap, int);
#else
	mode= va_arg(ap, mode_t);
#endif
	va_end(ap);

	/* If we get called recursively during initialization (which should
	 * be rare but might happen), just fail. */
	if (init_running > 0) {
		errno = EFAULT;
		return -1;
	}

	if(!init_complete)
		eatmydata_init();

	if (fnmatch(LIBSLOWMYDATA_ON_OPEN_SLEEP_IF_FNMATCH, pathname, 0)==0) {
		// printf("\n-> open(%s), [%s->%f]", pathname, LIBSLOWMYDATA_ON_OPEN_SLEEP_IF_FNMATCH, LIBSLOWMYDATA_ON_OPEN_SLEEP);
		mssleep(LIBSLOWMYDATA_ON_OPEN_SLEEP*1000);
	}

	return (*libc_open64)(pathname,flags,mode);
}
#endif

