/* A minimal libuv example. Reuses uv handles.
 *
 * 1. DNS resolve
 * 2. TCP connect
 * 3. TCP close
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <uv.h>


typedef struct ex_shared_buf_s {
  unsigned char buf[65536];
} ex_shared_buf_t;

typedef struct ex_conn_s {
  uv_loop_t         *loop;
  uv_tcp_t          conn;
  uv_timer_t        timeout;
  uv_connect_t      connector;
  uv_getaddrinfo_t  resolver;

  int               timeout_on;
  int               tcp_on;

  struct ex_shared_buf_s  rdbuf;

  struct addrinfo   *addrs;
  struct addrinfo   *addr_in_use;
  int               addr_needs_free;


  void *data;
} ex_conn_t;


int ex_start(ex_conn_t *conn);
int ex_conn_init(ex_conn_t *conn, uv_loop_t *loop);
int ex_conn_close(ex_conn_t *conn);
int ex_conn_free(ex_conn_t *conn);
void ex_resolve_cb(uv_getaddrinfo_t *req, int status, struct addrinfo *res);
void ex_connect_cb(uv_connect_t *handle, int status);


int
main(int argc, char *argv[]) {
  int rc = 0;
  uv_loop_t loop;
  ex_conn_t lconn;

  rc = uv_loop_init(&loop);
  assert(rc >= 0 && "failed at uv_loop_init()");

  rc = ex_conn_init(&lconn, &loop);
  assert(rc >= 0 && "failed at ex_conn_init()");

  for (int i = 0; i < 3; ++i) {
    rc = ex_start(&lconn);
    assert(rc >= 0 && "failed at ex_start()");

    rc = uv_run(&loop, UV_RUN_DEFAULT);
    assert(rc >= 0 && "failed at uv_run()");

    rc = ex_conn_close(&lconn);
    assert(rc >= 0 && "failed at ex_conn_close()");

    rc = uv_run(&loop, UV_RUN_DEFAULT);
    assert(rc >= 0 && "failed at uv_run()");
  }

  rc = ex_conn_free(&lconn);
  assert(rc >= 0 && "failed at ex_conn_free()");

  rc = uv_loop_close(&loop);
  if (rc < 0) {
    fprintf(stderr, "uv_loop_close(): (%d) %s\n", rc, uv_strerror(rc));
  }

  exit(EXIT_SUCCESS);
}

int
ex_start(ex_conn_t *conn) {
  int rc = 0;
  struct addrinfo hints;
  const char host[] = "broadcastlv.chat.bilibili.com";
  const char port[] = "2243";

  if (!conn)
    return -1;

  if (!conn->addrs) {
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    conn->resolver.data = conn;

    rc = uv_getaddrinfo(conn->loop, &conn->resolver, ex_resolve_cb, host, port, &hints);
    assert(rc >= 0 && "failed at uv_getaddrinfo()");
  } else {
    conn->tcp_on = 1;
    rc = uv_tcp_init(conn->loop, &conn->conn);
    assert(rc >= 0 && "failed at uv_tcp_init()");

    conn->conn.data = conn;
    conn->connector.data = conn;
    if (!conn->addr_in_use)
      conn->addr_in_use = conn->addrs;

    rc = uv_tcp_connect(&conn->connector, &conn->conn, conn->addr_in_use->ai_addr, ex_connect_cb);
    assert(rc >= 0 && "failed at uv_tcp_connect()");
  }

  return rc;
}

void
ex_resolve_cb(uv_getaddrinfo_t *req, int status, struct addrinfo *res) {
  int rc = 0;
  ex_conn_t *conn = req->data;

  if (status < 0) {
    fprintf(stderr, "(%p) ex_resolve_cb(): (%d) %s\n", conn, status, uv_strerror(status));
    return;
  }

  conn->addrs = res;
  conn->addr_needs_free = 1;
  rc = ex_start(conn);
  assert(rc >= 0 && "failed at ex_start()");
}

void ex_connect_cb(uv_connect_t *handle, int status) {
  ex_conn_t *conn = handle->data;

  if (status < 0) {
    fprintf(stderr, "(%p) ex_connect_cb: (%d) %s\n", conn, status, uv_strerror(status));
    return;
  }
  printf("Connected\n");
}

int
ex_conn_init(ex_conn_t *conn, uv_loop_t *loop) {
  if (!conn)
    return -1;

  conn->addrs = NULL;
  conn->addr_in_use = NULL;
  conn->addr_needs_free = 0;
  conn->loop = loop;
  conn->tcp_on = 0;
  conn->timeout_on = 0;
  return 0;
}

int
ex_conn_close(ex_conn_t *conn) {
  if (!conn)
    return 0;

  if (conn->tcp_on) {
    conn->tcp_on = 0;
    if (!uv_is_closing((uv_handle_t *)&conn->conn))
      uv_close((uv_handle_t *)&conn->conn, NULL);
  }
  if (conn->timeout_on) {
    conn->timeout_on = 0;
    if (!uv_is_closing((uv_handle_t *)&conn->timeout))
      uv_close((uv_handle_t *)&conn->timeout, NULL);
  }
  return 0;
}

int
ex_conn_free(ex_conn_t *conn) {
  if (!conn)
    return 0;

  if (conn->addr_needs_free)
    uv_freeaddrinfo(conn->addrs);

  conn->addrs = NULL;
  conn->addr_in_use = NULL;
  conn->addr_needs_free = 0;
  conn->loop = NULL;
  return 0;
}
