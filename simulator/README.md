# Balancing Demo

This demo tests the balancing algorithm using the BESS simulator
over Modbus TCP. No ESP32 or physical batteries are needed.

## Running the demo

Run all commands from the root of the repo.

Compile the C++ code:

```sh
c++ -std=c++11 simulator/test_balancing.cpp src/balancing/balancing.cpp -o simulator/balancing_test
```

Terminal 1:

```sh
python3 simulator/bess_sim.py serve --scenario session-demo
```

Terminal 2:

```sh
python3 simulator/balancing_client.py
```

Start the client within 12 seconds of starting the simulator.

Both converters request current until a simulated Level 1
alarm occurs. Both current references then go to zero and
the session stops.

Restart both programs to repeat the demo.

Use Ctrl+C to stop either program.

## Files

- `bess_sim.py`: Simulates battery measurements and sends
  them over Modbus TCP.
- `register_map.json`: Contains register addresses and units.
- `balancing_client.py`: Reads battery measurements from the
  simulator and sends them to the C++ program.
- `test_balancing.cpp`: Runs the balancing algorithm locally
  and prints the results.
- `balancing.cpp`: Contains the balancing algorithm.
- `balancing.h`: Defines the inputs, outputs and session state.
- `src/balancing/main.cpp`: Tests the balancing algorithm
  on the ESP32 using simulated measurements.
- `test_sim.py`: Tests the BESS simulator.
- `test_balancing_logic.py`: Tests the balancing decisions.

Data flow:

BESS simulator -> Modbus TCP -> Python client ->
C++ balancing algorithm -> two reference currents

## Battery structure

The simulator assumes 10 modules in series with 16 voltage
measurement channels per module (160 total).

Each physical module contains 32 cells arranged as two
parallel strings of 16 cells.

| Group | Modules | Array indices |
|---|---|---|
| Set 1 upper | 1-3 | 0-47 |
| Set 1 lower | 4-6 | 48-95 |
| Set 2 upper | 7-8 | 96-127 |
| Set 2 lower | 9-10 | 128-159 |

Set 1 consists of 3+3 modules.
Set 2 consists of 2+2 modules.

Each set has its own converter, reference current and
eventually its own PI current controller.

The actual Modbus channel ordering still needs to be verified.

## Balancing logic

Before starting balancing:

- At least 14 days since the previous session
- SOC >= 55%
- Max-min cell voltage difference > 0.74 V
- Temperature between 15 and 30 C
- Module voltages between 44.8 and 57.6 V
- Current magnitude below 10 A
- No alarms, faults or invalid measurements

For this demo, the higher-voltage group is selected as the
receiving group in each set. The direction is maintained
throughout the session.

Positive current means upper to lower.
Negative current means lower to upper.

The reference current is currently fixed at 1 A for testing.
The local current measurements and elapsed days are simulated.

Both converters stop when a Level 1 alarm or a fault occurs
during balancing.

## Other scenarios

Stop the simulator and client before changing scenarios.

```sh
python3 simulator/bess_sim.py serve --scenario mixed
```

| Scenario | Description |
|---|---|
| upper-high | Both upper groups receive charge |
| lower-high | Both lower groups receive charge |
| mixed | Opposite transfer directions |
| balanced | No imbalance |
| soc-below | SOC below 55% |
| soc-at | SOC at 55% |
| temperature-fault | Temperature above limit |
| level1 | Level 1 alarm |
| session-demo | Balancing followed by Level 1 alarm |

The 14-day requirement can also be tested using:

```sh
python3 simulator/balancing_client.py --days 13
```

Stop the server while balancing to test communication loss.

## Running tests

Compile the C++ code first, then run:

```sh
python3 -m unittest discover -s simulator -v
```

## Next steps

- MATLAB/Simulink PI current controller
- Current ramping and charge redistribution
- Confirm source and sink selection logic
- Integrate both PWM pairs
- Verify actual BAMS register mapping
- Implement persistent session timing
- Hardware testing

This is a preliminary simulation, not a complete physical
battery model. Do not use it to drive the actual converters.