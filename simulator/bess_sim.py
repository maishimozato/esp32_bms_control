import argparse
import json
from pathlib import Path
import socket
import socketserver
import struct
import time

MAP = json.loads(Path(__file__).with_name('register_map.json').read_text())

SCENARIOS = (
    'normal', 'soc-below', 'soc-at', 'soc-above', 'charging',
    'temperature-fault', 'level1', 'level2', 'level3',
    'communication-fault', 'demo-cycle', 'upper-high',
    'lower-high', 'balanced', 'small-imbalance', 'mixed',
    'session-demo'
)


def receive(sock, count):
    data = b''

    while len(data) < count:
        chunk = sock.recv(count - len(data))

        if not chunk:
            raise EOFError('Connection closed')

        data += chunk

    return data


def snapshot(scenario, channels=160, elapsed=0):

    if scenario == 'demo-cycle':
        scenario = (
            'normal', 'soc-below', 'soc-at',
            'level1', 'normal'
        )[int(elapsed // 10) % 5]

    if scenario == 'session-demo':
        scenario = (
            'upper-high', 'upper-high',
            'level1', 'upper-high'
        )[min(int(elapsed // 6), 3)]

    soc = {
        'soc-below': 54,
        'soc-at': 55,
        'soc-above': 56
    }.get(scenario, 60)

    current = -25 if scenario == 'charging' else 0

    cells = [
        3310 if i < channels // 2 else 3290
        for i in range(channels)
    ]

    if scenario in (
        'upper-high', 'lower-high', 'mixed',
        'soc-at', 'soc-below', 'soc-above',
        'temperature-fault'
    ) and channels == 160:

        for i in range(channels):

            # Set 1: 3+3 modules
            # Set 2: 2+2 modules
            upper = i < 48 or 96 <= i < 128

            high = not upper if scenario == 'lower-high' else upper

            if scenario == 'mixed':
                high = upper if i < 96 else not upper

            cells[i] = 3350 if high else 3250

        if scenario == 'lower-high':
            cells[0], cells[48] = 2800, 3550
        else:
            cells[0], cells[48] = 3550, 2800

    elif scenario == 'balanced':
        cells = [3300] * channels

    elif scenario == 'small-imbalance':
        cells = [
            3302 if i < channels // 2 else 3300
            for i in range(channels)
        ]

    temps = [250] * channels

    if scenario == 'level1':
        cells[0] = 3700

    if scenario == 'temperature-fault':
        temps[0] = 310

    voltage = round(sum(cells) / 100)

    data = {
        0x1044: 2 if current < 0 else 0,
        0x1045: current & 65535,
        0x1046: 0,
        0x1047: soc,
        0x1048: 0,
        0x1049: voltage,
        0x104A: 1000,
        0x104B: 0,
        0x104C: 0,
        0x104D: 0,

        0x2100: voltage,
        0x2101: current & 65535,
        0x2102: 2 if current < 0 else 0,
        0x2103: soc,
        0x2104: 1000,
        0x2105: cells.index(max(cells)) + 1,
        0x2106: max(cells),
        0x2107: cells.index(min(cells)) + 1,
        0x2108: min(cells),
        0x2109: temps.index(max(temps)) + 1,
        0x210A: max(temps),
        0x210B: temps.index(min(temps)) + 1,
        0x210C: min(temps),

        0x21A2: int(scenario == 'level1'),
        0x21A3: int(scenario == 'level2'),
        0x21A4: int(scenario == 'level3'),
        0x21A5: int(scenario == 'communication-fault')
    }

    data.update({
        0x2800 + i: value
        for i, value in enumerate(cells)
    })

    data.update({
        0x2C00 + i: value
        for i, value in enumerate(temps)
    })

    return data


def reply(pdu, registers):
    fc = pdu[0]

    if fc != 3:
        return bytes([fc | 128, 1])

    if len(pdu) != 5:
        return bytes([131, 3])

    start, count = struct.unpack('>HH', pdu[1:])

    if not 1 <= count <= 125:
        return bytes([131, 3])

    if any(address not in registers
           for address in range(start, start + count)):
        return bytes([131, 2])

    return bytes([3, count * 2]) + struct.pack(
        '>' + 'H' * count,
        *(registers[a] for a in range(start, start + count))
    )


class Handler(socketserver.BaseRequestHandler):

    def handle(self):
        self.request.settimeout(10)

        try:
            while True:
                tid, protocol, length, unit = struct.unpack(
                    '>HHHB', receive(self.request, 7)
                )

                if protocol != 0 or not 2 <= length <= 254:
                    return

                pdu = receive(self.request, length - 1)

                if unit != self.server.unit:
                    continue

                data = snapshot(
                    self.server.scenario,
                    self.server.channels,
                    time.monotonic() - self.server.started
                )

                response = reply(pdu, data)

                self.request.sendall(
                    struct.pack(
                        '>HHHB', tid, 0,
                        len(response) + 1, unit
                    ) + response
                )

        except (EOFError, OSError):
            return


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def read_registers(sock, start, count, unit=1, transaction=1):

    pdu = struct.pack('>BHH', 3, start, count)

    sock.sendall(
        struct.pack(
            '>HHHB', transaction, 0, len(pdu) + 1, unit
        ) + pdu
    )

    tid, protocol, length, received_unit = struct.unpack(
        '>HHHB', receive(sock, 7)
    )

    if (tid, protocol, received_unit) != (transaction, 0, unit):
        raise ValueError('Invalid response header')

    if not 2 <= length <= 254:
        raise ValueError('Invalid response length')

    response = receive(sock, length - 1)

    if response[0] & 128:
        raise ValueError(f'Modbus exception {response.hex()}')

    if len(response) != 2 + count * 2:
        raise ValueError('Invalid FC03 response')

    if response[:2] != bytes([3, count * 2]):
        raise ValueError('Invalid FC03 response')

    return struct.unpack('>' + 'H' * count, response[2:])


def probe(host, port, unit):

    with socket.create_connection((host, port), timeout=3) as sock:

        for number, field in enumerate(MAP['registers'], 1):

            raw = read_registers(
                sock, int(field['address'], 16),
                1, unit, number
            )[0]

            value = raw - 65536 if field.get('signed') \
                and raw >= 32768 else raw

            print(
                f"{field['address']} {field['name']}: "
                f"{value * field['scale']:g} {field['unit']}"
            )


def main():

    parser = argparse.ArgumentParser()

    parser.add_argument('mode', choices=['serve', 'probe'])
    parser.add_argument('--host', default='127.0.0.1')
    parser.add_argument('--port', type=int, default=1502)
    parser.add_argument('--unit', type=int, default=1)
    parser.add_argument('--channels', type=int, default=160)
    parser.add_argument('--scenario', choices=SCENARIOS,
                        default='normal')

    args = parser.parse_args()

    if not 0 <= args.unit <= 255 or not 1 <= args.channels <= 256:
        parser.error('unit must be 0..255 and channels 1..256')

    if args.mode == 'probe':
        probe(args.host, args.port, args.unit)

    else:
        with Server((args.host, args.port), Handler) as server:

            server.unit = args.unit
            server.scenario = args.scenario
            server.channels = args.channels
            server.started = time.monotonic()

            print(
                f'EXAMPLE MAP simulator {args.host}:{args.port}, '
                f'unit={args.unit}, {args.scenario}, '
                f'channels={args.channels}',
                flush=True
            )

            try:
                server.serve_forever()

            except KeyboardInterrupt:
                pass


if __name__ == '__main__':
    main()