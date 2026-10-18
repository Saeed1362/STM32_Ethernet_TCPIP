#ifndef EIP_SERVER_H
#define EIP_SERVER_H
#include "lwip/err.h"
/* Call once after MX_LWIP_Init(), before the main loop. */
err_t EIP_Server_Init(void);
#endif
