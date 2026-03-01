#include <stdio.h>
#include <assert.h>
#include <palloc.h>

void test_heap(void* p_out) {
  pa_heap_t* heap = pa_heap_new();
  void* p1 = pa_heap_malloc(heap,32);
  void* p2 = pa_heap_malloc(heap,48);
  pa_free(p_out);
  pa_heap_destroy(heap);
  //pa_heap_delete(heap); pa_free(p1); pa_free(p2);
}

void test_large() {
  const size_t N = 1000;

  for (size_t i = 0; i < N; ++i) {
    size_t sz = 1ull << 21;
    char* a = pa_mallocn_tp(char,sz);
    for (size_t k = 0; k < sz; k++) { a[k] = 'x'; }
    pa_free(a);
  }
}

int main() {
  void* p1 = pa_malloc(16);
  void* p2 = pa_malloc(1000000);
  pa_free(p1);
  pa_free(p2);
  p1 = pa_malloc(16);
  p2 = pa_malloc(16);
  pa_free(p1);
  pa_free(p2);

  test_heap(pa_malloc(32));

  p1 = pa_malloc_aligned(64, 16);
  p2 = pa_malloc_aligned(160,24);
  pa_free(p2);
  pa_free(p1);
  //test_large();

  pa_collect(true);
  pa_stats_print(NULL);
  return 0;
}
