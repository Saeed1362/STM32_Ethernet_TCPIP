#include "tcp_server.h"
#include "lwip/tcp.h"
#include "main.h"
#include <string.h>

#define TCP_PORT 5000
#define CMD_MAX 32

static struct tcp_pcb *server_pcb;

typedef struct {
    char command[CMD_MAX];
    size_t length;
} client_state_t;

static client_state_t client;

static err_t tcp_received(void *arg,
                          struct tcp_pcb *tpcb,
                          struct pbuf *p,
                          err_t err)
{
    client_state_t *state = (client_state_t *)arg;

    if (p == NULL) {
        tcp_arg(tpcb, NULL);
        tcp_close(tpcb);
        return ERR_OK;
    }

    if (err != ERR_OK) {
        pbuf_free(p);
        return err;
    }

    tcp_recved(tpcb, p->tot_len);

    for (struct pbuf *q = p; q != NULL; q = q->next) {
        const char *data = (const char *)q->payload;

        for (u16_t i = 0; i < q->len; i++) {
            char ch = data[i];

            if (ch == '\r') {
                continue;
            }

            if (ch == '\n') {
                state->command[state->length] = '\0';

                const char *response;

                if (strcmp(state->command, "LED_ON") == 0) {
                    HAL_GPIO_WritePin(gLEG_GPIO_Port,
                                      gLEG_Pin,
                                      GPIO_PIN_SET);
                    response = "OK LED ON\n";
                }
                else if (strcmp(state->command, "LED_OFF") == 0) {
                    HAL_GPIO_WritePin(gLEG_GPIO_Port,
                                      gLEG_Pin,
                                      GPIO_PIN_RESET);
                    response = "OK LED OFF\n";
                }
                else {
                    response = "UNKNOWN COMMAND\n";
                }

                tcp_write(tpcb, response,
                          strlen(response),
                          TCP_WRITE_FLAG_COPY);

                tcp_output(tpcb);

                state->length = 0;
            }
            else if (state->length < CMD_MAX - 1) {
                state->command[state->length++] = ch;
            }
            else {
                state->length = 0;
            }
        }
    }

    pbuf_free(p);
    return ERR_OK;
}

static err_t tcp_accepted_callback(void *arg,
                          struct tcp_pcb *newpcb,
                          err_t err)
{
    if (err != ERR_OK) {
        return err;
    }

    memset(&client, 0, sizeof(client));

    tcp_arg(newpcb, &client);
    tcp_recv(newpcb, tcp_received);

    return ERR_OK;
}

void TCP_Server_Init(void)
{
    server_pcb = tcp_new();

    if (server_pcb == NULL) {
        return;
    }

    if (tcp_bind(server_pcb,
                 IP_ADDR_ANY,
                 TCP_PORT) != ERR_OK) {
        tcp_close(server_pcb);
        server_pcb = NULL;
        return;
    }

    server_pcb = tcp_listen(server_pcb);

    if (server_pcb != NULL) {
    	tcp_accept(server_pcb, tcp_accepted_callback);
    }
}


