# PLAN.md — DOS/9

> **Project name:** **DOS/9** (pronounced "dos-nine")
> **Tagline:** Simple. Visible. Yours.
> **Sub tagline:** A Native Text Operating System
> **Status:** Phase 3 in progress — user-space shell running at ring 3; SYS_EXEC/READDIR/UNLINK/WAITPID live.
> **Repository:** https://github.com/njb1966/dos9 (GitHub only — see CLAUDE.md for push workflow)

DOS/9
Simple. Visible. Yours.
small subtext: A Native Text Operating System
---

## 1. The Premise

This is not a DOS clone. This is not a Linux distribution with a retro skin. This is an answer to a specific counterfactual:

**If DOS had been allowed to evolve on its own terms — without being smothered by Windows compatibility, without the GUI-first reorientation of the 90s, and if along the way it had learned from Plan 9 instead of ignoring it — what would it look like in 2026?**

The guiding philosophy is **evolution-first, not compatibility-first.** Real DOS died partly because it dragged its own past forever. DOS/9 does not. It inherits the *spirit* of DOS — single-user, text-mode, direct, simple, the shell as the center of the universe — and lets that spirit grow up, with Plan 9 as a quiet teacher along the way.

### The dual lineage

The name DOS/9 is not decorative. It is a commitment.

**From DOS, we inherit:** the shell-as-OS philosophy, single-user directness, text-mode as a first-class medium, the idea that a person should be able to understand the machine they use, command names short enough to type a thousand times a day, and an aesthetic continuity with computing's simpler eras.

**From Plan 9, we borrow specific architectural ideas** — not as cosmetic homage, but as real design commitments. See §1.1.

### 1.1. The Plan 9 commitments

DOS/9 makes one strong, kernel-level commitment to Plan 9 principles, and reserves space for a second, deeper one in later phases.

**Commit (v1): Everything is a file. Really this time.**

DOS gestured at this with `CON`, `PRN`, `NUL`, `COM1` — but it never followed through. Plan 9 did. DOS/9 finishes what DOS started.

In DOS/9, the following are filesystem-visible from the shell:

- **Devices:** `/dev/kbd`, `/dev/vga`, `/dev/com1`, `/dev/lpt`, etc. Read and write them like files.
- **Processes:** `/proc/<pid>/` exposes process state, memory maps, status. Killable by `rm` if you want it that way (we'll decide).
- **Network state:** `/net/tcp/`, `/net/udp/`, etc. — connection state inspectable with `cat`. (When networking lands.)
- **System state:** `/sys/` — kernel info, mounts, version. Single source of truth, no separate APIs.
- **Configuration:** the registry concept is rejected. Config is files. Files are config.

This costs almost nothing architecturally if decided early. It costs enormously if retrofitted late. Decided early.

**Aspirational (Phase 4+): Local and remote look identical.**

A 9P-style resource protocol, where a remote directory — including, eventually, a Gemini capsule or a Gopher hole — can be mounted into the local namespace and accessed with the same tools as local files. This is *deep* infrastructure and we do not promise it for v1. We design v1 in a way that does not preclude it.

**Explicitly NOT committing to:** per-process namespaces. Powerful but complex; deferred indefinitely, possibly forever.

### What DOS/9 values

- **The command line as the primary interface, not a fallback.** The shell *is* the OS. There is no GUI to fall back to.
- **Everything is a file** (see §1.1).
- **Text-mode UIs (TUIs) as a respected design medium.** Norton Commander was right. Turbo Vision was right. We pick up where they left off.
- **Structured I/O.** DOS pipes pass bytes. We pass typed records. PowerShell had the right idea, wrong execution.
- **Flat memory, no segmentation horrors.** 386 protected mode done correctly from day one.
- **Drivers as simple, loadable modules.** Not a kernel-mode horror show.
- **A filesystem that evolves FAT's simplicity rather than abandoning it for NTFS complexity.**
- **Small web citizenship.** Native Gemini, Gopher, and RSS clients as first-class system tools.
- **DOS-era games as native citizens** via integrated DOSBox-style emulation, treated with respect, not nostalgia kitsch.
- **Bootable on a Pentium.** If it doesn't run on hardware the small-web community already loves, we've failed.

### What DOS/9 rejects

- MS-DOS binary compatibility. We're DOS's *philosophical* heir, not its executable runtime.
- GUI-first design. A graphical layer may eventually exist, but it is never the default.
- Hidden complexity. The user should be able to understand the system they use.
- The registry concept. Configuration is files in the filesystem.
- Vendoring the Linux desktop and calling it a new OS. (See §3 — we've considered this and rejected it.)

---

## 2. Why this is achievable

The honest truth: writing a production OS from scratch is a multi-team, multi-year endeavor. **This project is not that.** This project is a hobbyist OS in the lineage of SerenityOS, MenuetOS, KolibriOS, and TempleOS — built evenings and weekends, shipped in pieces, valued for character rather than feature parity with mainstream systems.

The audience is not "everyone." The audience is the small web community — a few hundred to few thousand people who already love Gemini browsers, mutt, newsboat, tmux, and the aesthetic of computing's simpler eras. **If 200 of them try it and 20 use it weekly, this project has succeeded.**

The developer is one person ("dangerous"-tier programming background) working with an AI coding companion (Claude Code) and the substantial body of public OS-development knowledge: xv6, OSDev Wiki, the Intel SDM, *Operating Systems: Three Easy Pieces*, and the source of every hobby OS ever published.

---

## 3. Architectural decision: kernel-level, not "Linux distro with a theme"

This was considered carefully. A "custom Linux that boots straight into our environment" would be ~90% of the user experience for ~10% of the work. We are **rejecting that path** for the following reasons:

- The point of the project is partly the journey: "I built an OS" is a real and legitimate motivation.
- The small web audience can tell the difference. A real kernel earns credibility a themed distro does not.
- Long-term design freedom: kernel-level control means we can build structured I/O, our own driver model, our own process model — things a Linux base would constantly push back on.

**The kernel will be original.** The userland will lean on existing open-source code where it makes sense (porting a libc, possibly a port of DOSBox eventually), but the system is ours.

### Target architecture: 32-bit protected mode (i386 / IA-32)

Chosen deliberately:

- **Thematically correct.** A flat-memory 32-bit OS is what DOS *should have become* in the 386 era.
- **Simpler.** No long mode, no PAE complications, no four-level paging.
- **More reference material.** Every tutorial OS, every OSDev page, xv6 itself — all target 32-bit first.
- **Hardware-friendly.** Runs on a Pentium. Runs on a Thinkpad from 2003. Runs anywhere the small web community already runs.
- **64-bit can come later** if the project lives long enough to want it.

### Language: C

- Maximum reference material.
- Matches xv6, which is our spiritual teacher.
- Rust is tempting; rejected for v1 because the ecosystem assumes sophistication we shouldn't pay for yet.
- Some assembly is unavoidable (boot, interrupts, context switching). That's fine.

---

## 4. Phased plan

### Phase 0 — Calibration (current phase)

**Goal:** Build a correct mental model before writing a line of code.

- [ ] Read the xv6 book cover to cover.
- [ ] Skim the xv6 source alongside the relevant book chapters.
- [ ] Skim OSDev Wiki's "Getting Started" and "Beginner Mistakes" pages.
- [x] Choose a project name. Update this document. (**DOS/9**)
- [x] Set up the development environment: cross-compiler (i686-elf-gcc 14.3.0 + binutils 2.46 at `~/tools/cross`), QEMU, GDB, Makefile build system. Kernel boots to `kernel_main()` in QEMU.
- [x] Decide on the RAG / reference-lookup approach (see §6). (**Option B: grep + AI companion**)
- [x] Assemble reference corpus in `docs/references/` (see §6).
- [x] Create `CLAUDE.md` with project context and reference URLs for Claude Code sessions.

**Exit criteria:** You can describe, in your own words, what a bootloader does, what protected mode is, what a GDT and IDT are, how paging works, and how xv6 structures its process table.

### Phase 1 — To a prompt

**Goal:** Boot on QEMU, get to a working keyboard prompt. The screen shows your OS's name, a cursor blinks, you type, characters appear. Nothing more.

This is the emotional hardest phase. It looks like nothing. It is the foundation of everything.

- [x] Bootloader: using GRUB via Multiboot (`boot/boot.S` — Multiboot header + 16KB stack setup).
- [x] Kernel entry: Multiboot handles protected mode; `_start` sets stack, calls `kernel_main()` in C.
- [x] VGA text-mode driver: character write, scrolling, hardware cursor via CRTC ports. (`kernel/drivers/terminal.c`)
- [x] GDT: flat model, kernel + user code/data segments (5 entries). (`kernel/arch/i386/gdt.c`)
- [x] IDT: exception handlers 0-31 with named messages + register dump, ISR stubs in assembly. (`kernel/arch/i386/idt.c`, `isr.S`)
- [x] PIC remapping and IRQ dispatch: IRQs 0-7 → vectors 32-39, IRQs 8-15 → vectors 40-47; registered handler table. (`kernel/arch/i386/pic.c`)
- [x] PS/2 keyboard driver: scancode set 1 → ASCII (normal + shifted), ring buffer, IRQ1 handler. (`kernel/drivers/keyboard.c`)
- [x] Minimal REPL shell: `DOS/9>` prompt, line editing (backspace), `help` and `halt` commands. (`kernel/shell.c`)

**Exit criteria:** `qemu-system-i386 -kernel build/kernel.elf` shows the OS banner, accepts typed input, can halt cleanly.

**Estimated effort:** 2–4 months of evenings and weekends, depending on how often you have to read the Intel SDM.

### Phase 2 — Real kernel services ✓ COMPLETE

**Goal:** Have the things a real OS has, even if they're minimal. **This is where the "everything is a file" commitment becomes architectural reality** — the VFS must be designed early because every later subsystem will hang off it.

- [x] Physical memory manager (bitmap allocator). (`kernel/mm/pmm.c`)
- [x] Virtual memory: paging, page directory/table setup, kernel mapped high (`0xC0000000`), user space low. (`kernel/mm/vmm.c`)
- [x] Kernel heap (kmalloc/kfree) — 4MB fixed-mapped at `0xD0000000`. (`kernel/mm/kheap.c`)
- [x] Timer (PIT) at 100Hz, preemptive scheduling. (`kernel/arch/i386/pit.c`, `kernel/proc/process.c`)
- [x] Processes and context switching — 8-slot table, per-process kernel stacks, ring-3 iret. (`kernel/proc/process.c`, `kernel/arch/i386/switch.S`)
- [x] System call interface — `int 0x80`, SYS_EXIT/READ/WRITE/OPEN/CLOSE/LSEEK/GETPID/BRK/EXEC/READDIR/UNLINK/WAITPID. (`kernel/arch/i386/syscall.c`)
- [x] **Virtual File System (VFS) layer** — multi-mount namespace, vnode/fs_ops abstraction, per-process fd tables. (`kernel/fs/vfs.c`)
- [x] **`/dev` synthetic filesystem** — `/dev/con` (VGA console r/w), `/dev/kbd` (keyboard read). (`kernel/fs/devfs.c`)
- [x] **`/proc` synthetic filesystem** — `/proc/<pid>` entries; `rm /proc/<pid>` kills process. (`kernel/fs/procfs.c`)
- [x] **`/mod` synthetic filesystem** — loadable kernel module interface. (`kernel/fs/modfs.c`)
- [x] ELF loader — parses PT_LOAD segments, maps into per-process page directory, returns entry + brk. (`kernel/arch/i386/elf.c`)
- [x] User-space libc — crt0, stdio (printf/puts/putchar), string, stdlib, malloc/free/calloc/realloc. (`user/libc/`)
- [x] DOS9FS disk filesystem — custom flat FS; mkdisk host tool; mounted at `/disk`. (`kernel/fs/diskfs.c`, `tools/mkdisk.c`)
- [x] SYS_BRK (sbrk semantics) — extends user heap by mapping new pages into the process page directory.
- [x] Per-process fd tables — `file_t fds[16]` in `process_t`; inherited on fork, closed on exit.

**Exit criteria met:** User-space hello world loads from disk, runs at ring 3, returns cleanly. `cat /dev/kbd` shows keypresses. `ls /proc` shows running processes. Plan 9 commitment visible at the prompt.

### Phase 3 — The shell becomes the OS ← IN PROGRESS

**Goal:** The userland is where this project *differentiates*. Everything before was foundation. This is the project's actual identity.

#### Phase 3 foundation (done)

- [x] Per-process fd tables + SYS_LSEEK, SYS_GETPID.
- [x] SYS_BRK + user-space malloc/free/calloc/realloc (free-list allocator, coalescing).
- [x] SYS_EXEC — kernel loads ELF from VFS path, spawns ring-3 process, returns pid. Reads ELF into 4MB kernel heap (sufficient for Phase 3; fd-based streaming loader is future work).
- [x] SYS_READDIR, SYS_UNLINK, SYS_WAITPID.
- [x] `_syscall4` (esi as 4th arg) in user libc.
- [x] `user/sh.c` — ring-3 shell: readline with echo/backspace, echo/ls/cat/exec/run/rm/pid commands.
- [x] `sh.elf` on disk; boot sequence: `DOS/9> exec /disk/sh` → `sh>`.
- [x] **Auto-launch sh at boot** — `kernel_main` calls `shell_exec_user("/disk/sh")` after diskfs init, then waits in `schedule()` loop until sh exits; falls back to `DOS/9>` prompt. Keyboard exclusively owned by sh while running.
- [x] **Arrow keys in keyboard driver** — 0xE0 extended prefix handled; up/down/left/right translated to ANSI CSI sequences (ESC [ A/B/C/D) in the ring buffer. (`kernel/drivers/keyboard.c`)
- [x] **Shell: command history** — 32-entry circular buffer; up/down arrows cycle entries; saves current partial line when browsing, restores on down past newest; skips consecutive duplicates. (`user/sh.c`)
- [x] **Shell: tab completion** — splits last word at '/'; opens parent dir via VFS readdir; unique match appended in-place; multiple matches listed with prompt redraw. (`user/sh.c`)

- [x] **Pipes** — anonymous kernel pipe (4KB ring buffer, `VTYPE_CHR` vnodes). VFS vnode reference counting added so `dup2` tracks write-end refs correctly; reader blocks until `write_vnode->refs == 0`. `SYS_PIPE`/`SYS_DUP`/`SYS_DUP2` syscalls + user wrappers. (`kernel/fs/pipe.c`, `vfs.c` refcount)
- [x] **Shell: pipeline syntax** — `left | right`; pipe created, left exec'd with stdout→write end, right exec'd with stdin←read end; shell drops both ends and waits. External ELF programs on both sides; built-in piping deferred (needs fork). (`user/sh.c`)
- [x] **`/disk/cat`** — stdin→stdout passthrough; test with `/disk/hello | /disk/cat`. (`user/cat.c`)

#### Phase 3 remaining
- [ ] **Shell: structured pipes** — currently byte-stream; typed record framing deferred until built-in piping is solved (needs fork or in-process concurrency).
- [x] **Shell: scripting** — variables (`set`/`unset`/`env`, `$expansion`), `if <cmd> ... end` (condition runs immediately, block executes if exit 0), `for name in words ... end`, `loop ... end` with `break`; `true`/`false` built-ins. Kernel: `process_t.exit_code` + `sys_waitpid` returns child exit code. (`user/sh.c`, `kernel/proc/process.c`, `kernel/arch/i386/syscall.c`)
- [x] **Shell: inline help** — `--help` on every built-in; `help <cmd>` looks up usage+desc from `cmd_entry_t` table; table-driven dispatch replaces dispatch chain. (`user/sh.c`)
- [ ] **TUI toolkit** — consistent widget library: menus, dialogs, borders, color schemes, F-key bindings. Turbo Vision lineage. Required by file manager and editor.
- [ ] **File manager** — Norton Commander-style, two panes, F-key actions, fast keyboard navigation. Built on TUI toolkit.
- [ ] **Text editor** — `EDIT.COM`'s spiritual descendant. Full-screen, no modal nonsense, built on TUI toolkit.
- [ ] **Package system** — simple, signed. Installs ELF binaries + metadata to `/disk`. No dependency resolver for v1.

#### Known technical debt / future improvements
- ELF loader (`elf_load`) copies segment data byte-by-byte via page-table walk. Should batch by page.
- `sys_exec` reads entire ELF into kernel heap before loading. Future: fd-based streaming loader eliminates the buffer.
- Process table is fixed at 8 slots. Increase when user workloads grow.
- No `write()` syscall to disk (DOS9FS is read-only from user space). Needed before package system.

### Phase 4 — Small web citizenship

**Goal:** This is what makes DOS/9 *findable* and *loved* by its target audience.

- [ ] Native Gemini browser (TUI).
- [ ] Native Gopher client.
- [ ] Native RSS reader.
- [ ] Possibly: Finger, NNTP, IRC clients.
- [ ] A networking stack capable of supporting the above. (This is significant work. Possibly use lwIP as a starting point rather than rolling our own TCP/IP.)
- [ ] **Aspirational:** lay groundwork for the 9P-style remote-as-local mount idea (§1.1). Even a read-only "mount a Gemini capsule as a directory" prototype would be the moment DOS/9's thesis becomes undeniable. Not promised. Designed-for, not built-yet.

### Phase 5 — Games as first-class citizens

**Goal:** DOS-era games run natively (via integrated emulation), launched from the shell, treated as proper system programs rather than a curiosity.

- [ ] Port or integrate DOSBox-equivalent functionality.
- [ ] A game launcher in the OS's native TUI style.
- [ ] Save state management as a system service.

---

## 5. Working with an AI coding companion

This project will be built with Claude Code as the developer's companion. Some hard-won lessons specific to kernel work:

- **Cross-reference everything.** Kernel code has exact requirements that are easy to get subtly wrong — alignment, bit patterns in descriptors, ordering when setting up paging. When the AI confidently says "set this bit in CR0," verify against OSDev Wiki and the Intel SDM. The training data on this domain is good but not perfect, and one wrong bit in a GDT entry is a triple fault that wastes hours.
- **The Intel SDM is the ground truth.** Not the AI, not OSDev Wiki, not even xv6. When in doubt, the manual wins.
- **Commit constantly.** Every time something works, commit. "The last version that booted" is precious. You will break things in ways that are hard to reason about, and bisection saves projects.
- **Use QEMU + GDB, not real hardware.** Until late in the project. Real-hardware bugs add a layer of difficulty you don't need yet.
- **Don't let the AI over-architect early.** Phase 1 should be small, ugly, and working. Refactor later. Premature elegance kills hobby OS projects.

---

## 6. RAG / reference-lookup strategy

Kernel development requires fast, precise lookup of authoritative material. We have two viable approaches:

### Option A: Minimal RAG
Chunk the Intel SDM PDFs, scraped OSDev Wiki, and xv6 source. Embed with an off-the-shelf model. Query when needed. **Goal:** "What does page-fault error code bit 3 mean?" returns the right SDM page in two seconds.

### Option B: Grep + AI companion
Keep the SDM PDFs and OSDev Wiki scraped to markdown in `docs/references/`. Let the AI grep them. Less elegant, often more reliable for precise lookups (vector search can miss exact terminology where grep wouldn't).

**Recommendation:** Start with Option B. If it's insufficient after a few weeks of real use, build Option A on top of it. Do not spend two weeks building a beautiful retrieval pipeline before writing a bootloader.

### Reference corpus

All located under `docs/references/`. See `docs/references/README.md` for per-item notes, grep tips, and download sources.

- [x] **Intel SDM** — combined PDF (5342 pages) + greppable `.txt`. `intel-sdm/intel-sdm-combined.{pdf,txt}`
- [x] **OSDev Wiki** — browser-only (https://wiki.osdev.org). Cloudflare blocks scraping; wiki is a living resource so a local copy would drift anyway. URL is in `CLAUDE.md` for every session.
- [x] **xv6** — source in `xv6/xv6-source/`, book PDF in `xv6/`.
- [x] **Plan 9 papers** — `plan9/plan9-paper.pdf`, `namespaces.pdf`, `lexical-names.pdf`.
- [x] **9front** — shallow clone of the active Plan 9 fork. `plan9/9front/`. Key: `sys/src/9/` (kernel), `sys/src/lib9p/` (9P implementation), `rc/` (shell).
- [x] **Operating Systems: Three Easy Pieces** — all chapter PDFs. `ostep/`
- [x] **Little OS Book** — `little-os-book/little-os-book.pdf`. Concise 32-bit walkthrough, matches DOS/9's target exactly.
- [x] **Philipp Oppermann's blog** — full source cloned from GitHub as markdown. `phil-opp/blog/content/edition-2/posts/`. Cleaner than a scrape.
- [x] **Linux kernel subsystems** — sparse clone: `fs/proc`, `fs/sysfs`, `drivers/tty`, `drivers/input`. `linux-subsystems/`
- [x] **SerenityOS** — shallow clone (190MB). `serenity/`. Key: `Kernel/Memory`, `Kernel/FileSystem`, `Kernel/Tasks`, `Kernel/Interrupts`.
- [x] **FreeDOS** — kernel (`freedos/kernel/`) and command shell (`freedos/freecom/`). Reference for shell design and FAT-era filesystem conventions.
- [x] **Specs** — `specs/multiboot.pdf`, `specs/rfc1436-gopher.txt`, `specs/rfc4287-atom.txt`.

---

## 7. Repository structure

```
DOS9/
├── CLAUDE.md                # Claude Code session context — loaded automatically every session
├── PLAN.md                  # this file
├── README.md                # for the public, eventually
├── docs/
│   ├── design/              # design notes, decisions, rationale (to be created)
│   ├── references/          # reference corpus — see README.md inside
│   │   ├── README.md        # index: what's here and what each item is for
│   │   ├── intel-sdm/       # Intel SDM combined PDF + greppable txt
│   │   ├── osdev-wiki/      # empty — browser-only (wiki.osdev.org)
│   │   ├── xv6/             # xv6 book PDF + x86 source clone
│   │   ├── plan9/           # Plan 9 papers + 9front source clone
│   │   ├── ostep/           # all OSTEP chapter PDFs
│   │   ├── phil-opp/        # blog_os source clone (markdown)
│   │   ├── little-os-book/  # Little OS Book PDF
│   │   ├── freedos/         # FreeDOS kernel + freecom shell source
│   │   ├── linux-subsystems/# sparse Linux clone: fs/proc, fs/sysfs, drivers/tty, drivers/input
│   │   ├── serenity/        # SerenityOS shallow clone
│   │   └── specs/           # multiboot, gopher RFC, atom RFC
│   └── journal/             # dev journal — to be created; invaluable later
├── boot/                    # bootloader or multiboot stub (Phase 1)
├── kernel/                  # (Phase 1)
│   ├── arch/i386/           # arch-specific code
│   ├── mm/                  # memory management
│   ├── proc/                # processes, scheduling
│   ├── fs/                  # filesystem + VFS
│   ├── drivers/             # device drivers
│   └── include/             # kernel headers
├── libc/                    # our libc or newlib port (Phase 2)
├── userland/                # (Phase 3)
│   ├── shell/
│   ├── tui/                 # TUI toolkit
│   ├── coreutils/           # ls, cat, etc., redesigned
│   └── apps/                # editor, file manager, etc.
├── tools/                   # build tools, scripts (Phase 1)
├── scripts/                 # qemu launchers, debug helpers (Phase 1)
├── Makefile                 # (Phase 1)
└── .gitignore               # (Phase 1)
```

---

## 8. Success criteria (the honest version)

This project succeeds if:

1. **It boots, accepts input, and shows personality** — even if it never gets past Phase 2.
2. **At least one small-web person finds it and writes about it** without us asking.
3. **The developer still enjoys working on it after a year.** Burnout is the project's biggest risk.
4. **At least one piece of the userland (the shell, the editor, the TUI toolkit) is independently useful** — even ported back to Linux as a standalone program.

This project does **not** need to:
- Replace anyone's daily driver.
- Run modern web browsers.
- Support more than a handful of devices.
- Be "competitive" with anything.

---

## 9. Open questions (to resolve before / during Phase 0)

- [x] **Project name.** Resolved: **DOS/9**.
- [ ] **Bootloader:** GRUB/Multiboot (recommended) or roll our own?
- [ ] **Filesystem in Phase 2:** original FAT-evolved design, or port ext2? (Reminder: the disk-backed FS is *separate* from the synthetic `/dev`, `/proc`, `/sys` filesystems, which are non-negotiable per §1.1.)
- [ ] **VFS design study:** before writing the VFS, read how Plan 9, Linux, and xv6 each handle theirs. Plan 9's is the most relevant philosophically, Linux's is the most powerful, xv6's is the most readable. Pick what to steal from each.
- [ ] **`/dev` naming conventions:** follow Plan 9 (`/dev/cons`, `/dev/kbd`) or Linux (`/dev/tty`, `/dev/input/...`) or invent? Lean Plan 9.
- [ ] **Process kill via filesystem:** does `rm /proc/<pid>` kill the process? Plan 9 has flavors of this. Decide before `/proc` is implemented.
- [ ] **Configuration philosophy:** if config is files (no registry), where do they live? `/etc/` (Unix), `/cfg/` (shorter, fits DOS aesthetic), or per-app in `/sys/`?
- [ ] **License:** MIT, BSD-2, or something more opinionated?
- [ ] **Public repo from day one, or stealth until Phase 1 works?**
- [ ] **Dev journal location:** in-repo `docs/journal/`, or a separate Gemini capsule? (The latter is on-brand and might attract the audience earlier. *Recommendation: do both. Gemini capsule for narrative, in-repo for technical.*)

---

## 10. Current state and next steps

**Done:**
- Project named, philosophy documented.
- Reference corpus assembled (`docs/references/`).
- Toolchain: `i686-elf-gcc` 14.3.0 + binutils 2.46 at `~/tools/cross`; GDB, QEMU installed.
- **Phase 1 complete:** GDT, IDT, PIC, VGA driver, PS/2 keyboard, minimal REPL.
- **Phase 2 complete (2026-05-19):** PMM, VMM (4KB paging, higher-half), kernel heap, PIT,
  preemptive round-robin scheduler, VFS, devfs (/dev/vga /dev/kbd /dev/null),
  procfs (/proc/<pid>/status, rm-kills), modfs (/mod/N), int 0x80 syscall gate
  (exit/read/write/open/close), ELF loader, ring-3 user processes,
  user libc (crt0 + stdio + string + stdlib + dos9.h), ATA PIO driver,
  DOS9FS disk filesystem (/disk), tools/mkdisk host tool.

**Phase 3 in progress (2026-05-19):**
- [x] Per-process fd tables — `file_t fds[MAX_FDS]` in `process_t`; user processes inherit fds 0-2 (stdin/stdout/stderr); `process_exit()` closes all fds; slot recycling handles `process_kill()` path
- [x] `SYS_LSEEK` (#5) — `vfs_lseek()` with SEEK_SET/CUR/END; character devices return -1
- [x] `SYS_GETPID` (#6) — `process_getpid()`
- [x] `SYS_BRK` (#7) — `sbrk()` semantics; `brk` field in `process_t`, set by `elf_load()` brk_out; pages mapped on demand via `vmm_map_page_in`; ceiling 0xBF000000
- [x] User libc: `malloc/free/calloc/realloc` — free-list allocator in `user/libc/malloc.c`, 16-byte headers, 8-byte aligned payloads, first-fit with coalescing
- [ ] User-space shell program (ring-3, reads `/disk` for commands)
- [ ] TUI toolkit foundations
