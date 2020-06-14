/* A minimal libuv example.
 *
 * 1. DNS resolve
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <uv.h>

void on_dns_res(uv_getaddrinfo_t *info, int status, struct addrinfo *res);

const char *host = "broadcastlv.chat.bilibili.com";
const char *port = "2243";

int
main(int argc, char *argv[]) {
  uv_loop_t loop;
  uv_getaddrinfo_t resolver;
  struct addrinfo hints;
  int rc = 0;

  /* Acquire event loop */
  rc = uv_loop_init(&loop);
  assert(rc >= 0 && "failed at uv_loop_init()");

  memset(&hints, 0, sizeof(hints));   /* init hints */
  hints.ai_socktype = SOCK_STREAM;    /* TCP only. If not set, uv returns duplicates. */

  /* Initiate an async dns resolve */
  rc = uv_getaddrinfo(&loop, &resolver, on_dns_res, host, port, &hints);
  assert(rc >= 0 && "failed at uv_getaddrinfo()");

  /* Do work until no work is to be done (No more async) */
  rc = uv_run(&loop, UV_RUN_DEFAULT);
  assert(rc >= 0 && "failed at uv_run()");

  /* Release memory */
  rc = uv_loop_close(&loop);
  if (rc < 0) {
    fprintf(stderr, "(%d) %s\n", rc, uv_strerror(rc));
  }

  exit(EXIT_SUCCESS);
}

void
on_dns_res(uv_getaddrinfo_t *info, int status, struct addrinfo *res) {
  char host[256] = {};
  char port[16] = {};
  printf("(%p) DNS resolved addrinfo (%p) w/ status %d\n", info, res, status);
  for (struct addrinfo *p = res; p; p = p->ai_next) {
    getnameinfo(p->ai_addr, p->ai_addrlen,
        host, sizeof(host)/sizeof(host[0]),
        port, sizeof(port)/sizeof(port[0]),
        NI_NUMERICHOST | NI_NUMERICSERV);
    printf("(%p) %s:%s\n", p, host, port);
  }
  uv_freeaddrinfo(res);
}
