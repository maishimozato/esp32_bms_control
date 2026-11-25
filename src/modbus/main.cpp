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

  // Read 12 registers: 0x1044 through 0x104F.
  const uint16_t startAddress = 0x1044;
  const int registerCount = 12;

  int count = modbusClient.requestFrom(UNIT_ID, HOLDING_REGISTERS, startAddress,
                                       registerCount);

  if (count != registerCount || modbusClient.available() < registerCount) {
    Serial.print("Modbus block read failed: ");
    Serial.println(modbusClient.lastError());
    modbusClient.stop();
    modbusNetworkClient.stop();
    delay(3000);
    return;
  }

  uint16_t registers[registerCount];

  for (int i = 0; i < registerCount; i++) {
    registers[i] = static_cast<uint16_t>(modbusClient.read());
  }

  // Decode using the manual's example register map.
  uint16_t chargeState = registers[0];  // 0x1044
  int32_t currentRaw = registers[1];    // 0x1045
  // registers[2] is reserved                // 0x1046
  uint16_t soc = registers[3];              // 0x1047
  uint16_t runState = registers[4];         // 0x1048
  uint16_t voltageRaw = registers[5];       // 0x1049
  uint16_t insulationRaw = registers[6];    // 0x104A
  uint16_t chargeEnergy = registers[7];     // 0x104B
  uint16_t dischargeEnergy = registers[8];  // 0x104C
  // registers[9] is reserved                // 0x104D
  uint16_t chargeLimitRaw = registers[10];     // 0x104E
  uint16_t dischargeLimitRaw = registers[11];  // 0x104F

  // Convert current from signed 16-bit two's complement.
  if (currentRaw >= 32768) {
    currentRaw -= 65536;
  }

  Serial.println();
  Serial.println("---------- BAMS STATUS ----------");

  Serial.print("Charge state: ");
  Serial.println(chargeState);

  Serial.print("System current: ");
  Serial.print(currentRaw * 0.1f, 1);
  Serial.println(" A");

  Serial.print("System SOC raw (unvalidated): ");
  Serial.println(soc);

  Serial.print("Run state: ");
  Serial.println(runState);

  Serial.print("System voltage: ");
  Serial.print(voltageRaw * 0.1f, 1);
  Serial.println(" V");

  Serial.print("Insulation resistance (raw): ");
  Serial.println(insulationRaw);

  Serial.print("Charge energy: ");
  Serial.print(chargeEnergy);
  Serial.println(" kWh");

  Serial.print("Discharge energy: ");
  Serial.print(dischargeEnergy);
  Serial.println(" kWh");

  Serial.print("Charge current limit: ");
  Serial.print(chargeLimitRaw * 0.1f, 1);
  Serial.println(" A");

  Serial.print("Discharge current limit: ");
  Serial.print(dischargeLimitRaw * 0.1f, 1);
  Serial.println(" A");

  Serial.println("Raw registers:");
  for (int i = 0; i < registerCount; i++) {
    Serial.printf("  0x%04X = %u (0x%04X)\n",
                  static_cast<unsigned int>(startAddress + i),
                  static_cast<unsigned int>(registers[i]),
                  static_cast<unsigned int>(registers[i]));
  }

  Serial.println("---------------------------------");
  delay(3000);
}
