#include "sensor_server.h"
#include "lwip/tcp.h"
#include "main.h"
#include <stdio.h>

#define SENSOR_PORT       6000
#define STREAM_PERIOD_MS  100

static struct tcp_pcb *server_pcb = NULL;
static struct tcp_pcb *client_pcb = NULL;

static uint32_t last_tick = 0;
static uint16_t sensor_value = 1000;

static void sensor_error(void *arg, err_t err)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(err);
    client_pcb = NULL;
}

static err_t sensor_recv(void *arg,
                         struct tcp_pcb *pcb,
                         struct pbuf *p,
                         err_t err)
{
    LWIP_UNUSED_ARG(arg);

    if (p == NULL) {
        if (pcb == client_pcb)
            client_pcb = NULL;

        tcp_arg(pcb, NULL);
        tcp_recv(pcb, NULL);
        tcp_err(pcb, NULL);

        if (tcp_close(pcb) != ERR_OK)
            tcp_abort(pcb);

        return ERR_OK;
    }

    if (err != ERR_OK) {
        pbuf_free(p);
        return err;
    }

    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);

    return ERR_OK;
}

static err_t sensor_accept(void *arg,
                           struct tcp_pcb *pcb,
                           err_t err)
{
    LWIP_UNUSED_ARG(arg);

    if (err != ERR_OK || pcb == NULL)
        return ERR_VAL;

    // Only one streaming client at a time.
    if (client_pcb != NULL) {
        tcp_abort(pcb);
        return ERR_ABRT;
    }

    client_pcb = pcb;
    last_tick = HAL_GetTick();

    tcp_arg(pcb, NULL);
    tcp_recv(pcb, sensor_recv);
    tcp_err(pcb, sensor_error);

    return ERR_OK;
}

void Sensor_Server_Init(void)
{
    server_pcb = tcp_new();

    if (server_pcb == NULL)
        Error_Handler();

    if (tcp_bind(server_pcb,
                 IP_ADDR_ANY,
                 SENSOR_PORT) != ERR_OK) {
        tcp_close(server_pcb);
        Error_Handler();
    }

    server_pcb = tcp_listen(server_pcb);

    if (server_pcb == NULL)
        Error_Handler();

    tcp_accept(server_pcb, sensor_accept);
}

void Sensor_Server_Process(void)
{
    if (client_pcb == NULL)
        return;

    uint32_t now = HAL_GetTick();

    if ((uint32_t)(now - last_tick) <
        STREAM_PERIOD_MS)
        return;

    last_tick = now;

    char message[64];

    int len = snprintf(
        message, sizeof(message),
        "%lu,%u\n",
        (unsigned long)now,
        (unsigned int)sensor_value
    );

    if (len <= 0 || len >= (int)sizeof(message))
        return;

    if (tcp_sndbuf(client_pcb) < (u16_t)len)
        return;

    err_t result = tcp_write(
        client_pcb,
        message,
        (u16_t)len,
        TCP_WRITE_FLAG_COPY
    );

    if (result == ERR_OK)
        tcp_output(client_pcb);

    sensor_value += 10;

    if (sensor_value > 4000)
        sensor_value = 1000;
}
