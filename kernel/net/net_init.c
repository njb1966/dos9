#include <net_init.h>
#include <rtl8139.h>
#include <netfs.h>
#include <dhcp.h>
#include <terminal.h>

int net_init(void) {
    if (!rtl8139_init()) {
        terminal_write("[NET] no RTL8139 found\n");
        return 0;
    }
    netfs_init();
    terminal_write("[NET] /net mounted\n");
    dhcp_start();
    terminal_write("[DHCP] DISCOVER sent\n");
    return 1;
}
