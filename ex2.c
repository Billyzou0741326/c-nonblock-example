/* A minimal libuv example.
 *
 * 1. Timer (x10000)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <uv.h>

void on_timer(uv_timer_t *handle);
void on_close(uv_handle_t *handle);

int
main(int argc, char *argv[]) {
  uv_loop_t loop;
  uv_timer_t timer[10000];
  int wait_ms = 0;
  int rc = 0;

  /* Acquire event loop */
  rc = uv_loop_init(&loop);
  assert(rc >= 0 && "failed at uv_loop_init");

  /* Acquire timer */
  for (int i = 0; i < sizeof(timer)/sizeof(*timer); ++i) {
    rc = uv_timer_init(&loop, &timer[i]);
    assert(rc >= 0 && "failed at uv_timer_init");
  }

  /* Timer becomes the work (New async) */
  for (int i = 0; i < sizeof(timer)/sizeof(*timer); ++i) {
    rc = uv_timer_start(&timer[i], on_timer, wait_ms=10000+i, 0);
    assert(rc >= 0 && "failed at uv_timer_start");
  }

  printf("Waiting at most %d ms...\n", wait_ms);

  /* Do work until no work is to be done (No more async) */
  rc = uv_run(&loop, UV_RUN_DEFAULT);
  assert(rc >= 0 && "failed at uv_run");

  /* Closing the handle becomes the work (New async) */
  for (int i = 0; i < sizeof(timer)/sizeof(*timer); ++i) {
    timer[i].data = &loop;
    uv_close((uv_handle_t *)&timer[i], on_close);
  }

  printf("(%p) Closing handle...\n", timer);

  /* Do work until no work is to be done (No more async) */
  rc = uv_run(&loop, UV_RUN_DEFAULT);
  assert(rc >= 0 && "failed at uv_run");

  /* Release memory */
  rc = uv_loop_close(&loop);
  if (rc < 0) {
    fprintf(stderr, "(%d) %s\n", rc, uv_strerror(rc));
  }

  exit(EXIT_SUCCESS);
}

void
on_timer(uv_timer_t *handle) {
  printf("(%p) Timer fires\n", handle);
}

void
on_close(uv_handle_t *handle) {
  printf("(%p) Closed w/ data (%p)\n", handle, handle->data);
}

