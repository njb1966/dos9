#include <kheap.h>
#include <vmm.h>
#include <pmm.h>
#include <terminal.h>
#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE    4096u
#define HEAP_START   0xD0000000u
#define HEAP_PAGES   1024u          /* 4MB kernel heap */
#define HEAP_MAGIC   0xDEAD4EADu

/*
 * Block header — 16 bytes.
 * All returned pointers are 16-byte aligned (HEAP_START is page-aligned,
 * header is 16 bytes, and kmalloc rounds requests up to 8 bytes).
 */
typedef struct block {
    struct block *next;   /* next block in linear heap order */
    uint32_t      size;   /* bytes of user data following this header */
    uint32_t      free;   /* 1 = available, 0 = allocated */
    uint32_t      magic;  /* HEAP_MAGIC — detects header overwrites */
} block_t;

#define HDR   ((uint32_t)sizeof(block_t))   /* 16 */
#define ALIGN 8u

static block_t *heap_list = NULL;

void kheap_init(void) {
    for (uint32_t i = 0; i < HEAP_PAGES; i++) {
        uint32_t paddr = (uint32_t)pmm_alloc_frame();
        vmm_map_page(HEAP_START + i * PAGE_SIZE, paddr, PF_PRESENT | PF_WRITE);
    }

    heap_list        = (block_t *)HEAP_START;
    heap_list->next  = NULL;
    heap_list->size  = HEAP_PAGES * PAGE_SIZE - HDR;
    heap_list->free  = 1;
    heap_list->magic = HEAP_MAGIC;

    terminal_write("[HEAP] ");
    terminal_writedec(HEAP_PAGES * 4);
    terminal_write(" KB ready\n");
}

void *kmalloc(uint32_t size) {
    if (!size) return NULL;
    size = (size + ALIGN - 1) & ~(ALIGN - 1);

    for (block_t *b = heap_list; b; b = b->next) {
        if (!b->free || b->size < size) continue;

        /* Split: carve a new free block from the tail if there's room. */
        if (b->size >= size + HDR + ALIGN) {
            block_t *tail  = (block_t *)((uint8_t *)b + HDR + size);
            tail->next     = b->next;
            tail->size     = b->size - size - HDR;
            tail->free     = 1;
            tail->magic    = HEAP_MAGIC;
            b->next        = tail;
            b->size        = size;
        }
        b->free = 0;
        return (uint8_t *)b + HDR;
    }
    return NULL;    /* heap exhausted */
}

void kfree(void *ptr) {
    if (!ptr) return;
    block_t *b = (block_t *)((uint8_t *)ptr - HDR);
    if (b->magic != HEAP_MAGIC) {
        terminal_write("[HEAP] kfree: bad magic — heap corruption\n");
        return;
    }
    b->free = 1;
    /* Coalesce contiguous free successors. */
    while (b->next && b->next->free)  {
        b->size += HDR + b->next->size;
        b->next  = b->next->next;
    }
}
