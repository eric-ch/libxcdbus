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

/* xcdbus.c */
xcdbus_conn_t *xcdbus_of_conn(void *c);
xcdbus_conn_t *xcdbus_init(const char *service_name);
xcdbus_conn_t *xcdbus_init2(const char *service_name, DBusGConnection *connG);
xcdbus_conn_t *xcdbus_init_with_gloop(const char *service_name, DBusGConnection *conn, GMainLoop *loop);
xcdbus_conn_t *xcdbus_init_event(const char *service_name, DBusGConnection *connG);
DBusGConnection *xcdbus_get_dbus_glib_connection(xcdbus_conn_t *c);
DBusConnection *xcdbus_get_dbus_connection(xcdbus_conn_t *c);
void xcdbus_shutdown(xcdbus_conn_t *c);
int xcdbus_name_has_owner(xcdbus_conn_t *c, const char *service);
void xcdbus_wait_service(xcdbus_conn_t *c, const char *service);
int xcdbus_broadcast_signal(xcdbus_conn_t *c, const char *object_path, const char *interface, const char *member, const char *data);
const char *xcdbus_get_sender(xcdbus_conn_t *xc);
int32_t xcdbus_get_sender_domid(xcdbus_conn_t *xc);
int xcdbus_dispatch(xcdbus_conn_t *xc);
int xcdbus_pre_select(xcdbus_conn_t *c, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds);
void xcdbus_post_select(xcdbus_conn_t *c, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds);
int xcdbus_db_daemon_online(xcdbus_conn_t *conn);
int xcdbus_read_db(xcdbus_conn_t *c, const char *path, char *buf, int buf_size);
int xcdbus_write_db(xcdbus_conn_t *c, const char *path, const char *value);
int xcdbus_xenmgr_online(xcdbus_conn_t *c);
int xcdbus_xenmgr_list_domids(xcdbus_conn_t *c, int32_t *out_domids, size_t out_domids_bufsz, int *out_num_domains);
int xcdbus_input_online(xcdbus_conn_t *conn);
int xcdbus_input_get_focus_domid(xcdbus_conn_t *c, int32_t *out_domid);
int xcdbus_merge_fds(xcdbus_conn_t *c, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds);
void xcdbus_process_fds(xcdbus_conn_t *c, int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds);
void xcdbus_free(xcdbus_conn_t *c);
DBusGProxy *xcdbus_get_proxy(xcdbus_conn_t *c, const char *service, const char *objpath, const char *interface);
int xcdbus_get_property_var(xcdbus_conn_t *c, const char *service, const char *objpath, const char *interface, const char *property, GValue *outv);
int xcdbus_set_property_var(xcdbus_conn_t *c, const char *service, const char *objpath, const char *interface, const char *property, GValue *inpv);
int xcdbus_get_property_string(xcdbus_conn_t *c, const char *service, const char *objpath, const char *interface, const char *property, char **outv);
int xcdbus_set_property_string(xcdbus_conn_t *c, const char *service, const char *objpath, const char *interface, const char *property, const char *inpv);
int xcdbus_get_property_bool(xcdbus_conn_t *c, const char *service, const char *objpath, const char *interface, const char *property, gboolean *outv);
int xcdbus_set_property_bool(xcdbus_conn_t *c, const char *service, const char *objpath, const char *interface, const char *property, gboolean inpv);
int xcdbus_get_property_int(xcdbus_conn_t *c, const char *service, const char *objpath, const char *interface, const char *property, gint *outv);
int xcdbus_set_property_int(xcdbus_conn_t *c, const char *service, const char *objpath, const char *interface, const char *property, gint inpv);
int xcdbus_get_property_uint(xcdbus_conn_t *c, const char *service, const char *objpath, const char *interface, const char *property, guint *outv);
int xcdbus_set_property_uint(xcdbus_conn_t *c, const char *service, const char *objpath, const char *interface, const char *property, guint inpv);
int xcdbus_get_property_int64(xcdbus_conn_t *c, const char *service, const char *objpath, const char *interface, const char *property, gint64 *outv);
int xcdbus_set_property_int64(xcdbus_conn_t *c, const char *service, const char *objpath, const char *interface, const char *property, gint64 inpv);
int xcdbus_get_property_uint64(xcdbus_conn_t *c, const char *service, const char *objpath, const char *interface, const char *property, guint64 *outv);
int xcdbus_set_property_uint64(xcdbus_conn_t *c, const char *service, const char *objpath, const char *interface, const char *property, guint64 inpv);
int xcdbus_get_property_double(xcdbus_conn_t *c, const char *service, const char *objpath, const char *interface, const char *property, gdouble *outv);
int xcdbus_set_property_double(xcdbus_conn_t *c, const char *service, const char *objpath, const char *interface, const char *property, gdouble inpv);
int xcdbus_get_property_byte(xcdbus_conn_t *c, const char *service, const char *objpath, const char *interface, const char *property, unsigned char *outv);
int xcdbus_set_property_byte(xcdbus_conn_t *c, const char *service, const char *objpath, const char *interface, const char *property, unsigned char inpv);
/* version.c */
char *xcdbus_get_version(void);
/* util.c */
void *xcdbus_xmalloc(size_t s);
void *xcdbus_xrealloc(void *p, size_t s);
void *xcdbus_xfree(void *p);
