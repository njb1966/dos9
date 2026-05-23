#pragma once

/* Probe for RTL8139, bring up network stack, mount /net.
   Sends DHCP DISCOVER; caller should poll dhcp_done() after. */
int net_init(void);
