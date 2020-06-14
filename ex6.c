/* A minimal libuv example.
 *
 * 1. DNS resolve
 * 2. TCP connect
 * 3. TCP write
 * 4. TCP read
 * 5. TCP close
 *
 * Note: The TCP Connection should normally stop in about 1 minute. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <uv.h>

typedef struct ex_write_req_s {
  uv_write_t writer;
  uv_buf_t buf;
  int needs_free;
} ex_write_req_t;

typedef struct ex_liveconn_s {
  uv_loop_t *loop;
  uv_tcp_t *conn;
  uv_connect_t *connector;
  uv_shutdown_t *closer;
  uv_getaddrinfo_t *resolver;
  uint8_t *read_buf;
  ssize_t read_buf_len;
  struct addrinfo *addrs;
  struct addrinfo *addr_in_use;
} ex_liveconn_t;

int liveconn_init(uv_loop_t* loop, ex_liveconn_t *conn);
int liveconn_free(ex_liveconn_t *liveconn);
int liveconn_start(ex_liveconn_t *liveconn);
int liveconn_connect(ex_liveconn_t *liveconn);
int liveconn_close(ex_liveconn_t *liveconn);
void on_close(uv_handle_t *handle);
void on_dns_resolve(uv_getaddrinfo_t *info, int status, struct addrinfo *res);
void on_tcp_connect(uv_connect_t *connector, int status);
void on_write_done(uv_write_t *writer, int status);
void on_data(uv_stream_t *strm, ssize_t nread, const uv_buf_t *buf);
void make_buffer(uv_handle_t *handle, size_t suggested, uv_buf_t *buf);

const char *host = "broadcastlv.chat.bilibili.com";
const char *port = "2243";
const uint8_t web_handshake[] = {0,0,0,16, 0,16, 0,1, 0,0,0,7, 0,0,0,1};
const uint8_t web_heartbeat[] = {0,0,0,16, 0,16, 0,1, 0,0,0,2, 0,0,0,1};

int
main(int argc, char *argv[]) {
  uv_loop_t loop;
  ex_liveconn_t liveconn;
  int rc = 0;

  /* Acquire event loop */
  rc = uv_loop_init(&loop);
  assert(rc >= 0 && "failed at uv_loop_init()");

  /* Allocate liveconn memory */
  rc = liveconn_init(&loop, &liveconn);
  assert(rc >= 0 && "failed at liveconn_init()");

  /* Initiate the connection */
  rc = liveconn_start(&liveconn);
  assert(rc >= 0 && "failed at liveconn_start()");

  rc = uv_run(&loop, UV_RUN_DEFAULT);
  assert(rc >= 0 && "failed at uv_run()");

  /* Bring down the connection */
  rc = liveconn_close(&liveconn);
  assert(rc >= 0 && "failed at liveconn_close()");

  rc = uv_run(&loop, UV_RUN_DEFAULT);
  assert(rc >= 0 && "failed at uv_run()");

  rc = liveconn_free(&liveconn);
  assert(rc >= 0 && "failed at liveconn_free()");

  /* Release memory */
  rc = uv_loop_close(&loop);
  if (rc < 0) {
    fprintf(stderr, "(%d) %s\n", rc, uv_strerror(rc));
  }

  exit(EXIT_SUCCESS);
}

void
on_close(uv_handle_t *handle) {
  printf("(%p) Closed w/ data (%p)\n", handle, handle->data);
}

int
liveconn_start(ex_liveconn_t *liveconn) {
  struct addrinfo hints;
  int rc = 0;
  if (!liveconn)
    return UV_EINVAL;
  if (!liveconn->loop)
    return UV_EINVAL;

  /* TODO: replace these checks with a state flag. */

  /* Connection is established, may proceed to transfer. */
  if (liveconn->connector) {
    if (liveconn->connector) {
      free(liveconn->connector);
      liveconn->connector = NULL;
    }
    if (liveconn->addrs) {
      uv_freeaddrinfo(liveconn->addrs);
      liveconn->addrs = NULL;
    }
    ex_write_req_t *wr_req = malloc(sizeof(*wr_req));
    if (!wr_req)
      return UV_EAI_MEMORY;
    wr_req->needs_free = 0;
    wr_req->buf.base = (void*)web_handshake;
    wr_req->buf.len = sizeof(web_handshake)/sizeof(web_handshake[0]);
    wr_req->writer.data = wr_req;
    rc = uv_write(&wr_req->writer, (uv_stream_t*)liveconn->conn, &wr_req->buf, 1, on_write_done);
    assert(rc >= 0 && "failed at uv_write()");

    liveconn->conn->data = liveconn;
    rc = uv_read_start((uv_stream_t*)liveconn->conn, make_buffer, on_data);
    assert(rc >= 0 && "failed at uv_read_start()");
    return rc;
  }

  /* The TCP addr is ready. Connect. */
  if (liveconn->addrs) {
    if (liveconn->resolver) {
      free(liveconn->resolver);
      liveconn->resolver = NULL;
    }
    if (!liveconn->connector) {
      liveconn->connector = malloc(sizeof(*liveconn->connector));
      if (!liveconn->connector)
        return UV_EAI_MEMORY;
    }
    if (!liveconn->conn) {
      liveconn->conn = malloc(sizeof(*liveconn->conn));
      if (!liveconn->conn)
        return UV_EAI_MEMORY;
    }

    rc = uv_tcp_init(liveconn->loop, liveconn->conn);
    assert(rc >= 0 && "failed at uv_tcp_init()");

    /* TCP connect */
    liveconn->conn->data = liveconn;
    liveconn->connector->data = liveconn;
    if (!liveconn->addr_in_use) {
      liveconn->addr_in_use = liveconn->addrs;
    }
    rc = uv_tcp_connect(liveconn->connector, liveconn->conn, liveconn->addr_in_use->ai_addr, on_tcp_connect);

    if (rc < 0) {
      fprintf(stderr, "(%p) uv_tcp_connect(): (%d) %s\n", liveconn->connector, rc, uv_strerror(rc));
    }

    return rc;
  }

  /* No TCP addr or connection. Begin DNS resolve */
  if (!liveconn->resolver) {
    liveconn->resolver = malloc(sizeof(*liveconn->resolver));
    if (!liveconn->resolver)
      return UV_EAI_MEMORY;
  }
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;    /* Only TCP. Will never be UDP conn. */
  liveconn->resolver->data = liveconn;
  rc = uv_getaddrinfo(liveconn->loop, liveconn->resolver, on_dns_resolve, host, port, &hints);
  assert(rc >= 0 && "failed at uv_getaddrinfo()");

  return rc;
}

int
liveconn_write() {
  return 0;
}

int
liveconn_close(ex_liveconn_t *liveconn) {
  int rc = 0;
  if (!liveconn) {
    return 0;
  }
  if (liveconn->conn && !uv_is_closing((uv_handle_t*)liveconn->conn)) {
    rc = uv_read_stop((uv_stream_t*)liveconn->conn);
    uv_close((uv_handle_t*)liveconn->conn, on_close);
  }
  return rc;
}

int
liveconn_init(uv_loop_t* loop, ex_liveconn_t *liveconn) {
  if (!liveconn) {
    return -1;
  }

  memset(liveconn, 0, sizeof(*liveconn));

  liveconn->loop = loop;

  return 0;
}

int
liveconn_free(ex_liveconn_t *liveconn) {
  if (!liveconn) {
    return 0;
  }
  uv_freeaddrinfo(liveconn->addrs);
  free(liveconn->conn);
  free(liveconn->closer);
  free(liveconn->resolver);
  free(liveconn->connector);
  free(liveconn->read_buf);
  liveconn->addrs = NULL;
  liveconn->addr_in_use = NULL;
  liveconn->conn = NULL;
  liveconn->closer = NULL;
  liveconn->resolver = NULL;
  liveconn->connector = NULL;
  liveconn->read_buf = NULL;
  liveconn->read_buf_len = 0;
  return 0;
}

void
on_dns_resolve(uv_getaddrinfo_t *info, int status, struct addrinfo *res) {
  int rc = 0;
  ex_liveconn_t *liveconn = info->data;

  printf("(%p) DNS resolved addrinfo (%p) w/ status %d\n", info, res, status);
  if (status < 0) {
    return;
  }
  liveconn->addrs = res;
  rc = liveconn_start(liveconn);
  if (rc < 0) {
    fprintf(stderr, "(%p) liveconn_start: (%d) %s\n", liveconn, rc, uv_strerror(rc));
  }
}

void
on_tcp_connect(uv_connect_t *connector, int status) {
  char dest_addr[32];
  int rc = 0;
  ex_liveconn_t *liveconn = connector->data;

  printf("(%p) TCP connection completed w/ status %d\n", connector, status);
  if (status < 0) {
    return;
  }

  (void)dest_addr;
  rc = uv_ip4_name((struct sockaddr_in*)liveconn->addr_in_use->ai_addr, dest_addr, sizeof(dest_addr)/sizeof(dest_addr[0]));
  if (rc < 0) {
    fprintf(stderr, "(%p) liveconn_start: (%d) %s\n", liveconn, rc, uv_strerror(rc));
  }
  printf("(%p) TCP connection established w/ %s\n", liveconn, dest_addr);

  rc = liveconn_start(liveconn);
  if (rc < 0) {
    fprintf(stderr, "(%p) liveconn_start: (%d) %s\n", liveconn, rc, uv_strerror(rc));
  }
}


void
on_write_done(uv_write_t *writer, int status) {
  ex_write_req_t *wr_req = writer->data;

  if (wr_req->needs_free) {
    free(wr_req->buf.base);
  }
  free(wr_req);

  printf("(%p) TCP write data w/ status (%d)\n", writer, status);
}

void
on_data(uv_stream_t *strm, ssize_t nread, const uv_buf_t *buf) {
  uint8_t hex_map[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
  uint8_t *out = NULL;
  ssize_t i = 0, x = 0;
  ex_liveconn_t *liveconn = strm->data;
  printf("(%p) received (%ld) bytes of data\n", strm, nread);

  /* To hex */
  if (nread > 0) {
    out = malloc(nread * 3);
    if (!out)
      return;
    for (i = 0, x = 0; i < nread; ++i) {
      out[x++] = hex_map[buf->base[i]/16];
      out[x++] = hex_map[buf->base[i]%16];
      out[x++] = ' ';
    }
    printf("%.*s\n", (int)x, out);
    free(out);
  }
  else if (nread < 0) {
    liveconn_close(liveconn);
  }
}

void
make_buffer(uv_handle_t *handle, size_t suggested, uv_buf_t *buf) {
  ex_liveconn_t *liveconn = handle->data;
  size_t len = suggested > 8096 ? 8096 : suggested;
  void *data = liveconn->read_buf;

  printf("(%p) suggesting an allocation of (%ld) bytes\n", handle, suggested);

  if (!data) {
    data = malloc(len);
  }
  else if (liveconn->read_buf_len < len) {
    data = realloc(data, len);
  }
  if (!data)
    len = 0;
  liveconn->read_buf = data;
  liveconn->read_buf_len = len;
  buf->base = data;
  buf->len = len;
}
