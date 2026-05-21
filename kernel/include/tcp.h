#pragma once
#include <stdint.h>

#define TCP_MAX_CONNS  4
#define TCP_RX_BUF_SZ  4096

#define TCP_CLOSED      0
#define TCP_SYN_SENT    1
#define TCP_ESTABLISHED 2
#define TCP_FIN_WAIT    3   /* we sent FIN */
#define TCP_CLOSE_WAIT  4   /* they sent FIN */
#define TCP_TIME_WAIT   5

/* Called from ip_rx for incoming TCP segments. */
void tcp_rx(const void *pkt, uint16_t len, uint32_t src_ip, uint32_t dst_ip);

/* Allocate a connection slot (returns 0..TCP_MAX_CONNS-1 or -1). */
int tcp_alloc(void);

/* Initiate a TCP connection.  Blocks (via schedule()) until established
   or timeout.  Returns 0 on success, -1 on failure. */
int tcp_connect(int slot, uint32_t dst_ip, uint16_t dst_port);

/* Read up to len bytes from connection.  Blocks until data available
   or EOF.  Returns bytes read, 0 on EOF, -1 on error. */
int tcp_recv(int slot, void *buf, uint16_t len);

/* Send len bytes on connection.  Returns bytes sent or -1. */
int tcp_send(int slot, const void *buf, uint16_t len);

/* Initiate graceful close. */
void tcp_close(int slot);

/* Release slot back to pool (call after close completes). */
void tcp_free(int slot);

/* Current state string (for netfs status file). */
const char *tcp_state_str(int slot);
