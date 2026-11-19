#include <ETH.h>
#include <ModbusTCPClient.h>
#include <NetworkClient.h>

// Olimex ESP32-POE-ISO typical PHY pins
#define ETH_ADDR 0
#define ETH_MDC_PIN 23
#define ETH_MDIO_PIN 18
#define ETH_PHY_TYPE ETH_PHY_LAN8720
#define ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT
#define ETH_POWER_PIN 12

IPAddress espIp(192, 168, 1, 50);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

IPAddress bamsIp(192, 168, 1, 199);
const uint16_t BAMS_PORT = 503;
const uint8_t UNIT_ID = 6;

NetworkClient modbusNetworkClient;
ModbusTCPClient modbusClient(modbusNetworkClient);

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("Starting ESP32-P0E-ISO BAMS Modbus TCP test");

  pinMode(ETH_POWER_PIN, OUTPUT);
  digitalWrite(ETH_POWER_PIN, LOW);
  delay(200);

  if (!ETH.begin(ETH_PHY_TYPE, ETH_ADDR, ETH_MDC_PIN, ETH_MDIO_PIN,
                 ETH_POWER_PIN, ETH_CLK_MODE)) {
    Serial.println("ERROR: Ethernet initialization failed");
    while (true) {
      delay(1000);
    }
  }

  // Static IP; wait for link
  ETH.config(espIp, gateway, subnet);
  Serial.print("Waiting for Ethernet link");
  while (!ETH.linkUp()) {
    Serial.print(".");
    delay(100);
  }
  Serial.print("ESP32 IP: ");
  Serial.println(ETH.localIP());
}

void loop() {
  if (!ETH.linkUp()) {
    Serial.println("Ethernet link is down");
    delay(3000);
    return;
  }

  if (!modbusClient.connected()) {
    Serial.print("Connecting to ");
    Serial.print(bamsIp);
    Serial.print(":");
    Serial.println(BAMS_PORT);

    modbusClient.stop();
    modbusNetworkClient.stop();

    if (!modbusClient.begin(bamsIp, BAMS_PORT)) {
      Serial.print("TCP connection failed: ");
      Serial.println(modbusClient.lastError());

      modbusClient.stop();
      modbusNetworkClient.stop();
      delay(5000);
      return;
    }

    Serial.println("TCP connection established");
  }

  // Prefer the exact settings that worked in your other Modbus tool.
  const uint16_t registerAddress = 0x1044;

  Serial.print("Reading holding register 0x");
  Serial.print(registerAddress, HEX);
  Serial.print(", unit ID ");
  Serial.println(UNIT_ID);

  int count =
      modbusClient.requestFrom(UNIT_ID, HOLDING_REGISTERS, registerAddress, 1);

  if (count == 1 && modbusClient.available()) {
    int value = modbusClient.read();

    Serial.print("Value: ");
    Serial.print(value);
    Serial.print(" (0x");
    Serial.print(value, HEX);
    Serial.println(")");
  } else {
    Serial.print("Modbus read failed: ");
    Serial.println(modbusClient.lastError());

    modbusClient.stop();
    modbusNetworkClient.stop();
  }

  delay(3000);
}

// Read 10 holding registers: 0x1044 through 0x104D
//   if (modbusClient.requestFrom(UNIT_ID, 0x03, 0x1044, 10)) {
//     uint16_t chargeState = modbusClient.read();
//     int16_t currentRaw = modbusClient.read();
//     uint16_t soc = modbusClient.read();
//     uint16_t runState = modbusClient.read();
//     uint16_t voltageRaw = modbusClient.read();
//     uint16_t insulation = modbusClient.read();
//     uint16_t chargeEnergy = modbusClient.read();
//     uint16_t dischargeEnergy = modbusClient.read();
//     uint16_t reserved = modbusClient.read();
//     uint16_t chargeLimit = modbusClient.read();

//     float currentA = currentRaw * 0.1f;
//     float systemV = voltageRaw * 0.1f;

//     Serial.println("---------- BAMS STATUS ----------");
//     Serial.print("Charge state: ");
//     Serial.println(chargeState);
//     Serial.print("System current: ");
//     Serial.print(currentA, 1);
//     Serial.println(" A");
//     Serial.print("System SOC: ");
//     Serial.print(soc);
//     Serial.println(" %");
//     Serial.print("Run state: ");
//     Serial.println(runState);
//     Serial.print("System voltage: ");
//     Serial.print(systemV, 1);
//     Serial.println(" V");
//     Serial.println("---------------------------------");
//   } else {
//     Serial.print("Modbus read failed: ");
//     Serial.println(modbusClient.lastError());
//   }

//   delay(2000);
