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

#include "project.h"
#include "rpcgen/db_client.h"
#include "rpcgen/xenmgr_client.h"

static char rcsid[] = "$Id:$";

static const char *DB_SERVICE = "com.citrix.xenclient.db";
static const char *DB_INTERFACE = "com.citrix.xenclient.db";

static const char *XENMGR_SERVICE = "com.citrix.xenclient.xenmgr";
static const char *XENMGR_OBJ = "/";

static const char *INPUT_SERVICE = "com.citrix.xenclient.input";
static const char *INPUT_OBJ = "/";
static const char *INPUT_INTERFACE = "com.citrix.xenclient.input";

#define BLOCKING_TIMEOUT 5000

typedef struct proxyentry {
    DBusGProxy *proxy;
    DBusGConnection *conn;
    const char *service;
    const char *objpath;
    const char *interface;
} proxyentry_t;

struct xcdbus_conn {
    DBusGConnection *connG;
    DBusConnection  *conn;
    xcdbus_watch_t  *watches;
    int nwatches;
    int dispatching;
    int gloop;
    char sender[16];
};

static proxyentry_t *proxy_entries = NULL;
static int num_proxy_entries = 0;

static xcdbus_conn_t **connections = NULL;
static int num_connections = 0;

static xcdbus_watch_t *
find_watch_by_fd (xcdbus_conn_t * c, int fd, int create)
{
  int i;
  for (i = 0; i < c->nwatches; ++i)
    {
      if (c->watches[i].fd == fd)
        return &c->watches[i];
    }

  if (!create)
    return NULL;

  c->nwatches++;

  c->watches =
    (xcdbus_watch_t *) xcdbus_xrealloc (c->watches,
                                       sizeof (xcdbus_watch_t) * c->nwatches);

  memset (&c->watches[i], 0, sizeof (xcdbus_watch_t));
  c->watches[i].fd = fd;
  c->watches[i].c = c;

  return &c->watches[i];
}

static dbus_bool_t
watch_add (DBusWatch * watch, void *_c)
{
  xcdbus_conn_t *c = (xcdbus_conn_t *) _c;
  int fd = dbus_watch_get_unix_fd (watch);

  if (dbus_watch_get_enabled (watch))
    {
      xcdbus_watch_t *w = find_watch_by_fd (c, fd, 1);
      int flags = dbus_watch_get_flags (watch);
      if (flags & DBUS_WATCH_READABLE)
          w->cond |= XCDBUS_FD_COND_READ;
      if (flags & DBUS_WATCH_WRITABLE)
          w->cond |= XCDBUS_FD_COND_WRITE;

      /* integrate watch with select loop */
      if (w->cond) {
          w->dbw = watch;
      }
    }

  return TRUE;
}

static void
watch_remove (DBusWatch * watch, void *_c)
{
  xcdbus_conn_t *c = (xcdbus_conn_t *) _c;
  int fd = dbus_watch_get_unix_fd (watch);
  int cond = 0;
  xcdbus_watch_t *w = find_watch_by_fd (c, fd, 0);
  if (!w) return;

  int flags = dbus_watch_get_flags(watch);
  if (flags & DBUS_WATCH_READABLE) cond |= XCDBUS_FD_COND_READ;
  if (flags & DBUS_WATCH_WRITABLE) cond |= XCDBUS_FD_COND_WRITE;
  /* deintegrate from select loop */
  w->cond &= ~cond;
  if (!w->cond) {
      w->dbw = NULL;
  }
}

static void
watch_toggle (DBusWatch * watch, void *data)
{
    if (dbus_watch_get_enabled (watch)) {
        watch_add (watch, data);
    }
    else {
        watch_remove (watch, data);
    }
}

static void
watch_process (xcdbus_conn_t * c, DBusWatch * watch, int flags_to_process)
{
  if (!watch)
    return;                     /* why ? */

  dbus_watch_handle (watch, flags_to_process);
  dbus_connection_ref (c->conn);

  xcdbus_dispatch(c);

  dbus_connection_unref (c->conn);
}

/* xcdbus_conn_t* of either DBusConnection*, DBusGConnection* or xcdbus_conn_t* */
EXTERNAL
xcdbus_conn_t *xcdbus_of_conn(void *c)
{
    int i;
    for ( i = 0; i < num_connections; ++i ) {
        xcdbus_conn_t *xc = connections[i];
        if (xc == c || xc->conn == c || xc->connG == c) {
            return xc;
        }
    }
    return NULL;
}

static xcdbus_conn_t *xcdbus_init_common(const char *service_name, DBusGConnection *connG, int gloop)
{
  DBusError error;
  xcdbus_conn_t *c = NULL;
  int ret = 0;

  DBusConnection *conn = (DBusConnection*) dbus_g_connection_get_connection(connG);

  if (service_name) {
      dbus_error_init (&error);
      ret =
          dbus_bus_request_name (conn, service_name, DBUS_NAME_FLAG_DO_NOT_QUEUE,
                                 &error);
      if (ret == -1) {
          dbus_error_free (&error);
          return NULL;
      }
      dbus_error_free (&error);
  }

  c = xcdbus_xmalloc (sizeof (xcdbus_conn_t));
  memset (c, 0, sizeof (*c));

  c->connG = connG;
  c->conn = conn;
  c->watches = xcdbus_xmalloc (1);
  c->nwatches = 0;
  c->dispatching = 0;
  c->gloop = gloop;

  ++num_connections;
  connections = realloc( connections, num_connections * sizeof(xcdbus_conn_t) );
  connections[num_connections-1] = c;
  return c;
}

EXTERNAL xcdbus_conn_t *
xcdbus_init(const char *service_name)
{
  DBusGConnection *conn = NULL;

  conn = dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL);
  if (!conn) {
      return NULL;
  }
  return xcdbus_init2(service_name, conn);
}

EXTERNAL xcdbus_conn_t * 
xcdbus_init2(const char *service_name, DBusGConnection *connG)
{
  xcdbus_conn_t *c = xcdbus_init_common(service_name, connG, 0);

  /* setup watching */
  dbus_connection_set_watch_functions (
      c->conn,
      watch_add,
      watch_remove, watch_toggle, c, NULL
  );

  return c;
}

/* any of the arguments can be NULL */
EXTERNAL xcdbus_conn_t *
xcdbus_init_with_gloop(const char *service_name, DBusGConnection *conn, GMainLoop *loop)
{
    int gloop=0;
    if (!conn) {
        conn = dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL);
    }
    if (!conn) {
        return NULL;
    }
    if (loop) {
        gloop=1;
        dbus_connection_setup_with_g_main(dbus_g_connection_get_connection(conn), g_main_loop_get_context(loop));
    }
    /* DO NOT SETUP WATCH functions here, we assume glib's main loop takes care of that */
    return xcdbus_init_common(service_name, conn, gloop);
}

#ifdef HAVE_LIBEVENT
static void
watch_process (xcdbus_conn_t * c, DBusWatch * watch, int flags_to_process);

static void
event_cb(int fd, short ev_type, void *priv)
{
  xcdbus_watch_t *w = (xcdbus_watch_t *)priv;
  int watch_flags = 0;

  if (ev_type & EV_READ)
    {
      watch_flags |= DBUS_WATCH_READABLE;
    }
  if (ev_type & EV_WRITE)
    {
      watch_flags |= DBUS_WATCH_WRITABLE;
    }

  if (watch_flags) {
      watch_process (w->c, w->dbw, watch_flags);
  }
}

static dbus_bool_t
watch_add_event (DBusWatch * watch, void *_c)
{
  xcdbus_conn_t *c = (xcdbus_conn_t *) _c;
  int fd = dbus_watch_get_unix_fd (watch);

  if (dbus_watch_get_enabled (watch))
    {
      xcdbus_watch_t *w = find_watch_by_fd (c, fd, 1);
      int flags = dbus_watch_get_flags (watch);
      short ev_type = EV_PERSIST;

      if (flags & DBUS_WATCH_READABLE) {
          ev_type |= EV_READ;
          w->cond |= XCDBUS_FD_COND_READ;
      }
      if (flags & DBUS_WATCH_WRITABLE) {
          ev_type |= EV_WRITE;
          w->cond |= XCDBUS_FD_COND_WRITE;
      }

      /* integrate watch with select loop */
      if (w->cond) {
          w->dbw = watch;
      }

      event_set(&w->ev, w->fd, ev_type, event_cb, w);
      event_add(&w->ev, NULL);
    }

  return TRUE;
}

static void
watch_remove_event (DBusWatch * watch, void *_c)
{
  xcdbus_conn_t *c = (xcdbus_conn_t *) _c;
  int fd = dbus_watch_get_unix_fd (watch);
  int cond = 0;
  xcdbus_watch_t *w = find_watch_by_fd (c, fd, 0);
  if (!w) return;

  int flags = dbus_watch_get_flags(watch);
  if (flags & DBUS_WATCH_READABLE) cond |= XCDBUS_FD_COND_READ;
  if (flags & DBUS_WATCH_WRITABLE) cond |= XCDBUS_FD_COND_WRITE;
  /* deintegrate from select loop */
  w->cond &= ~cond;
  if (!w->cond) {
      w->dbw = NULL;
  }
  event_del(&w->ev);
}

static void
watch_toggle_event (DBusWatch * watch, void *data)
{
    if (dbus_watch_get_enabled (watch)) {
        watch_add_event (watch, data);
    }
    else {
        watch_remove_event (watch, data);
    }
}

EXTERNAL xcdbus_conn_t *
xcdbus_init_event(const char *service_name, DBusGConnection *connG)
{
  xcdbus_conn_t *c = xcdbus_init_common(service_name, connG, 0);
  /* setup watching */
  dbus_connection_set_watch_functions (
      c->conn,
      watch_add_event,
      watch_remove_event,
      watch_toggle_event,
      c, NULL);

  return c;
}
#else /* !HAVE_LIBEVENT */
EXTERNAL xcdbus_conn_t *
xcdbus_init_event(const char *service_name, DBusGConnection *connG)
{
  return NULL;
}
#endif

EXTERNAL DBusGConnection *xcdbus_get_dbus_glib_connection(xcdbus_conn_t *c)
{
    return c->connG;
}

EXTERNAL DBusConnection *xcdbus_get_dbus_connection(xcdbus_conn_t * c)
{
  return c->conn;
}

EXTERNAL void
xcdbus_shutdown (xcdbus_conn_t * c)
{
  if (!c)
    return;

  if (c->watches)
    xcdbus_xfree (c->watches);

  xcdbus_xfree (c);
}

/* test if service of given name is published on dbus already */
EXTERNAL int
xcdbus_name_has_owner (xcdbus_conn_t * c, const char *service)
{
  DBusMessage *msg, *reply;
  int has_owner = 0;
  DBusMessageIter args;
  msg = dbus_message_new_method_call ("org.freedesktop.DBus",
                                      "/org/freedesktop/DBus",
                                      "org.freedesktop.DBus", "NameHasOwner");
  if (!msg)
    return 0;

  dbus_message_iter_init_append (msg, &args);
  if (!dbus_message_iter_append_basic (&args, DBUS_TYPE_STRING, &service))
    {
      dbus_message_unref (msg);
      return 0;
    }
  reply =
    dbus_connection_send_with_reply_and_block (c->conn, msg, BLOCKING_TIMEOUT,
                                               NULL);
  if (!reply)
    {
      dbus_message_unref (msg);
      return 0;
    }
  dbus_message_get_args (reply, NULL, DBUS_TYPE_BOOLEAN, &has_owner,
                         DBUS_TYPE_INVALID);
  dbus_message_unref (msg);
  dbus_message_unref (reply);

  return has_owner;
}

/* wait until stuff appears on dbus */
EXTERNAL void
xcdbus_wait_service (xcdbus_conn_t * c, const char *service)
{
  /* FIXME: add timeout + logs */
  while (!xcdbus_name_has_owner (c, service))
    {
      struct timeval tv = { 0 };
      tv.tv_sec = 1;
      select (0, NULL, NULL, NULL, &tv);
    }
}

/* send a simple signal with one string parameter attached if data != NULL */
EXTERNAL int xcdbus_broadcast_signal (
    xcdbus_conn_t *c,
    const char *object_path,
    const char *interface,
    const char *member,
    /* can be NULL */
    const char *data)
{
    DBusMessage *msg;
    DBusMessageIter args;
    msg = dbus_message_new_signal(object_path, interface, member);
    if (!msg) {
        return 0;
    }
    if (data) {
        dbus_message_iter_init_append(msg, &args);
        if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &data)) { 
            dbus_message_unref(msg);
            return 0;
        }
    }
    if (!dbus_connection_send(c->conn, msg, NULL)) {
        dbus_message_unref(msg);
        return 0;
    }
    dbus_connection_flush(c->conn);
    dbus_message_unref(msg);
    return 1;
}

EXTERNAL const char*
xcdbus_get_sender (xcdbus_conn_t *xc)
{
    return xc->sender;
}

EXTERNAL int32_t
xcdbus_get_sender_domid (xcdbus_conn_t *xc)
{
    DBusMessage *msg, *reply;
    DBusMessageIter args;
    int32_t domid = -1;
    const char *sender = xc->sender;
    if (sender[0] == 0)
        return -1;

    msg = dbus_message_new_method_call( "org.freedesktop.DBus",
                                        "/org/freedesktop/DBus",
                                        "org.freedesktop.DBus",
                                        "GetConnectionDOMID" );
    if (!msg)
        goto error;
    dbus_message_iter_init_append (msg, &args);
    if (!dbus_message_iter_append_basic (&args, DBUS_TYPE_STRING, &sender))
        goto error;
    reply = dbus_connection_send_with_reply_and_block(xc->conn, msg, BLOCKING_TIMEOUT, NULL);
    if (!reply)
        goto error;
    dbus_message_get_args(reply, NULL, DBUS_TYPE_INT32, &domid, DBUS_TYPE_INVALID);

    dbus_message_unref(msg);
    dbus_message_unref(reply);
    return domid;

error:
    if (msg) dbus_message_unref(msg);
    if (reply) dbus_message_unref(reply);
    return -1;
}

EXTERNAL void
xcdbus_set_error(GError** err, const char* interface, const char* error, const char *fmt, ...)
{
   DBusError e;
   char* err_int = alloca(strlen(interface) + strlen(error)+2);
   char* buf;
   int len;
   va_list argptr;

   va_start(argptr, fmt);
   len = vsnprintf(NULL,0, fmt, argptr);
   va_end(argptr);

   buf = alloca(len+1);
   va_start(argptr, fmt);
   vsnprintf(buf,len+1, fmt, argptr);
   va_end(argptr);

   sprintf(err_int, "%s.%s",interface, error);
   dbus_set_error(&e, err_int,"%s", buf);
   dbus_set_g_error(err, &e);
}

EXTERNAL int
xcdbus_dispatch (xcdbus_conn_t *xc)
{
    DBusMessage *m  = NULL;
    if (!xc) {
        return 0;
    }
    if (xc->gloop) {
        /* no manual dispatching if using glib's main loop */
        return 0;
    }
    /* fixup accidental usage of other pointer type */
    xc = xcdbus_of_conn(xc);
    /* avoid recursive dispatching, as it blocks forever */
    if (!xc || xc->dispatching) {
        return 0;
    }
    xc->dispatching = 1;

    for (;;) {
        const char *sender;
        m = dbus_connection_borrow_message(xc->conn);
        if (!m)
            break;
        sender = dbus_message_get_sender(m);
        if (!sender) {
            xc->sender[0] = 0;
        } else {
            strncpy(xc->sender, sender, sizeof(xc->sender));
        }
        dbus_connection_return_message(xc->conn, m);
        /* dispatches at most 1 message according to dbus doc */
        dbus_connection_dispatch(xc->conn);
    }
    xc->dispatching = 0;
    return 0;
}

/* call before waiting on select(), returns modified number of file descriptors */
EXTERNAL int
xcdbus_pre_select (xcdbus_conn_t * c, int nfds, fd_set * readfds,
                   fd_set * writefds, fd_set * exceptfds)
{
  int i;

  /* dispatch remaining data */
  xcdbus_dispatch(c);

  for (i = 0; i < c->nwatches; ++i)
    {
      xcdbus_watch_t *w = &c->watches[i];
      if (!w->cond)
        continue;

      if (w->fd >= nfds)
        nfds = w->fd + 1;

      if (readfds && (w->cond & XCDBUS_FD_COND_READ))
        FD_SET (w->fd, readfds);

      if (writefds && (w->cond & XCDBUS_FD_COND_WRITE))
        FD_SET (w->fd, writefds);

      if (exceptfds && (w->cond & XCDBUS_FD_COND_EXCEPT))
        FD_SET (w->fd, exceptfds);

    }

  return nfds;
}

/* call after select() to let dbus handle filedescriptors */
EXTERNAL void
xcdbus_post_select (xcdbus_conn_t * c, int nfds, fd_set * readfds,
                    fd_set * writefds, fd_set * exceptfds)
{
  int watch_flags;
  int i;
  for (i = 0; i < c->nwatches; ++i)
    {
      xcdbus_watch_t *w = &c->watches[i];
      if (!w->cond)
        continue;

      watch_flags = 0;

      /* was listening to something, now get a wakeup condition from fd set */
      if (readfds && FD_ISSET (w->fd, readfds))
        {
          watch_flags |= DBUS_WATCH_READABLE;
        }
      if (writefds && FD_ISSET (w->fd, writefds))
        {
          watch_flags |= DBUS_WATCH_WRITABLE;
        }
      if (exceptfds && FD_ISSET (w->fd, exceptfds))
        {
          /* FIXME: do something here */
        }

      if (watch_flags)
        watch_process (c, w->dbw, watch_flags);
    }
}

/*
 * Check if database demon RPC service is up
 */
EXTERNAL int
xcdbus_db_daemon_online(xcdbus_conn_t *conn)
{
    return xcdbus_name_has_owner(conn, DB_SERVICE);
}

/*
 * Read value from config database. Returns 0 on RPC error.
 * Returns 1 otherwise. If database node does not exist, returns 1
 * and buf is filled with empty string
 */
EXTERNAL int
xcdbus_read_db(xcdbus_conn_t *c, const char *path, char *buf, int buf_size)
{
    char *value = NULL;
    if (!com_citrix_xenclient_db_read_(c, DB_SERVICE, "/", path, &value)) {
        return FALSE;
    }
    strncpy(buf, value, buf_size);
    free(value);
    return TRUE;
}

/*
 * Write value to database. Returns 0 on RPC fail. Node is created if it
 * doesn't exist already
 */
EXTERNAL int
xcdbus_write_db(xcdbus_conn_t *c, const char *path, const char *value)
{
    if (!com_citrix_xenclient_db_write_(c, DB_SERVICE, "/", path, value)) {
        return FALSE;
    }
    return TRUE;
}

/*
 * Check if xenmgr service is online
 */
EXTERNAL int
xcdbus_xenmgr_online(xcdbus_conn_t *c)
{
    return xcdbus_name_has_owner(c, XENMGR_SERVICE);
}

/*
 * Get list of active domain ids from xenmgr. Return 0 on RPC problem. Set num_domains to number
 * of domids written.
 */
EXTERNAL int
xcdbus_xenmgr_list_domids(xcdbus_conn_t *c, int32_t *out_domids, size_t out_domids_bufsz, int *out_num_domains)
{
    GArray *arr = NULL;
    int i = 0;
    *out_num_domains = 0;

    if (!com_citrix_xenclient_xenmgr_list_domids_(c, XENMGR_SERVICE, XENMGR_OBJ, &arr)) {
        return FALSE;
    }

    for (i = 0; i < arr->len && i < out_domids_bufsz / sizeof(int32_t); ++i) {
        *out_domids++ = g_array_index(arr, gint, i);
    }
    *out_num_domains = i;

    g_array_free(arr, TRUE);

    return TRUE;
}

/*
 * Check if input demon RPC service is up
 */
EXTERNAL int
xcdbus_input_online(xcdbus_conn_t *conn)
{
    return xcdbus_name_has_owner(conn, INPUT_SERVICE);
}

/*
 * Get focused domain id from input demon. Return 0 on RPC error
 */
EXTERNAL int
xcdbus_input_get_focus_domid(xcdbus_conn_t *c, int32_t *out_domid)
{
    DBusMessage *msg, *reply;
    DBusMessageIter args;
    *out_domid = 0;
    msg = dbus_message_new_method_call(
        INPUT_SERVICE,
        INPUT_OBJ,
        INPUT_INTERFACE,
        "get_focus_domid");
    if (!msg)
        goto error;
    reply = dbus_connection_send_with_reply_and_block(c->conn, msg, BLOCKING_TIMEOUT, NULL);
    if (!reply)
        goto error;
    dbus_message_get_args(reply, NULL, DBUS_TYPE_INT32, out_domid, DBUS_TYPE_INVALID);

    dbus_message_unref(msg);
    dbus_message_unref(reply);
    return 1;

error:
    if (msg) dbus_message_unref(msg);
    if (reply) dbus_message_unref(reply);
    return 0;
}


EXTERNAL int xcdbus_merge_fds (xcdbus_conn_t * c, int nfds, fd_set * readfds,
                               fd_set * writefds, fd_set * exceptfds)
  __attribute__ ((error
                  ("please use xcdbus_pre_select rather than xcdbus_merge_fds")));

EXTERNAL int xcdbus_merge_fds (xcdbus_conn_t * c, int nfds, fd_set * readfds,
                               fd_set * writefds, fd_set * exceptfds)
{
  return 0;
}

EXTERNAL void xcdbus_process_fds (xcdbus_conn_t * c, int nfds,
                                  fd_set * readfds, fd_set * writefds,
                                  fd_set * exceptfds)
  __attribute__ ((error
                  ("please use xcdbus_post_select rather than xcdbus_process_fds")));
EXTERNAL void xcdbus_process_fds (xcdbus_conn_t * c, int nfds,
                                  fd_set * readfds, fd_set * writefds,
                                  fd_set * exceptfds)
{
}

EXTERNAL void xcdbus_free (xcdbus_conn_t * c)
  __attribute__ ((error
                  ("please use xcdbus_shutdown rather than xcdbus_free")));
EXTERNAL void xcdbus_free (xcdbus_conn_t * c)
{
}

EXTERNAL DBusGProxy*
xcdbus_get_proxy(xcdbus_conn_t *c, const char *service, const char *objpath, const char *interface)
{
    int i;
    /* fixup accidental usage of other pointer type */
    DBusGConnection *conn = xcdbus_of_conn(c)->connG;

    for (i = 0; i < num_proxy_entries; ++i) {
        proxyentry_t *e = &proxy_entries[i];
        if (e->conn == conn &&
            !strcmp(e->service, service) &&
            !strcmp(e->objpath, objpath) &&
            !strcmp(e->interface, interface))
        {
            return e->proxy;
        }
    }
    ++num_proxy_entries;
    proxy_entries = realloc( proxy_entries, num_proxy_entries * sizeof(struct proxyentry) );
    proxyentry_t *e = &proxy_entries[num_proxy_entries-1];
    e->conn = conn;
    e->service = strdup(service);
    e->objpath = strdup(objpath);
    e->interface = strdup(interface);
    e->proxy = dbus_g_proxy_new_for_name(conn, service, objpath, interface);
    return e->proxy;
}


EXTERNAL int
xcdbus_get_property_var(
    xcdbus_conn_t *c,
    const char *service,
    const char *objpath,
    const char *interface,
    const char *property,
    GValue *outv)
{
    GError *error = NULL;
    GValue v = { 0, 0 };
    DBusGProxy *p = xcdbus_get_proxy(c, service, objpath, "org.freedesktop.DBus.Properties");

    if (!p) {
        return 0;
    }
    if (!dbus_g_proxy_call(
            p, "Get", &error,
            G_TYPE_STRING, interface, G_TYPE_STRING, property, DBUS_TYPE_INVALID,
            G_TYPE_VALUE, &v, DBUS_TYPE_INVALID ))
    {
        return 0;
    }
    *outv = v;
    return 1;
}


EXTERNAL int
xcdbus_set_property_var(
    xcdbus_conn_t *c,
    const char *service,
    const char *objpath,
    const char *interface,
    const char *property,
    GValue *inpv)
{
    GError *error = NULL;
    DBusGProxy *p = xcdbus_get_proxy(c, service, objpath, "org.freedesktop.DBus.Properties");

    if (!p) {
        return 0;
    }
    if (!dbus_g_proxy_call(
            p, "Set", &error,
            G_TYPE_STRING, interface, G_TYPE_STRING, property,
            G_TYPE_VALUE, inpv, DBUS_TYPE_INVALID, DBUS_TYPE_INVALID ))
    {
        return 0;
    }
    return 1;
}

#define stub_pget(name, typ, gtyp, gvalget)      \
int \
xcdbus_get_property_##name( \
    xcdbus_conn_t *c, \
    const char *service, \
    const char *objpath, \
    const char *interface, \
    const char *property, \
    typ *outv) \
{ \
    GValue var; \
    if (!xcdbus_get_property_var(c,service,objpath,interface,property,&var)) { \
        return 0; \
    } \
    if (G_VALUE_HOLDS(&var, gtyp)) { \
        *outv = gvalget(&var); \
        g_value_unset(&var); \
        return 1; \
    } \
    g_value_unset(&var); \
    return 0; \
}

#define stub_pset(name, typ, gtyp, gvalset)      \
int \
xcdbus_set_property_##name( \
    xcdbus_conn_t *c, \
    const char *service, \
    const char *objpath, \
    const char *interface, \
    const char *property, \
    typ inpv) \
{ \
    int r; \
    GValue var = {0,0}; \
    g_value_init( &var, gtyp ); \
    gvalset( &var, inpv ); \
    r = xcdbus_set_property_var(c,service,objpath,interface,property,&var); \
    g_value_unset( &var ); \
    return r; \
}

static const char* dup_gval_str(GValue *v)
{
    return strdup(g_value_get_string(v));
}

EXTERNAL stub_pget(string, char*, G_TYPE_STRING, dup_gval_str);
EXTERNAL stub_pset(string, const char*, G_TYPE_STRING, g_value_set_string);

EXTERNAL stub_pget(bool, gboolean, G_TYPE_BOOLEAN, g_value_get_boolean);
EXTERNAL stub_pset(bool, gboolean, G_TYPE_BOOLEAN, g_value_set_boolean);

EXTERNAL stub_pget(int, gint, G_TYPE_INT, g_value_get_int);
EXTERNAL stub_pset(int, gint, G_TYPE_INT, g_value_set_int);

EXTERNAL stub_pget(uint, guint, G_TYPE_UINT, g_value_get_uint);
EXTERNAL stub_pset(uint, guint, G_TYPE_UINT, g_value_set_uint);

EXTERNAL stub_pget(int64, gint64, G_TYPE_INT64, g_value_get_int64);
EXTERNAL stub_pset(int64, gint64, G_TYPE_INT64, g_value_set_int64);

EXTERNAL stub_pget(uint64, guint64, G_TYPE_UINT64, g_value_get_uint64);
EXTERNAL stub_pset(uint64, guint64, G_TYPE_UINT64, g_value_set_uint64);

EXTERNAL stub_pget(double, gdouble, G_TYPE_DOUBLE, g_value_get_double);
EXTERNAL stub_pset(double, gdouble, G_TYPE_DOUBLE, g_value_set_double);

EXTERNAL stub_pget(byte, unsigned char, G_TYPE_UCHAR, g_value_get_uchar);
EXTERNAL stub_pset(byte, unsigned char, G_TYPE_UCHAR, g_value_set_uchar);
