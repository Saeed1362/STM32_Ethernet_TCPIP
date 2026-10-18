#ifndef EIP_PROTOCOL_H
#define EIP_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define EIP_MAX_PACKET 512u
#define EIP_MAX_REPLY 128u
/* A complete 24-byte encapsulation header plus its declared payload is required.
 * Returns response length, 0 for no reply, or -1 for UnregisterSession/close.
 * ip is the board's IPv4 address in network byte order (four octets).
 * Called only from the single LwIP main-loop context. */
int EIP_Handle(const uint8_t *request, size_t length, uint8_t *reply,
               size_t capacity, uint32_t *session, const uint8_t ip[4]);
void EIP_LED_Set(uint8_t on);
uint8_t EIP_LED_Get(void);
#endif
