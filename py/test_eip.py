"""Python 3 standard-library EtherNet/IP/CIP client for this STM32 test.
Usage on Windows: py test_eip.py 192.168.10.2
Add --fragment to deliberately split requests across several TCP writes.
"""
import argparse
import socket
import struct
import time

HEADER = struct.Struct('<HHII8sI')


def recv_exact(sock, count):
    data = bytearray()
    while len(data) < count:
        chunk = sock.recv(count - len(data))
        if not chunk:
            raise ConnectionError('Board closed the connection before the reply was complete')
        data.extend(chunk)
    return bytes(data)


class EIPClient:
    def __init__(self, ip, fragment=False):
        self.sock = socket.create_connection((ip, 44818), timeout=5)
        self.sock.settimeout(5)
        self.session = 0
        self.counter = 0
        self.fragment = fragment

    def exchange(self, command, payload=b''):
        self.counter += 1
        context = struct.pack('<Q', self.counter)
        packet = HEADER.pack(command, len(payload), self.session, 0, context, 0) + payload
        if self.fragment:
            for offset in range(0, len(packet), 7):
                self.sock.sendall(packet[offset:offset + 7])
                time.sleep(0.01)
        else:
            self.sock.sendall(packet)
        cmd, size, session, status, echoed, options = HEADER.unpack(recv_exact(self.sock, 24))
        if size > 4096:
            raise RuntimeError('Unexpectedly large encapsulation reply')
        result = recv_exact(self.sock, size)
        if cmd != command or echoed != context or options:
            raise RuntimeError('Invalid encapsulation reply header/context')
        if status:
            raise RuntimeError(f'EtherNet/IP encapsulation status: 0x{status:08X}')
        if command == 0x65:
            if result != b'\x01\x00\x00\x00' or not session:
                raise RuntimeError('Invalid RegisterSession response')
            self.session = session
        elif command == 0x6F and session != self.session:
            raise RuntimeError('Unexpected session handle')
        return result

    def register(self):
        self.exchange(0x65, struct.pack('<HH', 1, 0))

    def cip(self, service, cls, instance, attribute, data=b''):
        path = bytes((0x20, cls, 0x24, instance, 0x30, attribute))
        request = bytes((service, len(path) // 2)) + path + data
        # Interface handle, timeout, CPF count, null address item, data item.
        cpf = struct.pack('<IHHHHHH', 0, 0, 2, 0, 0, 0xB2, len(request)) + request
        response = self.exchange(0x6F, cpf)
        if len(response) < 8 or struct.unpack_from('<I', response)[0] != 0:
            raise RuntimeError('Invalid SendRRData reply')
        count = struct.unpack_from('<H', response, 6)[0]
        offset = 8
        cip = None
        for _ in range(count):
            if offset + 4 > len(response):
                raise RuntimeError('Truncated CPF item header')
            kind, size = struct.unpack_from('<HH', response, offset)
            offset += 4
            if offset + size > len(response):
                raise RuntimeError('Truncated CPF item')
            if kind == 0xB2:
                cip = response[offset:offset + size]
            offset += size
        if offset != len(response) or cip is None or len(cip) < 4:
            raise RuntimeError('Missing or invalid CIP reply')
        if cip[0] != service | 0x80:
            raise RuntimeError('Unexpected CIP reply service')
        if cip[2]:
            raise RuntimeError(f'CIP general status: 0x{cip[2]:02X}')
        start = 4 + 2 * cip[3]
        if start > len(cip):
            raise RuntimeError('Truncated CIP additional status')
        return cip[start:]

    def close(self):
        try:
            if self.session:
                self.sock.sendall(HEADER.pack(0x66, 0, self.session, 0, b'\0' * 8, 0))
        except OSError:
            pass
        finally:
            self.sock.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('ip', nargs='?', default='192.168.10.2')
    parser.add_argument('--fragment', action='store_true')
    args = parser.parse_args()
    client = EIPClient(args.ip, args.fragment)
    try:
        identity = client.exchange(0x63)
        if len(identity) < 40 or struct.unpack_from('<HH', identity) != (1, 0x0C):
            raise RuntimeError('Invalid ListIdentity reply')
        name_n = identity[38]
        if 40 + name_n != len(identity):
            raise RuntimeError('Invalid ListIdentity name length')
        print('ListIdentity:', identity[39:39 + name_n].decode('ascii'))
        client.register()
        print(f'RegisterSession: OK (0x{client.session:08X})')
        name = client.cip(0x0E, 1, 1, 7)
        if not name or len(name) != 1 + name[0]:
            raise RuntimeError('Invalid CIP SHORT_STRING')
        print('CIP product name:', name[1:].decode('ascii'))
        for on in (1, 0, 1, 0):
            client.cip(0x10, 0x64, 1, 1, bytes((on,)))
            observed = client.cip(0x0E, 0x64, 1, 1)
            if observed != bytes((on,)):
                raise RuntimeError(f'LED readback mismatch: {observed!r}')
            print('LED:', 'ON' if on else 'OFF', '(CIP readback confirmed)')
            time.sleep(1)
        print('PASS: identity, session, CIP reads/writes, and LED readback')
    finally:
        client.close()


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, ValueError, struct.error) as exc:
        raise SystemExit(f'FAIL: {exc}')
