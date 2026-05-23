#include <procfs.h>
#include <process.h>
#include <vfs.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

/* ── helpers ─────────────────────────────────────────────────────────── */

static uint32_t uint_to_str(uint32_t n, char *buf) {
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return 1; }
    char tmp[12];
    uint32_t i = 0;
    while (n) { tmp[i++] = '0' + (n % 10); n /= 10; }
    uint32_t len = i;
    for (uint32_t j = 0; j < len; j++) buf[j] = tmp[len - 1 - j];
    buf[len] = '\0';
    return len;
}

static uint32_t str_append(char *dst, uint32_t pos, const char *src) {
    while (*src) dst[pos++] = *src++;
    return pos;
}

static int parse_pid(const char *name, uint32_t *pid_out) {
    uint32_t pid = 0;
    if (!name || !*name) return -1;
    while (*name >= '0' && *name <= '9') {
        uint32_t digit = (uint32_t)(*name++ - '0');
        if (pid > 429496729u || (pid == 429496729u && digit > 5u))
            return -1;
        pid = pid * 10u + digit;
    }
    if (*name != '\0') return -1;
    *pid_out = pid;
    return 0;
}

static uint32_t gen_status(int idx, char *buf) {
    process_t *p = process_get(idx);
    if (!p) return 0;
    char num[12];
    uint32_t pos = 0;
    pos = str_append(buf, pos, "name:  ");
    pos = str_append(buf, pos, p->name);
    buf[pos++] = '\n';
    pos = str_append(buf, pos, "pid:   ");
    uint_to_str(p->pid, num);
    pos = str_append(buf, pos, num);
    buf[pos++] = '\n';
    pos = str_append(buf, pos, "state: ");
    pos = str_append(buf, pos,
        p->state == PROC_RUNNING ? "running" :
        p->state == PROC_READY   ? "ready"   : "dead");
    buf[pos++] = '\n';
    return pos;
}

/* ── /proc/<pid>/status ──────────────────────────────────────────────── */

static int status_read(vnode_t *v, void *buf, uint32_t off, uint32_t len) {
    int idx = (int)(uint32_t)(uintptr_t)v->priv;
    char tmp[128];
    uint32_t total = gen_status(idx, tmp);
    if (off >= total) return 0;
    uint32_t avail = total - off;
    uint32_t n = (avail < len) ? avail : len;
    memcpy(buf, tmp + off, n);
    return (int)n;
}

static fs_ops_t status_ops = { .read = status_read };

/* ── /proc/<pid>/ ────────────────────────────────────────────────────── */

static vnode_t pid_dir_vnodes[MAX_PROCS];
static vnode_t pid_status_vnodes[MAX_PROCS];

static vnode_t *pid_dir_lookup(vnode_t *dir, const char *name) {
    int i = (int)(uint32_t)(uintptr_t)dir->priv;
    if (strcmp(name, "status") == 0) return &pid_status_vnodes[i];
    return NULL;
}

static int pid_dir_readdir(vnode_t *dir, uint32_t idx,
                            char *name_out, uint32_t nmax) {
    (void)dir;
    if (idx == 0) {
        if (nmax == 0) return -1;
        uint32_t i = 0;
        while (i + 1 < nmax && "status"[i]) {
            name_out[i] = "status"[i];
            i++;
        }
        name_out[i] = '\0';
        return 0;
    }
    return -1;
}

static fs_ops_t pid_dir_ops = { .lookup = pid_dir_lookup, .readdir = pid_dir_readdir };

/* ── /proc/ ──────────────────────────────────────────────────────────── */

static vnode_t *proc_root_lookup(vnode_t *dir, const char *name) {
    (void)dir;
    uint32_t pid = 0;
    if (parse_pid(name, &pid) < 0) return NULL;
    int n = process_count();
    for (int i = 0; i < n; i++) {
        process_t *p = process_get(i);
        if (p && p->state != PROC_DEAD && p->pid == pid)
            return &pid_dir_vnodes[i];
    }
    return NULL;
}

static int proc_root_readdir(vnode_t *dir, uint32_t idx,
                              char *name_out, uint32_t nmax) {
    (void)dir;
    uint32_t active = 0;
    int n = process_count();
    for (int i = 0; i < n; i++) {
        process_t *p = process_get(i);
        if (!p || p->state == PROC_DEAD) continue;
        if (active == idx) {
            char num[12];
            uint_to_str(p->pid, num);
            if (nmax == 0) return -1;
            uint32_t j = 0;
            while (j + 1 < nmax && num[j]) {
                name_out[j] = num[j];
                j++;
            }
            name_out[j] = '\0';
            return 0;
        }
        active++;
    }
    return -1;
}

/* rm /proc/<pid> kills the process — the §1.1 commitment, made typeable. */
static int proc_root_unlink(vnode_t *dir, const char *name) {
    (void)dir;
    uint32_t pid = 0;
    if (parse_pid(name, &pid) < 0) return -1;
    return process_kill(pid);
}

static fs_ops_t proc_root_ops = {
    .lookup  = proc_root_lookup,
    .readdir = proc_root_readdir,
    .unlink  = proc_root_unlink,
};

static vnode_t procfs_root = {
    .type = VTYPE_DIR, .size = 0, .priv = NULL, .ops = &proc_root_ops,
};

void procfs_init(void) {
    for (int i = 0; i < MAX_PROCS; i++) {
        pid_dir_vnodes[i].type = VTYPE_DIR;
        pid_dir_vnodes[i].size = 0;
        pid_dir_vnodes[i].priv = (void *)(uintptr_t)i;
        pid_dir_vnodes[i].ops  = &pid_dir_ops;

        pid_status_vnodes[i].type = VTYPE_FILE;
        pid_status_vnodes[i].size = 0;
        pid_status_vnodes[i].priv = (void *)(uintptr_t)i;
        pid_status_vnodes[i].ops  = &status_ops;
    }
    vfs_mount("/proc", &procfs_root);
}
