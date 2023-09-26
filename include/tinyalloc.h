#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

bool ta_init(const void *base, const void *limit, const size_t heap_blocks, const size_t split_thresh, const size_t alignment);
void *ta_alloc(size_t num);
void *ta_calloc(size_t num, size_t size);
bool ta_free(void *ptr);
void *ta_realloc(void *ptr,size_t num);

size_t ta_num_free();
size_t ta_num_used();
size_t ta_num_fresh();
bool ta_check();
size_t ta_bytes_used();

#ifdef __cplusplus
}
#endif
