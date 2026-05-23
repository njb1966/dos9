/*
 * rtl8139.c — RTL8139 PCI NIC driver.
 *
 * RX: single 8KB ring (WRAP=1 / no-wrap mode) + 1500-byte overflow area.
 *     Packets are always contiguous in memory; no split-packet handling needed.
 * TX: 4 static TX descriptors, round-robin.
 * IRQ: read from PCI config; registered with pic_unmask_irq.
 */

#include <rtl8139.h>
#include <pci.h>
#include <netif.h>
#include <net.h>
#include <io.h>
#include <pic.h>
#include <idt.h>
#include <kernel.h>
#include <string.h>
#include <terminal.h>
#include <stdint.h>
#include <stddef.h>

/* PCI IDs */
#define RTL_VENDOR  0x10EC
#define RTL_DEVICE  0x8139

/* Register offsets (relative to I/O base from BAR0) */
#define REG_IDR0    0x00    /* MAC bytes 0-5 */
#define REG_TSD0    0x10    /* TX status desc 0-3 (32-bit each, stride 4) */
#define REG_TSAD0   0x20    /* TX start addr  0-3 (32-bit each, stride 4) */
#define REG_RBSTART 0x30    /* RX buffer start (32-bit physical address) */
#define REG_CR      0x37    /* Command register */
#define REG_CAPR    0x38    /* Current Address of Packet Read (16-bit) */
#define REG_CBR     0x3A    /* Current Buffer Address (NIC write ptr, RO) */
#define REG_IMR     0x3C    /* Interrupt Mask Register (16-bit) */
#define REG_ISR     0x3E    /* Interrupt Status Register (16-bit) */
#define REG_TCR     0x40    /* TX Configuration (32-bit) */
#define REG_RCR     0x44    /* RX Configuration (32-bit) */
#define REG_CONFIG1 0x52    /* Config register 1 */

/* CR bits */
#define CR_RST  0x10        /* software reset — clears when done */
#define CR_RE   0x08        /* receiver enable */
#define CR_TE   0x04        /* transmitter enable */

/* ISR / IMR bits */
#define ISR_ROK  0x0001     /* RX OK */
#define ISR_RER  0x0002     /* RX error */
#define ISR_TOK  0x0004     /* TX OK */
#define ISR_TER  0x0008     /* TX error */

/* RCR config: accept all (0xF), DMA burst 256 (4<<8), ring 8KB (0<<11),
   no-wrap (1<<7) */
#define RCR_VAL  (0x0Fu | (1u << 7) | (4u << 8))

/* TX status: OWN bit (set = host owns descriptor) */
#define TSD_OWN  (1u << 13)

/* Ring + overflow buffer */
#define RX_RING_SZ   8192u
#define TX_BUF_SZ    1536u

static uint8_t  rx_buf[RX_RING_SZ + 16 + 1500] __attribute__((aligned(4)));
static uint8_t  tx_buf[4][TX_BUF_SZ]            __attribute__((aligned(4)));

static uint16_t g_iobase;
static uint8_t  g_irq;
static uint32_t g_rx_ptr;   /* our read cursor into rx_buf, wraps at RX_RING_SZ */
static int      g_tx_cur;   /* current TX descriptor (0-3) */

/* ── Low-level I/O helpers ───────────────────────────────────────────────── */

static inline uint8_t  r8 (uint16_t r) { return inb (g_iobase + r); }
static inline uint16_t r16(uint16_t r) { return inw (g_iobase + r); }
static inline uint32_t r32(uint16_t r) { return inl (g_iobase + r); }
static inline void     w8 (uint16_t r, uint8_t  v) { outb(g_iobase + r, v); }
static inline void     w16(uint16_t r, uint16_t v) { outw(g_iobase + r, v); }
static inline void     w32(uint16_t r, uint32_t v) { outl(g_iobase + r, v); }

/* ── TX ──────────────────────────────────────────────────────────────────── */

static void rtl_send(const void *frame, uint16_t len) {
    if (len > TX_BUF_SZ) return;

    /* Wait for OWN bit (host owns == transmit complete). */
    uint32_t timeout = 100000;
    while (!(r32((uint16_t)(REG_TSD0 + g_tx_cur * 4)) & TSD_OWN) && --timeout)
        ;
    if (!timeout) return;

    memcpy(tx_buf[g_tx_cur], frame, len);

    /* Minimum Ethernet frame is 60 bytes. */
    uint16_t send_len = len < 60 ? 60 : len;
    if (send_len > len)
        memset(tx_buf[g_tx_cur] + len, 0, (size_t)(send_len - len));

    /* Writing to TSD clears OWN (bit 13) and starts the transmit.
       Increment g_tx_cur BEFORE the write so a re-entrant send from
       an IRQ that fires synchronously during the TSD write uses the
       next descriptor, not this one. */
    int slot = g_tx_cur;
    g_tx_cur = (g_tx_cur + 1) & 3;

    w32((uint16_t)(REG_TSAD0 + slot * 4), PHYS(tx_buf[slot]));
    w32((uint16_t)(REG_TSD0  + slot * 4), (uint32_t)send_len);
}

/* ── RX IRQ handler ──────────────────────────────────────────────────────── */

static void rtl_irq_handler(struct registers *r) {
    (void)r;
    uint16_t isr = r16(REG_ISR);
    w16(REG_ISR, isr);   /* clear interrupts by writing back */

    if (!(isr & ISR_ROK)) return;

    /* Drain all available packets. */
    while (!(r8(REG_CR) & 0x01)) {   /* BUFE bit: 0 = buffer not empty */
        /* Packet header: [status 16-bit][length 16-bit] */
        uint16_t pkt_status = *(uint16_t *)(rx_buf + g_rx_ptr);
        uint16_t pkt_len    = *(uint16_t *)(rx_buf + g_rx_ptr + 2);
        uint32_t remain      = (uint32_t)sizeof(rx_buf) - g_rx_ptr;

        if (!(pkt_status & 0x0001)) break;  /* ROK not set in header */

        if (remain <= 4u) {
            g_rx_ptr = 0;
            w16(REG_CAPR, (uint16_t)(g_rx_ptr - 16));
            continue;
        }
        if (pkt_len < 4u) pkt_len = 4u;
        if ((uint32_t)pkt_len > remain - 4u)
            pkt_len = (uint16_t)(remain - 4u);

        uint16_t data_len = (uint16_t)(pkt_len - 4u);    /* strip 4-byte CRC */
        uint8_t *data     = rx_buf + g_rx_ptr + 4;

        netif_receive(data, data_len);

        /* Advance read pointer (align to 4 bytes), tell NIC. */
        g_rx_ptr = (g_rx_ptr + 4 + pkt_len + 3) & ~3u;
        if (g_rx_ptr >= RX_RING_SZ) g_rx_ptr -= RX_RING_SZ;
        w16(REG_CAPR, (uint16_t)(g_rx_ptr - 16));
    }
}

/* ── init ────────────────────────────────────────────────────────────────── */

int rtl8139_init(void) {
    pci_device_t pdev;
    if (!pci_find_device(RTL_VENDOR, RTL_DEVICE, &pdev)) return 0;

    /* BAR0 is I/O space: mask off the I/O indicator bit (bit 0). */
    g_iobase = (uint16_t)(pdev.bar[0] & ~1u);
    g_irq    = pdev.irq;

    pci_enable_busmaster(pdev.bus, pdev.dev, pdev.func);

    /* Power on. */
    w8(REG_CONFIG1, 0x00);

    /* Software reset. */
    w8(REG_CR, CR_RST);
    uint32_t t = 100000;
    while ((r8(REG_CR) & CR_RST) && --t)
        ;

    /* Read MAC address. */
    for (int i = 0; i < 6; i++)
        g_netif.mac[i] = r8((uint16_t)(REG_IDR0 + i));

    /* Set up RX ring. */
    g_rx_ptr = 0;
    w32(REG_RBSTART, PHYS(rx_buf));

    /* Unmask ROK+TOK interrupts. */
    w16(REG_IMR, ISR_ROK | ISR_TOK);

    /* RCR: accept broadcast+all, 8KB ring, no-wrap. */
    w32(REG_RCR, RCR_VAL);

    /* TCR: default DMA burst. */
    w32(REG_TCR, 0x00000600u);

    /* Init TX descriptor addresses and mark as host-owned. */
    for (int i = 0; i < 4; i++) {
        w32((uint16_t)(REG_TSAD0 + i * 4), PHYS(tx_buf[i]));
        w32((uint16_t)(REG_TSD0  + i * 4), TSD_OWN);   /* host owns = free */
    }
    g_tx_cur = 0;

    /* Enable RX + TX. */
    w8(REG_CR, CR_RE | CR_TE);

    /* Register IRQ. */
    irq_register(g_irq, rtl_irq_handler);
    pic_unmask_irq(g_irq);

    /* Wire send function into netif. */
    g_netif.send = rtl_send;

    terminal_write("[RTL8139] MAC=");
    const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 6; i++) {
        terminal_putchar(hex[g_netif.mac[i] >> 4]);
        terminal_putchar(hex[g_netif.mac[i] & 0xF]);
        if (i < 5) terminal_putchar(':');
    }
    terminal_write("  IRQ=");
    terminal_writedec(g_irq);
    terminal_write("\n");

    return 1;
}
