6950 //
6951 // Console input and output, to the uart.
6952 // Reads are line at a time.
6953 // Implements special input characters:
6954 //   newline -- end of line
6955 //   control-h -- backspace
6956 //   control-u -- kill line
6957 //   control-d -- end of file
6958 //   control-p -- print process list
6959 //
6960 
6961 #include <stdarg.h>
6962 
6963 #include "types.h"
6964 #include "param.h"
6965 #include "spinlock.h"
6966 #include "sleeplock.h"
6967 #include "fs.h"
6968 #include "file.h"
6969 #include "memlayout.h"
6970 #include "riscv.h"
6971 #include "defs.h"
6972 #include "proc.h"
6973 
6974 #define BACKSPACE 0x100  // erase the last output character
6975 #define C(x)  ((x)-'@')  // Control-x
6976 
6977 //
6978 // send one character to the uart, but don't use
6979 // interrupts or sleep(). safe to be called from
6980 // interrupts, e.g. by printf and to echo input
6981 // characters.
6982 //
6983 void
6984 consputc(int c)
6985 {
6986   if(c == BACKSPACE){
6987     // if the user typed backspace, overwrite with a space.
6988     uartputc_sync('\b'); uartputc_sync(' '); uartputc_sync('\b');
6989   } else {
6990     uartputc_sync(c);
6991   }
6992 }
6993 
6994 
6995 
6996 
6997 
6998 
6999 
7000 struct {
7001   struct spinlock lock;
7002 
7003   // input circular buffer
7004 #define INPUT_BUF_SIZE 128
7005   char buf[INPUT_BUF_SIZE];
7006   uint r;  // Read index
7007   uint w;  // Write index
7008   uint e;  // Edit index
7009 } cons;
7010 
7011 //
7012 // user write() system calls to the console go here.
7013 // uses sleep() and UART interrupts.
7014 //
7015 int
7016 consolewrite(int user_src, uint64 src, int n)
7017 {
7018   char buf[32]; // move batches from user space to uart.
7019   int i = 0;
7020 
7021   while(i < n){
7022     int nn = sizeof(buf);
7023     if(nn > n - i)
7024       nn = n - i;
7025     if(either_copyin(buf, user_src, src+i, nn) == -1)
7026       break;
7027     uartwrite(buf, nn);
7028     i += nn;
7029   }
7030 
7031   return i;
7032 }
7033 
7034 //
7035 // user read()s from the console go here.
7036 // copy (up to) a whole input line to dst.
7037 // user_dst indicates whether dst is a user
7038 // or kernel address.
7039 //
7040 int
7041 consoleread(int user_dst, uint64 dst, int n)
7042 {
7043   uint target;
7044   int c;
7045   char cbuf;
7046 
7047   target = n;
7048   acquire(&cons.lock);
7049   while(n > 0){
7050     // wait until interrupt handler has put some
7051     // input into cons.buffer.
7052     while(cons.r == cons.w){
7053       if(killed(myproc())){
7054         release(&cons.lock);
7055         return -1;
7056       }
7057       sleep(&cons.r, &cons.lock);
7058     }
7059 
7060     c = cons.buf[cons.r++ % INPUT_BUF_SIZE];
7061 
7062     if(c == C('D')){  // end-of-file
7063       if(n < target){
7064         // Save ^D for next time, to make sure
7065         // caller gets a 0-byte result.
7066         cons.r--;
7067       }
7068       break;
7069     }
7070 
7071     // copy the input byte to the user-space buffer.
7072     cbuf = c;
7073     if(either_copyout(user_dst, dst, &cbuf, 1) == -1)
7074       break;
7075 
7076     dst++;
7077     --n;
7078 
7079     if(c == '\n'){
7080       // a whole line has arrived, return to
7081       // the user-level read().
7082       break;
7083     }
7084   }
7085   release(&cons.lock);
7086 
7087   return target - n;
7088 }
7089 
7090 
7091 
7092 
7093 
7094 
7095 
7096 
7097 
7098 
7099 
7100 //
7101 // the console input interrupt handler.
7102 // uartintr() calls this for each input character.
7103 // do erase/kill processing, append to cons.buf,
7104 // wake up consoleread() if a whole line has arrived.
7105 //
7106 void
7107 consoleintr(int c)
7108 {
7109   acquire(&cons.lock);
7110 
7111   switch(c){
7112   case C('P'):  // Print process list.
7113     procdump();
7114     break;
7115   case C('U'):  // Kill line.
7116     while(cons.e != cons.w &&
7117           cons.buf[(cons.e-1) % INPUT_BUF_SIZE] != '\n'){
7118       cons.e--;
7119       consputc(BACKSPACE);
7120     }
7121     break;
7122   case C('H'): // Backspace
7123   case '\x7f': // Delete key
7124     if(cons.e != cons.w){
7125       cons.e--;
7126       consputc(BACKSPACE);
7127     }
7128     break;
7129   default:
7130     if(c != 0 && cons.e-cons.r < INPUT_BUF_SIZE){
7131       c = (c == '\r') ? '\n' : c;
7132 
7133       // echo back to the user.
7134       consputc(c);
7135 
7136       // store for consumption by consoleread().
7137       cons.buf[cons.e++ % INPUT_BUF_SIZE] = c;
7138 
7139       if(c == '\n' || c == C('D') || cons.e-cons.r == INPUT_BUF_SIZE){
7140         // wake up consoleread() if a whole line (or end-of-file)
7141         // has arrived.
7142         cons.w = cons.e;
7143         wakeup(&cons.r);
7144       }
7145     }
7146     break;
7147   }
7148 
7149 
7150   release(&cons.lock);
7151 }
7152 
7153 void
7154 consoleinit(void)
7155 {
7156   initlock(&cons.lock, "cons");
7157 
7158   uartinit();
7159 
7160   // connect read and write system calls
7161   // to consoleread and consolewrite.
7162   devsw[CONSOLE].read = consoleread;
7163   devsw[CONSOLE].write = consolewrite;
7164 }
7165 
7166 
7167 
7168 
7169 
7170 
7171 
7172 
7173 
7174 
7175 
7176 
7177 
7178 
7179 
7180 
7181 
7182 
7183 
7184 
7185 
7186 
7187 
7188 
7189 
7190 
7191 
7192 
7193 
7194 
7195 
7196 
7197 
7198 
7199 
