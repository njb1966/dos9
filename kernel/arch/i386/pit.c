#include <pit.h>
#include <io.h>
#include <stdint.h>

#define PIT_CH0   0x40
#define PIT_CMD   0x43
/* channel 0, lo/hi byte, mode 3 (square wave), binary */
#define PIT_MODE  0x36

void pit_init(uint32_t hz) {
    uint32_t divisor = 1193182u / hz;
    outb(PIT_CMD, PIT_MODE);
    outb(PIT_CH0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0, (uint8_t)(divisor >> 8));
}
