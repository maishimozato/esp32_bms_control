import argparse
import socket
import subprocess
import time
from pathlib import Path
from bess_sim import read_registers

PROGRAM = Path(__file__).resolve().parent / 'balancing_test'


def signed(raw):
    return raw - 65536 if raw >= 32768 else raw


def get_battery_data(port):
    with socket.create_connection(('127.0.0.1', port), timeout=3) as sock:
        soc = read_registers(sock, 0x2103, 1, transaction=1)[0]

        cells = read_registers(sock, 0x2800, 80, transaction=2)
        cells += read_registers(sock, 0x2850, 80, transaction=3)

        alarms = read_registers(sock, 0x21A2, 4, transaction=4)
        temps = read_registers(sock, 0x210A, 3, transaction=5)

        fault = any(alarms[1:])
        level1 = bool(alarms[0])

        tmin = signed(temps[2]) / 10
        tmax = signed(temps[0]) / 10

        return soc, fault, cells, tmin, tmax, level1


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=int, default=1502)
    parser.add_argument('--days', type=float, default=14)
    parser.add_argument('--once', action='store_true')
    args = parser.parse_args()

    print('BMS balancing client - simulation only', flush=True)

    with subprocess.Popen([str(PROGRAM)],
                          stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE,
                          text=True) as proc:

        try:
            while True:
                try:
                    soc, fault, cells, tmin, tmax, level1 = \
                        get_battery_data(args.port)

                    valid = True

                except (OSError, ValueError, EOFError):
                    print('Modbus read failed', flush=True)

                    soc = 0
                    fault = False
                    cells = [0] * 160
                    tmin = tmax = 0
                    level1 = False
                    valid = False

                # Current measurements and elapsed days are simulated.
                values = [
                    soc, int(fault), int(valid),
                    tmin, tmax, 0, 0,
                    args.days, int(level1)
                ] + list(cells)

                proc.stdin.write('data ' + ' '.join(map(str, values)) + '\n')
                proc.stdin.flush()

                print('----------------------------', flush=True)

                while True:
                    line = proc.stdout.readline()

                    if not line:
                        raise RuntimeError('C++ program exited')

                    if line.strip() == 'END':
                        break

                    print(line, end='', flush=True)

                if args.once:
                    break

                time.sleep(3)

        except KeyboardInterrupt:
            print('\nClient stopped')

        finally:
            proc.stdin.close()


if __name__ == '__main__':
    main()