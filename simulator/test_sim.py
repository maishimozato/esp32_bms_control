import socket
import struct
import threading
import time
import unittest
from bess_sim import Handler, Server, read_registers, receive, reply, snapshot

class Fixtures(unittest.TestCase):

    def test_vendor_meanings_and_scaling(self):
        data = snapshot('charging')

        self.assertEqual(data[0x1044], 2)
        self.assertEqual(data[0x1047], 60)
        self.assertEqual(data[0x2103], 60)
        self.assertEqual(data[0x1045] - 65536, -25)
        self.assertEqual(data[0x1049], 5280)

        self.assertEqual(
            len([a for a in data if 0x2800 <= a <= 0x28FF]),
            160
        )

    def test_scenarios(self):

        self.assertEqual(
            [snapshot(s)[0x2103]
             for s in ['soc-below', 'soc-at', 'soc-above']],
            [54, 55, 56]
        )

        for i, s in enumerate(['level1', 'level2', 'level3']):
            data = snapshot(s)

            self.assertEqual(
                [data[a] for a in [0x21A2, 0x21A3, 0x21A4]],
                [int(j == i) for j in range(3)]
            )

        self.assertEqual(snapshot('temperature-fault')[0x210A], 310)

        self.assertEqual(
            snapshot('demo-cycle', elapsed=30)[0x21A2], 1
        )

        self.assertEqual(
            snapshot('demo-cycle', elapsed=40)[0x21A2], 0
        )

    def test_read_only_and_sparse_errors(self):
        data = snapshot('normal')

        self.assertEqual(
            reply(struct.pack('>BHH', 6, 0x2080, 3600), data),
            b'\x86\x01'
        )

        self.assertEqual(
            reply(struct.pack('>BHH', 3, 0x2080, 1), data),
            b'\x83\x02'
        )

        for count in [0, 126]:
            self.assertEqual(
                reply(struct.pack('>BHH', 3, 0x2800, count), data),
                b'\x83\x03'
            )

        self.assertEqual(reply(b'\x03', data), b'\x83\x03')


class Network(unittest.TestCase):

    def test_reads_fragmentation_and_exceptions(self):

        with Server(('127.0.0.1', 0), Handler) as server:

            server.unit = 1
            server.scenario = 'charging'
            server.channels = 160
            server.started = time.monotonic()

            worker = threading.Thread(
                target=server.serve_forever, daemon=True
            )

            worker.start()

            try:
                with socket.create_connection(
                    server.server_address, timeout=2
                ) as sock:

                    values = read_registers(sock, 0x1044, 10)

                    self.assertEqual(values[0], 2)
                    self.assertEqual(values[3], 60)

                    self.assertEqual(
                        len(read_registers(
                            sock, 0x2800, 125, transaction=2
                        )), 125
                    )

                    self.assertEqual(
                        len(read_registers(
                            sock, 0x287D, 35, transaction=3
                        )), 35
                    )

                    frame = struct.pack(
                        '>HHHBBHH', 77, 0, 6, 1, 3, 0x2103, 1
                    )

                    for byte in frame:
                        sock.sendall(bytes([byte]))

                    self.assertEqual(
                        struct.unpack('>HHHB', receive(sock, 7)),
                        (77, 0, 5, 1)
                    )

                    self.assertEqual(
                        receive(sock, 4), b'\x03\x02\x00\x3c'
                    )

                    with self.assertRaisesRegex(ValueError, '8302'):
                        read_registers(
                            sock, 0x2080, 1, transaction=4
                        )

            finally:
                server.shutdown()
                worker.join()


if __name__ == '__main__':
    unittest.main()