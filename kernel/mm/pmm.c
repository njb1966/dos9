#include <pmm.h>
#include <multiboot.h>
#include <kernel.h>
#include <terminal.h>
#include <stddef.h>

#define PAGE_SIZE        4096u
#define BITS_PER_WORD    32u

/* Linker symbol — address is the first byte past the kernel image. */
extern char _kernel_end[];

static uint32_t *bitmap;
static uint32_t  total_frames;
static uint32_t  free_count;

/* Snapshot of multiboot mods array.  Captured before the bitmap is written
   so it survives even when the loader places mods at _kernel_end. */
#define MAX_SAVED_MODS 8
static struct multiboot_mod saved_mods[MAX_SAVED_MODS];
static uint32_t              saved_mods_count;

uint32_t pmm_mod_count(void) { return saved_mods_count; }
struct multiboot_mod *pmm_mod(uint32_t i) {
    return (i < saved_mods_count) ? &saved_mods[i] : NULL;
}

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
    if (len == 0) return;
    uint64_t first = (uint64_t)base / PAGE_SIZE;
    uint64_t end   = (uint64_t)base + (uint64_t)len;
    uint64_t last  = (end + PAGE_SIZE - 1u) / PAGE_SIZE;
    for (uint64_t f = first; f < last && f < total_frames; f++)
        frame_set(f);
}

/*
 * Mark every frame wholly inside [base, base+len) as free.
 * Rounds start up and end down to avoid freeing partially-covered frames.
 */
static void mark_free(uint32_t base, uint32_t len) {
    uint64_t first = ((uint64_t)base + PAGE_SIZE - 1u) / PAGE_SIZE;
    uint64_t last  = ((uint64_t)base + (uint64_t)len) / PAGE_SIZE;
    for (uint64_t f = first; f < last && f < total_frames; f++) {
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
    uint64_t mem_bytes = (uint64_t)mem_kb * 1024u;
    total_frames = (uint32_t)(mem_bytes / PAGE_SIZE);
    free_count   = 0;

    /* Snapshot multiboot mods BEFORE we touch the bitmap.  The loader often
       places the mods array exactly at _kernel_end (the bitmap's address),
       so reading it after the bitmap fill would return 0xFFFFFFFF garbage. */
    saved_mods_count = 0;
    if ((mbi->flags & MULTIBOOT_FLAG_MODS) && mbi->mods_count > 0) {
        struct multiboot_mod *src =
            (struct multiboot_mod *)(uintptr_t)(mbi->mods_addr);
        uint32_t n = mbi->mods_count;
        if (n > MAX_SAVED_MODS) n = MAX_SAVED_MODS;
        for (uint32_t i = 0; i < n; i++) {
            saved_mods[i] = src[i];
            if (saved_mods[i].mod_end < saved_mods[i].mod_start)
                saved_mods[i].mod_end = saved_mods[i].mod_start;
        }
        saved_mods_count = n;
    }

    /* Place the bitmap past _kernel_end AND past any multiboot artifacts
       (mods array + every module's data).  Otherwise the bitmap fill below
       clobbers them — the loader typically puts the mods array and the
       module data immediately after the kernel image. */
    uint32_t kend_phys = PHYS((uint32_t)_kernel_end);
    uint32_t bm_phys   = kend_phys;
    if ((mbi->flags & MULTIBOOT_FLAG_MODS) && mbi->mods_count > 0) {
        uint64_t mods_end64 = (uint64_t)mbi->mods_addr +
                              (uint64_t)mbi->mods_count *
                              (uint64_t)sizeof(struct multiboot_mod);
        uint32_t mods_end = (mods_end64 > 0xFFFFFFFFu)
            ? 0xFFFFFFFFu
            : (uint32_t)mods_end64;
        if (mods_end > bm_phys) bm_phys = mods_end;
        for (uint32_t i = 0; i < saved_mods_count; i++)
            if (saved_mods[i].mod_end > bm_phys) bm_phys = saved_mods[i].mod_end;
    }
    bm_phys = (bm_phys + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);    /* page-align */

    bitmap = (uint32_t *)VIRT(bm_phys);
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
        uint64_t upper_bytes = (uint64_t)mbi->mem_upper * 1024u;
        if (upper_bytes > 0xFFFFFFFFu) upper_bytes = 0xFFFFFFFFu;
        mark_free(0x100000, (uint32_t)upper_bytes);
    }

    /* 3. Re-mark the kernel image used. */
    mark_used(0, kend_phys);

    /* 4. Re-mark the bitmap itself used. */
    mark_used(bm_phys, bitmap_words * sizeof(uint32_t));

    /* 5. Mark multiboot mods array + every module's data used (from snapshot,
          since the originals may already be inside the bitmap region). */
    if (saved_mods_count > 0) {
        uint64_t mods_len64 = (uint64_t)mbi->mods_count *
                              (uint64_t)sizeof(struct multiboot_mod);
        uint32_t mods_len = (mods_len64 > 0xFFFFFFFFu)
            ? 0xFFFFFFFFu
            : (uint32_t)mods_len64;
        mark_used(mbi->mods_addr, mods_len);
        for (uint32_t i = 0; i < saved_mods_count; i++)
            mark_used(saved_mods[i].mod_start,
                      saved_mods[i].mod_end - saved_mods[i].mod_start);
    }

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
