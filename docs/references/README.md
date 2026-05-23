# DOS/9 Reference Library

Authoritative reference material for building DOS/9. Documents are organized by source. Each entry notes what it's good for and which questions it answers.

---

## intel-sdm/
**Intel® 64 and IA-32 Architectures Software Developer's Manual** — Volumes 1, 2 (A–Z), 3 (A–D).

Ground truth for all x86 behavior: instruction encoding, CPU modes, segmentation, paging, interrupts, privilege levels, CPUID. When OSDev Wiki and reality disagree, this wins. Run `pdftotext` on each volume to produce `.txt` siblings for grepping.

Download: https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html (combined bundle available).

Files expected: `vol1.pdf`, `vol1.txt`, `vol2.pdf`, `vol2.txt`, `vol3.pdf`, `vol3.txt`

---

## osdev-wiki/
**OSDev Wiki** — browser only. https://wiki.osdev.org

The hobbyist OS developer's primary reference. Covers everything needed for Phase 0–2: Bare Bones, GDT, IDT, Interrupts, Paging, PIC, PIT, VGA Hardware, PS/2 Keyboard, Multiboot, ELF, ATA/IDE, FAT. The wiki is behind Cloudflare bot protection and cannot be scraped with CLI tools. Access it in a browser — it's a living resource and a local copy would drift anyway.

Priority pages: `Bare_Bones`, `GDT_Tutorial`, `IDT`, `8259_PIC`, `PIT`, `VGA_Hardware`, `PS/2_Keyboard`, `Multiboot`, `ELF`, `ATA_PIO_Mode`, `FAT`

This directory is intentionally empty.

---

## xv6/
**xv6** — MIT's teaching OS (x86 version).

`xv6-book.pdf`: the companion book. Read it before writing any kernel code — it explains process scheduling, virtual memory, file systems, and system calls at exactly the right level of detail for DOS/9's scope.

`xv6-source/`: clone of `https://github.com/mit-pdos/xv6-public` (x86 branch, not RISC-V). Use it for "how do I actually structure this?" lookups — scheduler, trap handlers, file system layer.

---

## plan9/
**Plan 9 from Bell Labs papers** — the conceptual foundation of DOS/9.

- `plan9-paper.pdf` — Pike et al. (1995). The original systems paper. Read before designing any API. Everything-is-a-file, per-process namespaces, and the network-transparent resource model all originate here.
- `namespaces.pdf` — *The Use of Name Spaces in Plan 9* (1992). Essential before designing the VFS or `/proc`.
- `lexical-names.pdf` — *Lexical File Names in Plan 9 or Getting Dot-Dot Right* (2000). Short paper on path resolution; read before implementing `..` traversal.

Sources: https://9p.io/sys/doc/

---

## ostep/
**Operating Systems: Three Easy Pieces** — Arpaci-Dusseau (free).

Chapter PDFs downloaded from https://pages.cs.wisc.edu/~remzi/OSTEP/. The three parts (virtualization/memory, concurrency, persistence) are all relevant. Use as concept reference when you hit something you haven't implemented before — clearer than any textbook.

---

## phil-opp/
**Writing an OS in Rust** — Philipp Oppermann's blog, scraped to markdown.

Source: https://os.phil-opp.com/ (second edition). DOS/9 is C, not Rust, but Oppermann's explanations of x86 mechanics are the clearest anywhere: VGA text mode, interrupts/exceptions, paging, heap allocator design. Use when OSDev Wiki is too terse.

---

## little-os-book/
**The Little Book About OS Development** — Helin & Renberg.

Source: https://littleosbook.github.io/ (~70 pages). Concise, practical, 32-bit focused — matches DOS/9's target exactly. Good first read before the Intel SDM. Covers bootloader → kernel entry → GDT → interrupts → paging → processes.

---

## freedos/
**FreeDOS source**.

Not a model for DOS/9's design, but the closest existing reference for "how does a DOS-like system actually work at the code level." Useful for shell design and FAT-evolved filesystem questions. Source: https://www.freedos.org/

---

## linux-subsystems/
**Selected Linux kernel subsystems** — comparison reference only.

Linux idioms assume infrastructure DOS/9 doesn't have. Use for "how does a production kernel handle this?" lookups, not as a model to follow.

- `fs-proc/` — Linux's `/proc` implementation. Reference when designing DOS/9's synthetic process filesystem.
- `fs-sysfs/` — Linux's `sysfs`. Reference for synthetic filesystem design patterns.
- `drivers-tty/` + `drivers-input/` — keyboard and console handling.

Clone selectively: `git clone --depth 1 --filter=blob:none --sparse https://github.com/torvalds/linux && git sparse-checkout set fs/proc fs/sysfs drivers/tty drivers/input`

---

## serenity/
**SerenityOS** — `https://github.com/SerenityOS/serenity`.

The most relevant living hobby OS. Single-author origins, full kernel, beautiful userland. Use for "how did Andreas solve this?" lookups. Caveats: C++ not C, 64-bit not 32-bit. Don't copy patterns wholesale — understand the intent, then adapt for DOS/9's constraints.

---

## specs/
**Hardware and protocol specifications.**

- `multiboot.pdf` — Multiboot specification. Required if using GRUB as bootloader (recommended). Source: https://www.gnu.org/software/grub/manual/multiboot/multiboot.html
- `fat.pdf` — FAT12/16/32 specification. Microsoft's public doc; also covered on OSDev Wiki. Reference when designing the filesystem layer.
- `ext2.pdf` — ext2 specification. Alternative to FAT if you decide against rolling your own.
- `gemini.txt` — Gemini protocol specification. Needed in Phase 4 for the Gemini client. Mirror: https://geminiprotocol.net/docs/specification.gmi
- `rfc1436-gopher.txt` — Gopher RFC 1436.
- `rfc4287-atom.txt` — Atom feed specification (RFC 4287). For RSS/feed reader work in Phase 4.
- `vga-freevga/` — FreeVGA project docs. Deeper VGA reference than OSDev Wiki when you need register-level detail. Source: http://www.osdever.net/FreeVGA/

---

## Search tips

PDFs are not directly greppable. For documents you'll search frequently (especially Intel SDM), run:

```bash
pdftotext vol1.pdf vol1.txt
```

Then grep the `.txt` file. Keep the PDF for reading (diagrams matter).

For scraped markdown (osdev-wiki, phil-opp):

```bash
grep -r "page fault" osdev-wiki/ --include="*.md" -l
```
