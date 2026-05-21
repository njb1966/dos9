#pragma once
#include <stdint.h>

void icmp_rx(const void *pkt, uint16_t len, uint32_t src_ip);
void icmp_ping(uint32_t dst_ip, uint16_t seq);
