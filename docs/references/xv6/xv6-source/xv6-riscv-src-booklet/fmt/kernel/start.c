1050 #include "types.h"
1051 #include "param.h"
1052 #include "memlayout.h"
1053 #include "riscv.h"
1054 #include "defs.h"
1055 
1056 void main();
1057 void timerinit();
1058 
1059 // entry.S needs one stack per CPU.
1060 __attribute__ ((aligned (16))) char stack0[4096 * NCPU];
1061 
1062 // entry.S jumps here in machine mode on stack0.
1063 void
1064 start()
1065 {
1066   // set M Previous Privilege mode to Supervisor, for mret.
1067   unsigned long x = r_mstatus();
1068   x &= ~MSTATUS_MPP_MASK;
1069   x |= MSTATUS_MPP_S;
1070   w_mstatus(x);
1071 
1072   // set M Exception Program Counter to main, for mret.
1073   // requires gcc -mcmodel=medany
1074   w_mepc((uint64)main);
1075 
1076   // disable paging for now.
1077   w_satp(0);
1078 
1079   // delegate all interrupts and exceptions to supervisor mode.
1080   w_medeleg(0xffff);
1081   w_mideleg(0xffff);
1082   w_sie(r_sie() | SIE_SEIE | SIE_STIE);
1083 
1084   // configure Physical Memory Protection to give supervisor mode
1085   // access to all of physical memory.
1086   w_pmpaddr0(0x3fffffffffffffull);
1087   w_pmpcfg0(0xf);
1088 
1089   // ask for clock interrupts.
1090   timerinit();
1091 
1092   // keep each CPU's hartid in its tp register, for cpuid().
1093   int id = r_mhartid();
1094   w_tp(id);
1095 
1096   // switch to supervisor mode and jump to main().
1097   asm volatile("mret");
1098 }
1099 
1100 // ask each hart to generate timer interrupts.
1101 void
1102 timerinit()
1103 {
1104   // enable supervisor-mode timer interrupts.
1105   w_mie(r_mie() | MIE_STIE);
1106 
1107   // enable the sstc extension (i.e. stimecmp).
1108   w_menvcfg(r_menvcfg() | (1L << 63));
1109 
1110   // allow supervisor to use stimecmp and time.
1111   w_mcounteren(r_mcounteren() | 2);
1112 
1113   // ask for the very first timer interrupt.
1114   w_stimecmp(r_time() + 1000000);
1115 }
1116 
1117 
1118 
1119 
1120 
1121 
1122 
1123 
1124 
1125 
1126 
1127 
1128 
1129 
1130 
1131 
1132 
1133 
1134 
1135 
1136 
1137 
1138 
1139 
1140 
1141 
1142 
1143 
1144 
1145 
1146 
1147 
1148 
1149 
