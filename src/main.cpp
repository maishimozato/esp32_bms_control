#include <ETH.h>
#include <ModbusTCPClient.h>
#include <NetworkClient.h>
#include <driver/mcpwm_prelude.h>
#include <esp_err.h>

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
// GPIO13/14 are candidates on the ESP32-POE-ISO; verify board wiring before use.
constexpr int PWM_HIGH_GPIO = 13;
constexpr int PWM_LOW_GPIO = 14;
constexpr uint32_t PWM_FREQUENCY_HZ = 20000;
constexpr uint32_t PWM_TIMER_RESOLUTION_HZ = 10000000;
constexpr uint32_t PWM_PERIOD_TICKS =
    PWM_TIMER_RESOLUTION_HZ / PWM_FREQUENCY_HZ;

// Keep these disabled until the driver input mapping and dead time are verified.
constexpr uint32_t PWM_DEAD_TIME_NS = 0;
constexpr float PWM_DUTY_PERCENT = 0.0f;
constexpr bool PWM_OUTPUTS_VERIFIED = false;

NetworkClient modbusNetworkClient;
ModbusTCPClient modbusClient(modbusNetworkClient);

void setupConverterPwm() {
  pinMode(PWM_HIGH_GPIO, OUTPUT);
  digitalWrite(PWM_HIGH_GPIO, LOW);
  pinMode(PWM_LOW_GPIO, OUTPUT);
  digitalWrite(PWM_LOW_GPIO, LOW);

  if (!PWM_OUTPUTS_VERIFIED || PWM_DEAD_TIME_NS == 0 ||
      PWM_DUTY_PERCENT <= 0.0f || PWM_DUTY_PERCENT >= 100.0f) {
    Serial.println(
        "Converter PWM disabled: verify outputs, dead time, and duty first");
    return;
  }

  mcpwm_timer_config_t timerConfig = {};
  timerConfig.group_id = 0;
  timerConfig.clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT;
  timerConfig.resolution_hz = PWM_TIMER_RESOLUTION_HZ;
  timerConfig.count_mode = MCPWM_TIMER_COUNT_MODE_UP;
  timerConfig.period_ticks = PWM_PERIOD_TICKS;

  mcpwm_timer_handle_t timer = nullptr;
  esp_err_t error = mcpwm_new_timer(&timerConfig, &timer);
  if (error != ESP_OK) {
    Serial.print("MCPWM timer creation failed: ");
    Serial.println(esp_err_to_name(error));
    return;
  }

  mcpwm_operator_config_t operatorConfig = {};
  operatorConfig.group_id = 0;
  mcpwm_oper_handle_t oper = nullptr;
  error = mcpwm_new_operator(&operatorConfig, &oper);
  if (error != ESP_OK) {
    Serial.print("MCPWM operator creation failed: ");
    Serial.println(esp_err_to_name(error));
    return;
  }

  error = mcpwm_operator_connect_timer(oper, timer);
  if (error != ESP_OK) {
    Serial.print("MCPWM timer connection failed: ");
    Serial.println(esp_err_to_name(error));
    return;
  }

  mcpwm_comparator_config_t comparatorConfig = {};
  comparatorConfig.flags.update_cmp_on_tez = true;
  mcpwm_cmpr_handle_t comparator = nullptr;
  error = mcpwm_new_comparator(oper, &comparatorConfig, &comparator);
  if (error != ESP_OK) {
    Serial.print("MCPWM comparator creation failed: ");
    Serial.println(esp_err_to_name(error));
    return;
  }

  const uint32_t compareTicks = static_cast<uint32_t>(
      PWM_PERIOD_TICKS * PWM_DUTY_PERCENT / 100.0f);
  error = mcpwm_comparator_set_compare_value(comparator, compareTicks);
  if (error != ESP_OK) {
    Serial.print("MCPWM duty setup failed: ");
    Serial.println(esp_err_to_name(error));
    return;
  }

  mcpwm_generator_config_t highGeneratorConfig = {};
  highGeneratorConfig.gen_gpio_num = PWM_HIGH_GPIO;
  mcpwm_gen_handle_t highGenerator = nullptr;
  error = mcpwm_new_generator(oper, &highGeneratorConfig, &highGenerator);
  if (error != ESP_OK) {
    Serial.print("MCPWM high-side generator creation failed: ");
    Serial.println(esp_err_to_name(error));
    return;
  }

  mcpwm_generator_config_t lowGeneratorConfig = {};
  lowGeneratorConfig.gen_gpio_num = PWM_LOW_GPIO;
  mcpwm_gen_handle_t lowGenerator = nullptr;
  error = mcpwm_new_generator(oper, &lowGeneratorConfig, &lowGenerator);
  if (error != ESP_OK) {
    Serial.print("MCPWM low-side generator creation failed: ");
    Serial.println(esp_err_to_name(error));
    return;
  }

  error = mcpwm_generator_set_action_on_timer_event(
      highGenerator,
      MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                  MCPWM_TIMER_EVENT_EMPTY,
                                  MCPWM_GEN_ACTION_HIGH));
  if (error == ESP_OK) {
    error = mcpwm_generator_set_action_on_compare_event(
        highGenerator,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator,
                                       MCPWM_GEN_ACTION_LOW));
  }
  if (error != ESP_OK) {
    Serial.print("MCPWM waveform setup failed: ");
    Serial.println(esp_err_to_name(error));
    return;
  }

  const uint32_t deadTimeTicks = static_cast<uint32_t>(
      (static_cast<uint64_t>(PWM_DEAD_TIME_NS) *
           PWM_TIMER_RESOLUTION_HZ +
       999999999ULL) /
      1000000000ULL);
  mcpwm_dead_time_config_t deadTimeConfig = {};
  deadTimeConfig.posedge_delay_ticks = deadTimeTicks;
  error = mcpwm_generator_set_dead_time(highGenerator, highGenerator,
                                        &deadTimeConfig);
  if (error != ESP_OK) {
    Serial.print("MCPWM high-side dead-time setup failed: ");
    Serial.println(esp_err_to_name(error));
    return;
  }

  deadTimeConfig.posedge_delay_ticks = 0;
  deadTimeConfig.negedge_delay_ticks = deadTimeTicks;
  deadTimeConfig.flags.invert_output = true;
  error = mcpwm_generator_set_dead_time(highGenerator, lowGenerator,
                                        &deadTimeConfig);
  if (error != ESP_OK) {
    Serial.print("MCPWM low-side dead-time setup failed: ");
    Serial.println(esp_err_to_name(error));
    return;
  }

  error = mcpwm_timer_enable(timer);
  if (error == ESP_OK) {
    error = mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP);
  }
  if (error != ESP_OK) {
    Serial.print("MCPWM timer start failed: ");
    Serial.println(esp_err_to_name(error));
    return;
  }

  Serial.println("Complementary converter PWM started at 20 kHz");
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("Starting ESP32-P0E-ISO BAMS Modbus TCP test");
  setupConverterPwm();

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
