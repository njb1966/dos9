#include <pmm.h>
#include <multiboot.h>
#include <terminal.h>
#include <stddef.h>

#define PAGE_SIZE        4096u
#define BITS_PER_WORD    32u

/* Linker symbol — address is the first byte past the kernel image. */
extern char _kernel_end[];

static uint32_t *bitmap;
static uint32_t  total_frames;
static uint32_t  free_count;

/* ── bitmap helpers ──────────────────────────────────────────────── */

static inline void frame_set(uint32_t f) {
    bitmap[f / BITS_PER_WORD] |= (1u << (f % BITS_PER_WORD));
}

static inline void frame_clear(uint32_t f) {
    bitmap[f / BITS_PER_WORD] &= ~(1u << (f % BITS_PER_WORD));
}

static inline int frame_test(uint32_t f) {
    return (bitmap[f / BITS_PER_WORD] >> (f % BITS_PER_WORD)) & 1;
}

/* ── range helpers ───────────────────────────────────────────────── */

/* Mark every frame that overlaps [base, base+len) as used. */
static void mark_used(uint32_t base, uint32_t len) {
    uint32_t first = base / PAGE_SIZE;
    uint32_t last  = (base + len + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t f = first; f < last && f < total_frames; f++)
        frame_set(f);
}

/*
 * Mark every frame wholly inside [base, base+len) as free.
 * Rounds start up and end down to avoid freeing partially-covered frames.
 */
static void mark_free(uint32_t base, uint32_t len) {
    uint32_t first = (base + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t last  = (base + len) / PAGE_SIZE;
    for (uint32_t f = first; f < last && f < total_frames; f++) {
        if (frame_test(f)) {    /* only count frames that were used */
            frame_clear(f);
            free_count++;
        }
    }
}

/* ── public API ──────────────────────────────────────────────────── */

void pmm_init(uint32_t magic, void *mbi_ptr) {
    struct multiboot_info *mbi = (struct multiboot_info *)mbi_ptr;

    if (magic != MULTIBOOT_MAGIC) {
        terminal_write("[PMM] ERROR: bad Multiboot magic\n");
        return;
    }

    /* Determine total physical frames from mem_upper (KB above 1MB). */
    uint32_t mem_kb = 1024 + (
        (mbi->flags & MULTIBOOT_FLAG_MEM) ? mbi->mem_upper : 0
    );
    total_frames = (mem_kb * 1024) / PAGE_SIZE;
    free_count   = 0;

    /* Bitmap lives immediately after the kernel image (_kernel_end is
       page-aligned by the linker script so no rounding needed here). */
    bitmap = (uint32_t *)_kernel_end;
    uint32_t bitmap_words = (total_frames + BITS_PER_WORD - 1) / BITS_PER_WORD;

    /* 1. Mark everything used. */
    for (uint32_t i = 0; i < bitmap_words; i++)
        bitmap[i] = 0xFFFFFFFFu;

    /* 2. Free every available region from the mmap (preferred path). */
    if (mbi->flags & MULTIBOOT_FLAG_MMAP) {
        struct multiboot_mmap_entry *e =
            (struct multiboot_mmap_entry *)(uintptr_t)mbi->mmap_addr;
        struct multiboot_mmap_entry *end =
            (struct multiboot_mmap_entry *)(uintptr_t)(mbi->mmap_addr + mbi->mmap_length);

        while (e < end) {
            /* Ignore regions above 4GB — we're 32-bit. */
            if (e->type == MULTIBOOT_MMAP_AVAILABLE && e->addr < 0x100000000ULL) {
                uint32_t base = (uint32_t)e->addr;
                uint32_t len  = (e->addr + e->len > 0x100000000ULL)
                    ? (uint32_t)(0x100000000ULL - e->addr)
                    : (uint32_t)e->len;
                mark_free(base, len);
            }
            /* Advance: size field does not include itself. */
            e = (struct multiboot_mmap_entry *)
                ((uint8_t *)e + e->size + sizeof(uint32_t));
        }
    } else {
        /* Fallback: assume memory above 1MB is all available. */
        mark_free(0x100000, mbi->mem_upper * 1024);
    }

    /* 3. Re-mark the kernel + bitmap region used.
          bitmap_words * 4 bytes for the bitmap itself. */
    uint32_t protected_end = (uint32_t)_kernel_end + bitmap_words * sizeof(uint32_t);
    mark_used(0, protected_end);

    /* Report. */
    uint32_t free_mb  = (free_count  * 4) / 1024;
    uint32_t total_mb = (total_frames * 4) / 1024;
    terminal_write("[PMM] ");
    terminal_writedec(total_mb);
    terminal_write(" MB RAM, ");
    terminal_writedec(free_mb);
    terminal_write(" MB free (");
    terminal_writedec(free_count);
    terminal_write(" frames)\n");
}

void *pmm_alloc_frame(void) {
    for (uint32_t f = 0; f < total_frames; f++) {
        if (!frame_test(f)) {
            frame_set(f);
            free_count--;
            return (void *)(f * PAGE_SIZE);
        }
    }
    return NULL;    /* out of memory */
}

void pmm_free_frame(void *addr) {
    uint32_t f = (uint32_t)(uintptr_t)addr / PAGE_SIZE;
    if (f < total_frames && frame_test(f)) {
        frame_clear(f);
        free_count++;
    }
}

uint32_t pmm_free_frames(void)  { return free_count;   }
uint32_t pmm_total_frames(void) { return total_frames; }
