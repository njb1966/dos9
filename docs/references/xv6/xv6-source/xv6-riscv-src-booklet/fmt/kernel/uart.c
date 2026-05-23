7200 //
7201 // low-level driver for 16550a UART.
7202 //
7203 
7204 #include "types.h"
7205 #include "param.h"
7206 #include "memlayout.h"
7207 #include "riscv.h"
7208 #include "spinlock.h"
7209 #include "proc.h"
7210 #include "defs.h"
7211 
7212 // the UART control registers are memory-mapped
7213 // at address UART0. this macro returns the
7214 // address of one of the registers.
7215 #define Reg(reg) ((volatile unsigned char *)(UART0 + (reg)))
7216 
7217 #define ReadReg(reg) (*(Reg(reg)))
7218 #define WriteReg(reg, v) (*(Reg(reg)) = (v))
7219 
7220 // the UART control registers.
7221 // some have different meanings for read vs write.
7222 // see http://byterunner.com/16550.html
7223 #define RHR 0                 // receive holding register (for input bytes)
7224 #define THR 0                 // transmit holding register (for output bytes)
7225 #define IER 1                 // interrupt enable register
7226 #define IER_RX_ENABLE (1<<0)
7227 #define IER_TX_ENABLE (1<<1)
7228 #define FCR 2                 // FIFO control register
7229 #define FCR_FIFO_ENABLE (1<<0)
7230 #define FCR_FIFO_CLEAR (3<<1) // clear the content of the two FIFOs
7231 #define ISR 2                 // interrupt status register
7232 #define LCR 3                 // line control register
7233 #define LCR_EIGHT_BITS (3<<0)
7234 #define LCR_BAUD_LATCH (1<<7) // special mode to set baud rate
7235 #define LSR 5                 // line status register
7236 #define LSR_RX_READY (1<<0)   // input is waiting to be read from RHR
7237 #define LSR_TX_IDLE (1<<5)    // THR can accept another character to send
7238 
7239 // for sending threads to synchronize with uart "ready" interrupts.
7240 static struct spinlock tx_lock;
7241 static int tx_busy;           // is the UART busy sending?
7242 static int tx_chan;           // &tx_chan is the "wait channel"
7243 
7244 extern volatile int panicking; // from printf.c
7245 extern volatile int panicked; // from printf.c
7246 
7247 
7248 
7249 
7250 void
7251 uartinit(void)
7252 {
7253   // disable interrupts.
7254   WriteReg(IER, 0x00);
7255 
7256   // special mode to set baud rate.
7257   WriteReg(LCR, LCR_BAUD_LATCH);
7258 
7259   // LSB for baud rate of 38.4K.
7260   WriteReg(0, 0x03);
7261 
7262   // MSB for baud rate of 38.4K.
7263   WriteReg(1, 0x00);
7264 
7265   // leave set-baud mode,
7266   // and set word length to 8 bits, no parity.
7267   WriteReg(LCR, LCR_EIGHT_BITS);
7268 
7269   // reset and enable FIFOs.
7270   WriteReg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);
7271 
7272   // enable transmit and receive interrupts.
7273   WriteReg(IER, IER_TX_ENABLE | IER_RX_ENABLE);
7274 
7275   initlock(&tx_lock, "uart");
7276 }
7277 
7278 // transmit buf[] to the uart. it blocks if the
7279 // uart is busy, so it cannot be called from
7280 // interrupts, only from write() system calls.
7281 void
7282 uartwrite(char buf[], int n)
7283 {
7284   acquire(&tx_lock);
7285 
7286   int i = 0;
7287   while(i < n){
7288     while(tx_busy != 0){
7289       // wait for a UART transmit-complete interrupt
7290       // to set tx_busy to 0.
7291       sleep(&tx_chan, &tx_lock);
7292     }
7293 
7294     WriteReg(THR, buf[i]);
7295     i += 1;
7296     tx_busy = 1;
7297   }
7298 
7299 
7300   release(&tx_lock);
7301 }
7302 
7303 
7304 // write a byte to the uart without using
7305 // interrupts, for use by kernel printf() and
7306 // to echo characters. it spins waiting for the uart's
7307 // output register to be empty.
7308 void
7309 uartputc_sync(int c)
7310 {
7311   if(panicking == 0)
7312     push_off();
7313 
7314   if(panicked){
7315     for(;;)
7316       ;
7317   }
7318 
7319   // wait for UART to set Transmit Holding Empty in LSR.
7320   while((ReadReg(LSR) & LSR_TX_IDLE) == 0)
7321     ;
7322   WriteReg(THR, c);
7323 
7324   if(panicking == 0)
7325     pop_off();
7326 }
7327 
7328 // try to read one input character from the UART.
7329 // return -1 if none is waiting.
7330 int
7331 uartgetc(void)
7332 {
7333   if(ReadReg(LSR) & LSR_RX_READY){
7334     // input data is ready.
7335     return ReadReg(RHR);
7336   } else {
7337     return -1;
7338   }
7339 }
7340 
7341 
7342 
7343 
7344 
7345 
7346 
7347 
7348 
7349 
7350 // handle a uart interrupt, raised because input has
7351 // arrived, or the uart is ready for more output, or
7352 // both. called from devintr().
7353 void
7354 uartintr(void)
7355 {
7356   ReadReg(ISR); // acknowledge the interrupt
7357 
7358   acquire(&tx_lock);
7359   if(ReadReg(LSR) & LSR_TX_IDLE){
7360     // UART finished transmitting; wake up sending thread.
7361     tx_busy = 0;
7362     wakeup(&tx_chan);
7363   }
7364   release(&tx_lock);
7365 
7366   // read and process incoming characters, if any.
7367   while(1){
7368     int c = uartgetc();
7369     if(c == -1)
7370       break;
7371     consoleintr(c);
7372   }
7373 }
7374 
7375 
7376 
7377 
7378 
7379 
7380 
7381 
7382 
7383 
7384 
7385 
7386 
7387 
7388 
7389 
7390 
7391 
7392 
7393 
7394 
7395 
7396 
7397 
7398 
7399 
