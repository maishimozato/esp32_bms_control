#include <ETH.h>
#include <NetworkClient.h>
#include <ModbusTCPClient.h>

// Olimex ESP32-POE-ISO typical PHY pins
#define ETH_ADDR        0
#define ETH_MDC_PIN     23
#define ETH_MDIO_PIN    18
#define ETH_PHY_TYPE    ETH_PHY_LAN8720
#define ETH_CLK_MODE    ETH_CLOCK_GPIO17_OUT
#define ETH_POWER_PIN   12

IPAddress ip(192, 168, 1, 50);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

IPAddress bamsIp(192, 168, 1, 199);
const uint16_t BAMS_PORT = 502;
const uint8_t UNIT_ID = 1;

NetworkClient modbusNetworkClient;
ModbusTCPClient modbusClient(modbusNetworkClient);

void setup() {
  Serial.begin(115200);

  ETH.begin(ETH_PHY_TYPE, ETH_ADDR, ETH_MDC_PIN, ETH_MDIO_PIN,
            ETH_POWER_PIN, ETH_CLK_MODE);

  // Static IP; wait for link
  ETH.config(ip, gateway, subnet);
  while (!ETH.linkUp()) delay(100);

  Serial.print("ESP32 IP: ");
  Serial.println(ETH.localIP());

  if (!modbusClient.begin(bamsIp, BAMS_PORT)) {
    Serial.println("Modbus TCP connection failed");
  }
}

void loop() {
  if (!modbusClient.connected()) {
    modbusClient.begin(bamsIp, BAMS_PORT);
  }

  // Read 10 holding registers starting at 0x1044 (4164 decimal)
  if (modbusClient.requestFrom(UNIT_ID, HOLDING_REGISTERS, 0x1044, 10)) {
    while (modbusClient.available()) {
      uint16_t value = modbusClient.read();
      Serial.println(value, HEX);
    }
  } else {
    Serial.print("Modbus error: ");
    Serial.println(modbusClient.lastError());
  }

  delay(1000);
}