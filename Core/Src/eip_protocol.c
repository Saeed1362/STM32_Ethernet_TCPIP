/* Educational EtherNet/IP explicit-messaging subset. Not a complete adapter.
 * Original lab code; no OpENer source is incorporated. */
#include "eip_protocol.h"
#include <string.h>

static const char product_name[] = "STM32 EtherNetIP Lab";
static uint32_t next_session = 0x12340000u;
static uint16_t r16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t r32(const uint8_t *p) {
    return (uint32_t)r16(p) | ((uint32_t)r16(p + 2) << 16);
}
static void w16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}
static void w32(uint8_t *p, uint32_t v) {
    w16(p, (uint16_t)v); w16(p + 2, (uint16_t)(v >> 16));
}
static size_t identity_attribute(uint8_t attr, uint8_t *out) {
    switch (attr) {
    case 1: w16(out, 0); return 2; /* Lab placeholder, not an ODVA vendor ID */
    case 2: w16(out, 0); return 2; /* Generic device */
    case 3: w16(out, 1); return 2; /* Lab product code */
    case 4: out[0] = 1; out[1] = 0; return 2;
    case 5: w16(out, 0); return 2;
    case 6: w32(out, 0x12345678u); return 4;
    case 7:
        out[0] = (uint8_t)(sizeof(product_name) - 1u);
        memcpy(out + 1, product_name, sizeof(product_name) - 1u);
        return sizeof(product_name);
    default: return 0;
    }
}
static size_t cip_reply(const uint8_t *q, size_t n, uint8_t *a) {
    size_t path_end, pos, data_n;
    uint8_t cls = 0, inst = 0, attr = 0, seen = 0;
    a[0] = n ? (uint8_t)(q[0] | 0x80u) : 0x80u;
    a[1] = 0; a[2] = 0; a[3] = 0;
    if (n < 2) { a[2] = 0x13; return 4; }
    path_end = 2u + 2u * q[1];
    if (path_end > n) { a[2] = 0x13; return 4; }
    /* This lab implements only 8-bit logical class/instance/attribute paths. */
    for (pos = 2; pos < path_end; pos += 2) {
        uint8_t bit;
        switch (q[pos]) {
        case 0x20: bit = 1; cls = q[pos + 1]; break;
        case 0x24: bit = 2; inst = q[pos + 1]; break;
        case 0x30: bit = 4; attr = q[pos + 1]; break;
        default: a[2] = 0x04; return 4;
        }
        if (seen & bit) { a[2] = 0x04; return 4; }
        seen |= bit;
    }
    if (seen != 7 || inst != 1 || (cls != 1 && cls != 0x64)) {
        a[2] = 0x05; return 4;
    }
    if (q[0] != 0x0e && q[0] != 0x10) { a[2] = 0x08; return 4; }
    data_n = n - path_end;
    if (q[0] == 0x0e) { /* Get_Attribute_Single */
        if (data_n) { a[2] = 0x15; return 4; }
        if (cls == 1) {
            size_t count = identity_attribute(attr, a + 4);
            if (!count) a[2] = 0x14;
            return 4 + count;
        }
        if (attr != 1) { a[2] = 0x14; return 4; }
        a[4] = EIP_LED_Get(); return 5;
    }
    /* Set_Attribute_Single, custom LED class 0x64, instance 1, attribute 1. */
    if (cls == 1) { a[2] = (attr >= 1 && attr <= 7) ? 0x0e : 0x14; return 4; }
    if (attr != 1) { a[2] = 0x14; return 4; }
    if (data_n != 1) { a[2] = data_n ? 0x15 : 0x13; return 4; }
    if (q[path_end] > 1) { a[2] = 0x09; return 4; }
    EIP_LED_Set(q[path_end]);
    return 4;
}
int EIP_Handle(const uint8_t *q, size_t n, uint8_t *a,
               size_t cap, uint32_t *session, const uint8_t ip[4]) {
    uint16_t cmd;
    size_t payload = 0;
    uint32_t status = 0;
    if (n < 24 || n > EIP_MAX_PACKET || cap < EIP_MAX_REPLY) return 0;
    if (24u + r16(q + 2) != n) return 0;
    cmd = r16(q);
    memset(a, 0, EIP_MAX_REPLY);
    memcpy(a, q, 24); /* Includes sender context, echoed unchanged. */
    w16(a + 2, 0); w32(a + 8, 0); w32(a + 20, 0);
    if (r32(q + 8) != 0 || r32(q + 20) != 0) {
        status = 0x03;
    } else if (cmd == 0x0065) { /* RegisterSession */
        if (n != 28) status = 0x65;
        else if (r16(q + 24) != 1 || r16(q + 26) != 0) status = 0x69;
        else {
            if (!*session) {
                ++next_session;
                if (!next_session) ++next_session;
                *session = next_session;
            }
            w32(a + 4, *session);
            w16(a + 24, 1); payload = 4;
        }
    } else if (cmd == 0x0066) { /* No response to UnregisterSession */
        if (*session && r32(q + 4) == *session && n == 24) {
            *session = 0; return -1;
        }
        return 0;
    } else if (cmd == 0x0063) { /* ListIdentity over TCP (no UDP discovery). */
        uint8_t *d = a + 30;
        size_t name_n = sizeof(product_name) - 1u;
        if (n != 24) status = 0x65;
        else {
            w16(a + 24, 1); w16(a + 26, 0x000c);
            w16(a + 28, (uint16_t)(34u + name_n));
            w16(d, 1); /* Encapsulation protocol version */
            d[2] = 0; d[3] = 2; /* AF_INET, network byte order */
            d[4] = 0xaf; d[5] = 0x12; /* TCP port 44818, network order */
            memcpy(d + 6, ip, 4);
            w16(d + 18, 0); w16(d + 20, 0); w16(d + 22, 1);
            d[24] = 1; d[25] = 0; w16(d + 26, 0);
            w32(d + 28, 0x12345678u);
            d[32] = (uint8_t)name_n;
            memcpy(d + 33, product_name, name_n);
            d[33 + name_n] = 0; /* Device state, unspecified */
            payload = 6u + 34u + name_n;
        }
    } else if (cmd == 0x006f) { /* SendRRData: unconnected explicit messaging */
        const uint8_t *cip = NULL;
        size_t cip_n = 0, pos = 32, i;
        unsigned null_seen = 0;
        if (!*session || r32(q + 4) != *session) status = 0x64;
        else if (n < 32 || r32(q + 24) != 0 || r16(q + 30) != 2) status = 0x03;
        else {
            for (i = 0; i < 2 && !status; ++i) {
                uint16_t type, bytes;
                if (n - pos < 4) { status = 0x03; break; }
                type = r16(q + pos); bytes = r16(q + pos + 2); pos += 4;
                if (bytes > n - pos) { status = 0x03; break; }
                if (type == 0 && bytes == 0 && !null_seen) null_seen = 1;
                else if (type == 0x00b2 && !cip) { cip = q + pos; cip_n = bytes; }
                else status = 0x03;
                pos += bytes;
            }
            if (!status && (!cip || !null_seen || pos != n)) status = 0x03;
            if (!status) {
                size_t answer_n = cip_reply(cip, cip_n, a + 40);
                w16(a + 30, 2); /* CPF item count; interface and timeout are zero */
                w16(a + 32, 0); w16(a + 34, 0);
                w16(a + 36, 0x00b2); w16(a + 38, (uint16_t)answer_n);
                payload = 16u + answer_n;
            }
        }
    } else {
        status = 0x01; /* Unsupported encapsulation command */
    }
    w16(a + 2, (uint16_t)payload); w32(a + 8, status);
    return (int)(24u + payload);
}
