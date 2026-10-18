#include "http_server.h"
#include "lwip/tcp.h"
#include "main.h"
#include <string.h>
#include <stdio.h>

#define HTTP_PORT 80
#define HTTP_REQ_SIZE 1024

static struct tcp_pcb *http_pcb;

static struct {
    struct tcp_pcb *pcb;
    char request[HTTP_REQ_SIZE];
    size_t length;
    uint8_t responding;
} client;

static uint8_t led_state = 0;

static void http_error(void *arg, err_t err)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(err);

    memset(&client, 0, sizeof(client));
}

static void http_close(struct tcp_pcb *pcb)
{
    if (pcb == client.pcb) {
        client.pcb = NULL;
    }

    tcp_arg(pcb, NULL);
    tcp_recv(pcb, NULL);
    tcp_sent(pcb, NULL);
    tcp_err(pcb, NULL);

    if (tcp_close(pcb) != ERR_OK) {
        tcp_abort(pcb);
    }
}

static err_t http_sent(void *arg,
                       struct tcp_pcb *pcb,
                       u16_t len)
{
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(len);

    // For this small response, close after all
    // queued data has been acknowledged.
    if (pcb->unsent == NULL &&
        pcb->unacked == NULL) {
        http_close(pcb);
    }

    return ERR_OK;
}

static err_t http_recv(void *arg,
                       struct tcp_pcb *pcb,
                       struct pbuf *p,
                       err_t err)
{
    LWIP_UNUSED_ARG(arg);

    if (p == NULL) {
        http_close(pcb);
        return ERR_OK;
    }

    if (err != ERR_OK) {
        pbuf_free(p);
        return err;
    }

    tcp_recved(pcb, p->tot_len);

    if (client.responding) {
        pbuf_free(p);
        return ERR_OK;
    }

    size_t remaining =
        sizeof(client.request) - 1 - client.length;

    u16_t copy_len = p->tot_len < remaining
                     ? p->tot_len : (u16_t)remaining;

    pbuf_copy_partial(p,
                      client.request + client.length,
                      copy_len, 0);

    client.length += copy_len;
    client.request[client.length] = '\0';

    pbuf_free(p);

    // Wait for the complete HTTP header.
    if (strstr(client.request, "\r\n\r\n") == NULL) {
        if (client.length >= HTTP_REQ_SIZE - 1) {
            http_close(pcb);
        }
        return ERR_OK;
    }

    if (strncmp(client.request, "GET /on ", 8) == 0) {
        HAL_GPIO_WritePin(gLEG_GPIO_Port,
                          gLEG_Pin, GPIO_PIN_SET);
        led_state = 1;
    }
    else if (strncmp(client.request,
                     "GET /off ", 9) == 0) {
        HAL_GPIO_WritePin(gLEG_GPIO_Port,
                          gLEG_Pin, GPIO_PIN_RESET);
        led_state = 0;
    }

    const char *status =
        led_state ? "ON" : "OFF";

    char html[320];

    int html_len = snprintf(
        html, sizeof(html),
        "<!DOCTYPE html>"
        "<html><head>"
        "<meta name='viewport' "
        "content='width=device-width,initial-scale=1'>"
        "<title>STM32</title>"
        "</head><body>"
        "<h2>STM32 Web Server</h2>"
        "<p>LED is %s</p>"
        "<a href='/on'>Turn ON</a> | "
        "<a href='/off'>Turn OFF</a>"
        "</body></html>",
        status
    );

    if (html_len < 0 ||
        html_len >= (int)sizeof(html)) {
        http_close(pcb);
        return ERR_OK;
    }

    char response[512];

    int response_len = snprintf(
        response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        html_len, html
    );

    if (response_len < 0 ||
        response_len >= (int)sizeof(response)) {
        http_close(pcb);
        return ERR_OK;
    }

    client.responding = 1;

    err_t result = tcp_write(
        pcb,
        response,
        (u16_t)response_len,
        TCP_WRITE_FLAG_COPY
    );

    if (result != ERR_OK) {
        http_close(pcb);
        return ERR_OK;
    }

    tcp_sent(pcb, http_sent);
    tcp_output(pcb);

    return ERR_OK;
}

static err_t http_accept(void *arg,
                         struct tcp_pcb *pcb,
                         err_t err)
{
    LWIP_UNUSED_ARG(arg);

    if (err != ERR_OK || pcb == NULL) {
        return ERR_VAL;
    }

    // Single browser connection at a time.
    if (client.pcb != NULL) {
        tcp_abort(pcb);
        return ERR_ABRT;
    }

    memset(&client, 0, sizeof(client));
    client.pcb = pcb;

    tcp_arg(pcb, &client);
    tcp_recv(pcb, http_recv);
    tcp_err(pcb, http_error);

    return ERR_OK;
}

void HTTP_Server_Init(void)
{
    http_pcb = tcp_new();

    if (http_pcb == NULL) {
        Error_Handler();
    }

    if (tcp_bind(http_pcb,
                 IP_ADDR_ANY,
                 HTTP_PORT) != ERR_OK) {
        tcp_close(http_pcb);
        Error_Handler();
    }

    http_pcb = tcp_listen(http_pcb);

    if (http_pcb == NULL) {
        Error_Handler();
    }

    tcp_accept(http_pcb, http_accept);
}

