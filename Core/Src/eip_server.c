#include "eip_server.h"
#include "eip_protocol.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "main.h"
#include <string.h>

#if !NO_SYS
#error "This lab is intended for NO_SYS=1 and MX_LWIP_Process() in main()."
#endif

#define EIP_PORT 44818u
#define CLIENTS 2u
#define RX_CAP (2u * EIP_MAX_PACKET)
#define TX_CAP (4u * EIP_MAX_REPLY)
#define IDLE_MS 60000u

typedef struct {
    struct tcp_pcb *pcb;
    uint32_t session;
    uint32_t last_activity;
    uint16_t rx_n, tx_n;
    uint8_t closing, eof;
    uint8_t rx[RX_CAP];
    uint8_t tx[TX_CAP];
} eip_client_t;
static eip_client_t clients[CLIENTS];
static struct tcp_pcb *listener;
static err_t received(void *, struct tcp_pcb *, struct pbuf *, err_t);
static err_t sent(void *, struct tcp_pcb *, u16_t);
static err_t poll_client(void *, struct tcp_pcb *);
static void failed(void *, err_t);

void EIP_LED_Set(uint8_t on) {
    HAL_GPIO_WritePin(gLEG_GPIO_Port, gLEG_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
uint8_t EIP_LED_Get(void) {
    return HAL_GPIO_ReadPin(gLEG_GPIO_Port, gLEG_Pin) == GPIO_PIN_SET ? 1u : 0u;
}
static void attach(eip_client_t *c) {
    tcp_arg(c->pcb, c); tcp_recv(c->pcb, received);
    tcp_sent(c->pcb, sent); tcp_err(c->pcb, failed);
    tcp_poll(c->pcb, poll_client, 2); /* Approximately one second */
}
static void detach(struct tcp_pcb *pcb) {
    tcp_arg(pcb, NULL); tcp_recv(pcb, NULL); tcp_sent(pcb, NULL);
    tcp_err(pcb, NULL); tcp_poll(pcb, NULL, 0);
}
static err_t drop(eip_client_t *c) {
    struct tcp_pcb *pcb = c->pcb;
    detach(pcb); memset(c, 0, sizeof(*c)); tcp_abort(pcb);
    return ERR_ABRT;
}
static err_t flush(eip_client_t *c) {
    while (c->tx_n) {
        u16_t count = tcp_sndbuf(c->pcb);
        err_t e;
        if (!count) break;
        if (count > c->tx_n) count = c->tx_n;
        e = tcp_write(c->pcb, c->tx, count, TCP_WRITE_FLAG_COPY);
        if (e == ERR_MEM) break; /* Retry from sent/poll callback */
        if (e != ERR_OK) return e;
        c->tx_n = (uint16_t)(c->tx_n - count);
        memmove(c->tx, c->tx + count, c->tx_n);
    }
    /* LwIP retains copied bytes until acknowledged; its timers handle retry. */
    (void)tcp_output(c->pcb);
    return ERR_OK;
}
static err_t pump(eip_client_t *c) {
    uint8_t answer[EIP_MAX_REPLY], ip[4] = {0};
    if (netif_default) {
        const ip4_addr_t *a = netif_ip4_addr(netif_default);
        ip[0] = ip4_addr1(a); ip[1] = ip4_addr2(a);
        ip[2] = ip4_addr3(a); ip[3] = ip4_addr4(a);
    }
    if (flush(c) != ERR_OK) return drop(c);
    while (!c->closing && c->rx_n >= 24) {
        size_t bytes = 24u + (size_t)c->rx[2] + ((size_t)c->rx[3] << 8);
        int out_n;
        if (bytes > EIP_MAX_PACKET) return drop(c);
        if (c->rx_n < bytes || TX_CAP - c->tx_n < EIP_MAX_REPLY) break;
        out_n = EIP_Handle(c->rx, bytes, answer, sizeof(answer), &c->session, ip);
        c->rx_n = (uint16_t)(c->rx_n - bytes);
        memmove(c->rx, c->rx + bytes, c->rx_n);
        if (out_n < 0) { c->closing = 1; c->rx_n = 0; break; }
        if (out_n) {
            memcpy(c->tx + c->tx_n, answer, (size_t)out_n);
            c->tx_n = (uint16_t)(c->tx_n + out_n);
        }
        if (flush(c) != ERR_OK) return drop(c);
    }
    if (c->eof && !c->closing) {
        /* Finish complete buffered requests before closing our send side. */
        if (!c->rx_n) c->closing = 1;
        else if (TX_CAP - c->tx_n >= EIP_MAX_REPLY) return drop(c);
    }
    if (c->closing && c->tx_n == 0) {
        struct tcp_pcb *pcb = c->pcb;
        err_t e;
        detach(pcb);
        e = tcp_close(pcb);
        if (e == ERR_OK) memset(c, 0, sizeof(*c));
        else { attach(c); if (e != ERR_MEM) return drop(c); }
    }
    return ERR_OK;
}
static err_t received(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t e) {
    eip_client_t *c = (eip_client_t *)arg;
    if (!p) { c->eof = 1; return pump(c); }
    if (e != ERR_OK || c->closing || p->tot_len > RX_CAP - c->rx_n) {
        pbuf_free(p); return drop(c);
    }
    /* Handles chained pbufs; no assumption that payload is contiguous. */
    {
        u16_t count = p->tot_len;
        u16_t copied = pbuf_copy_partial(p, c->rx + c->rx_n, count, 0);
        pbuf_free(p);
        if (copied != count) return drop(c);
        c->rx_n = (uint16_t)(c->rx_n + count);
        tcp_recved(pcb, count);
    }
    c->last_activity = HAL_GetTick();
    return pump(c);
}
static err_t sent(void *arg, struct tcp_pcb *pcb, u16_t len) {
    eip_client_t *c = (eip_client_t *)arg;
    (void)pcb; (void)len;
    c->last_activity = HAL_GetTick();
    return pump(c);
}
static err_t poll_client(void *arg, struct tcp_pcb *pcb) {
    eip_client_t *c = (eip_client_t *)arg;
    (void)pcb;
    if ((uint32_t)(HAL_GetTick() - c->last_activity) > IDLE_MS) return drop(c);
    return pump(c);
}
static void failed(void *arg, err_t err) {
    (void)err;
    if (arg) memset(arg, 0, sizeof(eip_client_t)); /* PCB already freed by LwIP */
}
static err_t accepted(void *arg, struct tcp_pcb *pcb, err_t e) {
    unsigned i;
    (void)arg;
    if (e != ERR_OK || !pcb) return e;
    for (i = 0; i < CLIENTS; ++i) {
        if (!clients[i].pcb) {
            memset(&clients[i], 0, sizeof(clients[i]));
            clients[i].pcb = pcb;
            clients[i].last_activity = HAL_GetTick();
            attach(&clients[i]); return ERR_OK;
        }
    }
    tcp_abort(pcb); return ERR_ABRT;
}
err_t EIP_Server_Init(void) {
    struct tcp_pcb *pcb;
    err_t e;
    if (listener) return ERR_OK;
    pcb = tcp_new();
    if (!pcb) return ERR_MEM;
    e = tcp_bind(pcb, IP_ADDR_ANY, EIP_PORT);
    if (e != ERR_OK) { tcp_abort(pcb); return e; }
    listener = tcp_listen(pcb);
    if (!listener) { tcp_abort(pcb); return ERR_MEM; }
    tcp_accept(listener, accepted);
    return ERR_OK;
}
