#
# Copyright (c) 2012 Citrix Systems, Inc.
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
# 
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#

INCLUDES = @DBUS_CFLAGS@ @DBUS_GLIB_CFLAGS@

DBUS_CLIENT_IDLS=xenmgr db
DBUS_SERVER_IDLS=

SRCS= xcdbus.c version.c util.c
CPROTO=cproto

XCDBUSSRCS=${SRCS}

noinst_HEADERS= project.h prototypes.h xcdbus-tail.h ext_prototypes.h

libxcdbus_la_SOURCES = ${XCDBUSSRCS}

lib_LTLIBRARIES=libxcdbus.la

include_HEADERS=xcdbus.h

AM_CFLAGS=-g

libxcdbus_la_LDFLAGS = \
        -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
        -release $(LT_RELEASE) \
        -export-dynamic 

VFD=${srcdir}/..
VF=${shell cat ${VFD}/version-files}
VFS=${VF:%=${VFD}/%}
VCHK=${shell cat ${VFS} | @MD5SUM@ | @AWK@ '{print $$1 }' }
VNUM=${shell @GREP@ ${VCHK} ${VFD}/version-md5sums | @AWK@ '{ print $$2 }'  }
VDEF=${shell echo `cat ${VFD}/version-major`.`cat ${VFD}/version-minor`.`cat ${VFD}/version-micro` }


xcdbus.h:xcdbus-head.h ext_prototypes.h xcdbus-tail.h
	cat xcdbus-head.h ext_prototypes.h xcdbus-tail.h > xcdbus.h

protos:  ${BUILT_SOURCES}
	echo > prototypes.h
	echo > ext_prototypes.h
	cat xcdbus-head.h ext_prototypes.h xcdbus-tail.h > xcdbus.h
	${CPROTO} -v -DINT_PROTOS `echo "${INCLUDES}" | sed "s,[ ^]-[^I][^[:blank:]]*,,g"` ${SRCS} > prototypes.tmp
	mv -f prototypes.tmp prototypes.h
	${CPROTO} -v -DEXT_PROTOS `echo "${INCLUDES}" | sed "s,[ ^]-[^I][^[:blank:]]*,,g"` ${SRCS} > ext_prototypes.h
	cat xcdbus-head.h ext_prototypes.h xcdbus-tail.h > xcdbus.h

version.h: $(VFD)/version-files $(VFD)/version-major \
	$(VFD)/version-minor $(VFD)/version-micro \
	$(VFD)/version-md5sums ${VFS} Makefile
	if [ .${VNUM} = . ]; then \
		echo "#define FULL_VERSION \"libxcdbus Version ${VDEF} + Edits\"" > version.h; \
		echo ${VDEF}-E > version-num; \
	else \
		echo "#define FULL_VERSION \"libxcdbus Version ${VNUM}\"" > version.h; \
		echo ${VNUM} > version-num; \
	fi



BUILT_SOURCES = version.h \
	${DBUS_CLIENT_IDLS:%=rpcgen/%_client.h} \
	${DBUS_SERVER_IDLS:%=rpcgen/%_server_marshall.h} \
	${DBUS_SERVER_IDLS:%=rpcgen/%_server_obj.c} \
	${DBUS_SERVER_IDLS:%=rpcgen/%_server_obj.h} \
	version.h xcdbus.h


rpcgen/%_client.h: ${IDLDIR}/%.xml
	mkdir -p rpcgen
	if [ .${RPCGEN_TEMPLATES} = . ]; then \
		${XC_RPCGEN} -c -o rpcgen/ $<; \
	else \
		${XC_RPCGEN} --templates-dir=${RPCGEN_TEMPLATES} -c -o rpcgen/ $<; \
	fi


rpcgen/%_server_marshall.h rpcgen/%_server_obj.h rpcgen/%_server_obj.c : ${IDLDIR}/%.xml
	mkdir -p rpcgen
	if [ .${RPCGEN_TEMPLATES} = . ]; then \
		${XC_RPCGEN} -s -o rpcgen/ $<; \
	else \
		${XC_RPCGEN} --templates-dir=${RPCGEN_TEMPLATES} -s -o rpcgen/ $<; \
	fi

