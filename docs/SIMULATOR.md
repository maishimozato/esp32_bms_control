The `simulator` folder has three files:

- **`bess_sim.py`** runs a pretend BAMS that responds to Modbus TCP requests with sample battery readings. It also has a `probe` mode to request those readings from your laptop, without an ESP32.
- **`register_map.json`** lists the register addresses and explains what each reading means, including its units and scale.
- **`test_sim.py`** checks that the simulator returns the expected values and handles requests correctly.

To start the simulator:
```sh
python3 simulator/bess_sim.py serve
```

In a second terminal, read its values:
```sh
python3 simulator/bess_sim.py probe
```

To run the tests:
```sh
python3 -m unittest discover -s simulator -v
```

The register map is based on Narada’s example and still needs to be checked against the lab system.