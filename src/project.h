/*
 * Copyright (c) 2012 Citrix Systems, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __PROJECT_H__
#define __PROJECT_H__

#include "config.h"

#ifdef TM_IN_SYS_TIME
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#endif
#include <time.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#if defined(HAVE_STDINT_H)
#include <stdint.h>
#elif defined(HAVE_SYS_INT_TYPES_H)
#include <sys/int_types.h>
#endif

#ifdef HAVE_LIBEVENT
#include <event.h>
#endif

#ifdef INT_PROTOS
#define INTERNAL
#define EXTERNAL
#else 
#ifdef EXT_PROTOS
#define INTERNAL static
#define EXTERNAL
#else
#define INTERNAL
#define EXTERNAL
#endif
#endif

#include "xcdbus.h"

#define XCDBUS_FD_COND_READ 1
#define XCDBUS_FD_COND_WRITE 2
#define XCDBUS_FD_COND_EXCEPT 4

typedef int xcdbus_fdcond_t;

struct xcdbus_conn;

typedef struct {
    int fd;
    xcdbus_fdcond_t cond;
    DBusWatch *dbw;
    struct xcdbus_conn *c;
#ifdef HAVE_LIBEVENT
    struct event ev;
#endif
} xcdbus_watch_t;

#include "prototypes.h"

#endif /* __PROJECT_H__ */
