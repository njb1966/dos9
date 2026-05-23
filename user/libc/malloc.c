#include <dos9.h>

/*
 * Simple free-list allocator backed by sbrk().
 *
 * Block layout (block_t header immediately precedes the payload):
 *
 *   [  size  |  free  |  next  |  _pad  ] [ payload ... ]
 *    4 bytes   4 bytes   4 bytes   4 bytes
 *   <------- 16-byte header ------------> < size bytes  >
 *
 * The 16-byte header keeps the payload 16-byte aligned (the heap starts
 * at a page boundary from sbrk, so all subsequent payloads are aligned).
 * `size` is rounded up to a multiple of 8 to maintain alignment through
 * the list.
 */

typedef struct block {
    uint32_t       size;    /* payload size (8-byte multiple) */
    uint32_t       free;    /* 1 = available */
    struct block  *next;    /* next block in the list */
    uint32_t       _pad;    /* pad header to 16 bytes */
} block_t;

static block_t *heap_head = NULL;

static uint32_t align8(size_t n) {
    if (n > (size_t)UINT32_MAX - 7u) return 0;
    return (uint32_t)((n + 7u) & ~7u);
}

/* Extend the heap with a new block of at least `need` payload bytes. */
static block_t *heap_extend(uint32_t need) {
    if (need > (uint32_t)INT32_MAX - (uint32_t)sizeof(block_t)) return NULL;
    uint32_t total = sizeof(block_t) + need;
    block_t *b = (block_t *)sbrk((int32_t)total);
    if ((void *)b == (void *)-1) return NULL;
    b->size = need;
    b->free = 0;
    b->next = NULL;
    return b;
}

/* First-fit search for a free block with at least `need` payload bytes. */
static block_t *find_free(uint32_t need) {
    block_t *b = heap_head;
    while (b) {
        if (b->free && b->size >= need) return b;
        b = b->next;
    }
    return NULL;
}

void *malloc(size_t size) {
    if (!size) return NULL;
    uint32_t need = align8(size);
    if (!need) return NULL;

    if (!heap_head) {
        /* First allocation — init from sbrk(0). */
        void *base = sbrk(0);
        if (base == (void *)-1) return NULL;
        heap_head = heap_extend(need);
        if (!heap_head) return NULL;
        return (void *)(heap_head + 1);
    }

    block_t *b = find_free(need);
    if (b) {
        b->free = 0;
        return (void *)(b + 1);
    }

    /* Walk to the last block and chain a new one. */
    block_t *last = heap_head;
    while (last->next) last = last->next;

    block_t *nb = heap_extend(need);
    if (!nb) return NULL;
    last->next = nb;
    return (void *)(nb + 1);
}

void free(void *ptr) {
    if (!ptr) return;
    block_t *b = (block_t *)ptr - 1;
    b->free = 1;

    /* Coalesce adjacent free blocks to reduce fragmentation. */
    block_t *cur = heap_head;
    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            cur->size += sizeof(block_t) + cur->next->size;
            cur->next  = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

void *calloc(size_t nmemb, size_t size) {
    if (nmemb && size > (size_t)-1 / nmemb) return NULL;
    size_t total = nmemb * size;
    if (!total) return NULL;
    void *p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (!size) { free(ptr); return NULL; }

    block_t *b = (block_t *)ptr - 1;
    uint32_t need = align8(size);
    if (!need) return NULL;

    if (b->size >= need) return ptr;   /* already large enough */

    void *np = malloc(size);
    if (!np) return NULL;
    memcpy(np, ptr, (size_t)b->size < size ? (size_t)b->size : size);
    free(ptr);
    return np;
}
