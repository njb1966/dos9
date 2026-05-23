1950 // Saved registers for kernel context switches.
1951 struct context {
1952   uint64 ra;
1953   uint64 sp;
1954 
1955   // callee-saved
1956   uint64 s0;
1957   uint64 s1;
1958   uint64 s2;
1959   uint64 s3;
1960   uint64 s4;
1961   uint64 s5;
1962   uint64 s6;
1963   uint64 s7;
1964   uint64 s8;
1965   uint64 s9;
1966   uint64 s10;
1967   uint64 s11;
1968 };
1969 
1970 // Per-CPU state.
1971 struct cpu {
1972   struct proc *proc;          // The process running on this cpu, or null.
1973   struct context context;     // swtch() here to enter scheduler().
1974   int noff;                   // Depth of push_off() nesting.
1975   int intena;                 // Were interrupts enabled before push_off()?
1976 };
1977 
1978 extern struct cpu cpus[NCPU];
1979 
1980 // per-process data for the trap handling code in trampoline.S.
1981 // sits in a page by itself just under the trampoline page in the
1982 // user page table. not specially mapped in the kernel page table.
1983 // uservec in trampoline.S saves user registers in the trapframe,
1984 // then initializes registers from the trapframe's
1985 // kernel_sp, kernel_hartid, kernel_satp, and jumps to kernel_trap.
1986 // usertrapret() and userret in trampoline.S set up
1987 // the trapframe's kernel_*, restore user registers from the
1988 // trapframe, switch to the user page table, and enter user space.
1989 // the trapframe includes callee-saved user registers like s0-s11 because the
1990 // return-to-user path via usertrapret() doesn't return through
1991 // the entire kernel call stack.
1992 struct trapframe {
1993   /*   0 */ uint64 kernel_satp;   // kernel page table
1994   /*   8 */ uint64 kernel_sp;     // top of process's kernel stack
1995   /*  16 */ uint64 kernel_trap;   // usertrap()
1996   /*  24 */ uint64 epc;           // saved user program counter
1997   /*  32 */ uint64 kernel_hartid; // saved kernel tp
1998   /*  40 */ uint64 ra;
1999   /*  48 */ uint64 sp;
2000   /*  56 */ uint64 gp;
2001   /*  64 */ uint64 tp;
2002   /*  72 */ uint64 t0;
2003   /*  80 */ uint64 t1;
2004   /*  88 */ uint64 t2;
2005   /*  96 */ uint64 s0;
2006   /* 104 */ uint64 s1;
2007   /* 112 */ uint64 a0;
2008   /* 120 */ uint64 a1;
2009   /* 128 */ uint64 a2;
2010   /* 136 */ uint64 a3;
2011   /* 144 */ uint64 a4;
2012   /* 152 */ uint64 a5;
2013   /* 160 */ uint64 a6;
2014   /* 168 */ uint64 a7;
2015   /* 176 */ uint64 s2;
2016   /* 184 */ uint64 s3;
2017   /* 192 */ uint64 s4;
2018   /* 200 */ uint64 s5;
2019   /* 208 */ uint64 s6;
2020   /* 216 */ uint64 s7;
2021   /* 224 */ uint64 s8;
2022   /* 232 */ uint64 s9;
2023   /* 240 */ uint64 s10;
2024   /* 248 */ uint64 s11;
2025   /* 256 */ uint64 t3;
2026   /* 264 */ uint64 t4;
2027   /* 272 */ uint64 t5;
2028   /* 280 */ uint64 t6;
2029 };
2030 
2031 enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };
2032 
2033 // Per-process state
2034 struct proc {
2035   struct spinlock lock;
2036 
2037   // p->lock must be held when using these:
2038   enum procstate state;        // Process state
2039   void *chan;                  // If non-zero, sleeping on chan
2040   int killed;                  // If non-zero, have been killed
2041   int xstate;                  // Exit status to be returned to parent's wait
2042   int pid;                     // Process ID
2043 
2044   // wait_lock must be held when using this:
2045   struct proc *parent;         // Parent process
2046 
2047 
2048 
2049 
2050   // these are private to the process, so p->lock need not be held.
2051   uint64 kstack;               // Virtual address of kernel stack
2052   uint64 sz;                   // Size of process memory (bytes)
2053   pagetable_t pagetable;       // User page table
2054   struct trapframe *trapframe; // data page for trampoline.S
2055   struct context context;      // swtch() here to run process
2056   struct file *ofile[NOFILE];  // Open files
2057   struct inode *cwd;           // Current directory
2058   char name[16];               // Process name (debugging)
2059 };
2060 
2061 
2062 
2063 
2064 
2065 
2066 
2067 
2068 
2069 
2070 
2071 
2072 
2073 
2074 
2075 
2076 
2077 
2078 
2079 
2080 
2081 
2082 
2083 
2084 
2085 
2086 
2087 
2088 
2089 
2090 
2091 
2092 
2093 
2094 
2095 
2096 
2097 
2098 
2099 
