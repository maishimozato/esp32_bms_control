import subprocess
import threading
import time
import unittest
from pathlib import Path
from bess_sim import Server, Handler
from balancing_client import get_battery_data

PROGRAM = Path(__file__).with_name('balancing_test')

class BalancingTests(unittest.TestCase):

    def test_modbus_and_session(self):

        with Server(('127.0.0.1', 0), Handler) as server:

            server.unit = 1
            server.channels = 160
            server.started = time.monotonic()

            worker = threading.Thread(
                target=server.serve_forever, daemon=True
            )

            worker.start()

            try:
                def data(scenario, days=14, current=0):

                    server.scenario = scenario

                    soc, fault, cells, lo, hi, alarm = \
                        get_battery_data(server.server_address[1])

                    values = [
                        soc, int(fault), 1, lo, hi,
                        current, 0, days, int(alarm)
                    ] + list(cells)

                    return 'data ' + ' '.join(map(str, values)) + '\n'

                cases = [
                    ('upper-high', 'BALANCING (SIMULATED)', '-1.00', '-1.00'),
                    ('lower-high', 'BALANCING (SIMULATED)', '1.00', '1.00'),
                    ('mixed', 'BALANCING (SIMULATED)', '-1.00', '1.00'),
                    ('balanced', 'NO IMBALANCE FLAG', '0.00', '0.00'),
                    ('soc-below', 'WAITING - LOW SOC', '0.00', '0.00'),
                    ('soc-at', 'BALANCING (SIMULATED)', '-1.00', '-1.00'),
                    ('temperature-fault', 'TEMPERATURE LIMIT', '0.00', '0.00'),
                    ('level2', 'FAULT', '0.00', '0.00')
                ]

                for scenario, status, first, second in cases:

                    out = subprocess.check_output(
                        [str(PROGRAM)],
                        input=data(scenario),
                        text=True
                    )

                    self.assertIn('Status: ' + status, out, scenario)

                    currents = [
                        line.split('Iref: ')[1].split()[0]
                        for line in out.splitlines()
                        if 'Iref:' in line
                    ]

                    self.assertEqual(currents, [first, second])

                out = subprocess.check_output(
                    [str(PROGRAM)],
                    input=data('upper-high')
                        + data('level1')
                        + data('upper-high'),
                    text=True
                )

                self.assertIn('LEVEL 1 - TRANSFER STOPPED', out)
                self.assertIn('SESSION STOPPED', out)

                for days, current, status in [
                    (13, 0, 'WAITING - SESSION INTERVAL'),
                    (14, 10, 'CURRENT LIMIT')
                ]:

                    out = subprocess.check_output(
                        [str(PROGRAM)],
                        input=data('upper-high', days, current),
                        text=True
                    )

                    self.assertIn(status, out)

            finally:
                server.shutdown()
                worker.join()


if __name__ == '__main__':
    unittest.main()