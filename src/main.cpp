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
const uint16_t BAMS_PORT = 502;
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

  if (!modbusClient.begin(bamsIp, BAMS_PORT)) {
    Serial.println("Modbus TCP connection failed");
  }
  Serial.println("Connected to BAMS Modbus TCP server");
}

void loop() {
  if (!modbusClient.connected()) {
    Serial.println("Reconnecting to BAMS...");

    modbusClient.stop();
    delay(250);

    if (!modbusClient.begin(bamsIp, BAMS_PORT)) {
      Serial.print("Connection failed: ");
      Serial.println(modbusClient.lastError());
      delay(2000);
      return;
    }
  }

  if (modbusClient.requestFrom(UNIT_ID, 0x03, 0x0000, 1)) {
    uint16_t value = modbusClient.read();
    Serial.print("Register 0: 0x");
    Serial.println(value, HEX);
  } else {
    Serial.print("Modbus read failed: ");
    Serial.println(modbusClient.lastError());
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
}
