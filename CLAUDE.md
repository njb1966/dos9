# DOS/9 — Claude Code Context

DOS/9 is a hobbyist OS kernel targeting 32-bit x86. It is conceptually inspired by Plan 9 (per-process namespaces, everything-is-a-file) but runs as a native kernel, not a hosted process. The goal is a working kernel with a shell, VFS, process model, and small-web client support (Gemini, Gopher, RSS, Finger, IRC, and NNTP).

Development target and compatibility policy:

- QEMU is the fast inner-loop test harness.
- VMs are the compatibility and integration target.
- Old physical hardware is a first-class target, including Pentium-class machines.
- New code should avoid assuming emulator-only behavior unless it is explicitly gated or documented.

Compatibility policy:

- Set the support floor to 32-bit Pentium-class x86 with conservative BIOS-era behavior.
- Write to that floor directly; do not assume QEMU conveniences are universal.
- Detect optional features and provide safe fallbacks.
- Use conservative VMs as the default pre-hardware test environment.
- Keep the hardware-readiness checklist current for timing, storage, and networking changes.

Hardware readiness gate before calling a feature deployment-ready:

- RTC/time must work on real hardware without relying on the fallback Unix timestamp.
- Networking must work without SLIRP-only defaults or other QEMU-specific assumptions.
- Serial mirroring must fail safely if COM1 is absent or unwired.
- New fixed-size limits need an explicit reason and a growth path.
- Boot, shell, disk, and Gemini should still work under a conservative VM setup.
- Major timing, storage, or networking changes should be checked on at least one real Pentium-class machine when available.

Compatibility matrix:

- `dev-qemu` for fast iteration
- `vm-baseline` for boot and core device checks
- `vm-guestfwd` for local TCP smoke tests without external DNS/TLS. Pair `scripts/guestfwd-server.sh` on the host with `make smoke-net`.
- `vm-network` for DNS, TLS, and Gemini validation
- `pentium-floor` for real hardware validation when available

Known non-blocking limitation:

- Shell parsing is intentionally small and now supports basic quoting/escaping, but it is still not a full POSIX shell. Revisit it only when user-facing commands need richer shell semantics.
- `make smoke-shell` is the repeatable shell regression check; it feeds `tests/shellreg.txt` into the guest shell and asserts the expected output.
- The shell smoke set also covers package metadata and reinstall flow with `/disk/pkg info /disk/hello.d9p` and `/disk/pkg install /disk/hello.d9p`, plus the `time`, formatter, allocator, argv, pipeline, and basic `if`/`for` probes already on disk.

---

## Key Reference URLs

These are live browser resources — consult them when the local reference docs don't cover something.

| Resource | URL | What it's for |
|---|---|---|
| OSDev Wiki | https://wiki.osdev.org | Primary reference for all x86 hardware details: GDT, IDT, PIC, PIT, paging, VGA, PS/2, ATA, Multiboot, ELF. Always check here first for hardware-level questions. |
| Philipp Oppermann's OS blog | https://os.phil-opp.com | Clearest explanations of x86 mechanics anywhere. C++ but the concepts map directly. |
| Little OS Book | https://littleosbook.github.io | Concise 32-bit OS walkthrough — matches DOS/9's target exactly. |
| OSTEP (online) | https://pages.cs.wisc.edu/~remzi/OSTEP | Chapter-by-chapter OS theory reference. |
| Plan 9 papers | https://9p.io/sys/doc | Authoritative source for Plan 9 namespace and VFS design. |
| xv6 source (x86) | Local copy in `docs/references/xv6/` | Teaching OS — use for "how is this structured?" lookups. |

---

## Local Reference Docs

All under `docs/references/`. Key items:

- `intel-sdm/intel-sdm-combined.pdf` — 5342-page Intel x86 manual. Ground truth for all CPU behavior.
- `intel-sdm/intel-sdm-combined.txt` — greppable text version (`grep -i "page fault" intel-sdm-combined.txt`)
- `plan9/` — Plan 9 paper, namespaces paper, lexical names paper
- `ostep/` — all chapters as PDFs
- `little-os-book/little-os-book.pdf`
- `linux-subsystems/` — sparse Linux kernel clone: `fs/proc`, `fs/sysfs`, `drivers/tty`, `drivers/input`
- `specs/multiboot.pdf`, `specs/rfc1436-gopher.txt`, `specs/rfc4287-atom.txt`

---

## Repository

This workspace is local-only.

The working copy for Codex lives at:

`/media/nick/1TB_Storage1/projects/tech/DOS9_codex`

Do not write to `tech/DOS9` from this workspace. That directory is the
separate Claude Code build.

There is no push workflow for this copy. Keep all edits, build outputs,
and scratch work under `tech/DOS9_codex/`.

---

## Project Status

Phase 4c readers are complete; the remaining open item in that bucket is the aspirational 9P-style remote mount idea.

See `PLAN.md` for the current phase and task breakdown.
