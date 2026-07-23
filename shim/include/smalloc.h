// smalloc (static-pool allocator) shim: forwards to the host heap.
#pragma once
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

struct smalloc_pool {
  void *pool;
  size_t pool_size;
  int do_zero;
};

extern struct smalloc_pool smalloc_curr_pool;

static inline int sm_set_pool(struct smalloc_pool *spool, void *new_pool,
                              size_t new_pool_size, int do_zero, void *oomfn) {
  (void)oomfn;
  if (spool) {
    spool->pool = new_pool;
    spool->pool_size = new_pool_size;
    spool->do_zero = do_zero;
  }
  return 1;
}
static inline int sm_set_default_pool(void *new_pool, size_t new_pool_size,
                                      int do_zero, void *oomfn) {
  (void)new_pool; (void)new_pool_size; (void)do_zero; (void)oomfn;
  return 1;
}
static inline void *sm_malloc_pool(struct smalloc_pool *spool, size_t n) {
  (void)spool;
  return malloc(n);
}
static inline void *sm_zalloc_pool(struct smalloc_pool *spool, size_t n) {
  (void)spool;
  return calloc(1, n);
}
static inline void sm_free_pool(struct smalloc_pool *spool, void *p) {
  (void)spool;
  free(p);
}
static inline void *sm_malloc(size_t n) { return malloc(n); }
static inline void *sm_zalloc(size_t n) { return calloc(1, n); }
static inline void sm_free(void *p) { free(p); }
static inline void *sm_realloc(void *p, size_t n) { return realloc(p, n); }
static inline void *sm_calloc(size_t nmemb, size_t n) { return calloc(nmemb, n); }

#ifdef __cplusplus
}
#endif
