# DOS/9 — Claude Code Context

DOS/9 is a hobbyist OS kernel targeting 32-bit x86. It is conceptually inspired by Plan 9 (per-process namespaces, everything-is-a-file) but runs as a native kernel, not a hosted process. The goal is a working kernel with a shell, VFS, process model, and eventually Gemini/Gopher client support.

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
| xv6 source (x86) | https://github.com/mit-pdos/xv6-public | Teaching OS — use for "how is this structured?" lookups. Local copy in docs/references/xv6/. |

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

**GitHub only.** This project lives at:

```
https://github.com/njb1966/dos9
git remote: github-dos9
```

The local working copy is inside a monorepo at
`/media/nick/1TB_Storage1/projects/tech/DOS9` (also symlinked from
`/home/nick/projects/tech/DOS9`). The git root is one level up
(`/media/nick/1TB_Storage1/projects`), so all git commands must be
run from there.

**Push workflow:**
```bash
# From /media/nick/1TB_Storage1/projects (the monorepo root):
git add --sparse tech/DOS9/<path>   # stage new files outside kernel/
git add tech/DOS9/<path>            # stage files in tracked sparse paths
git commit -m "..."
git subtree split --prefix=tech/DOS9 -b dos9-push
git push --force github-dos9 dos9-push:main
git branch -D dos9-push
```

**Do NOT push to Gitea (`origin`).** `origin` is the infrastructure
monorepo at `100.116.111.80:3000` — a different repo for a different
purpose. All DOS9 work goes to GitHub only.

---

## Project Status

See `PLAN.md` for current phase and task breakdown.
