// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_ANDROID_SOURCE_CHECK_UNISTD_H
#define MOL_ANDROID_SOURCE_CHECK_UNISTD_H

#if defined(__APPLE__)
#include <sys/unistd.h>
int usleep(unsigned int microseconds);
#else
typedef int clockid_t;
int usleep(unsigned int microseconds);
#endif

#endif
