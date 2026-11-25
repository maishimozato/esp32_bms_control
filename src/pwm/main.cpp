#include <Arduino.h>
#include <driver/mcpwm_prelude.h>
#include <esp_err.h>

constexpr int PWM_HIGH_GPIO = 13;
constexpr int PWM_LOW_GPIO = 14;
constexpr uint32_t PWM_FREQUENCY_HZ = 20000;
constexpr uint32_t PWM_TIMER_RESOLUTION_HZ = 10000000;
constexpr uint32_t PWM_PERIOD_TICKS =
    PWM_TIMER_RESOLUTION_HZ / PWM_FREQUENCY_HZ;

constexpr uint32_t PWM_DEAD_TIME_NS = 1000;
constexpr float PWM_DUTY_PERCENT = 50.0f;
constexpr bool PWM_OUTPUTS_VERIFIED = true;

mcpwm_cmpr_handle_t comparator = nullptr;
bool pwmRunning = false;
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
  error = mcpwm_new_comparator(oper, &comparatorConfig, &comparator);
  if (error != ESP_OK) {
    Serial.print("MCPWM comparator creation failed: ");
    Serial.println(esp_err_to_name(error));
    return;
  }

  // setting the duty cycle
  const uint32_t compareTicks =
      static_cast<uint32_t>(PWM_PERIOD_TICKS * PWM_DUTY_PERCENT / 100.0f);
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
      highGenerator, MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
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
      (static_cast<uint64_t>(PWM_DEAD_TIME_NS) * PWM_TIMER_RESOLUTION_HZ +
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

  pwmRunning = true;

  Serial.println("Complementary converter PWM started at 20 kHz");
}

bool setDemoDuty(float percent) {
  if (!pwmRunning || comparator == nullptr) {
    Serial.println("PWM is not running");
    return false;
  }

  if (percent < 20.0f || percent > 80.0f) {
    Serial.println("Demo duty must be between 20% and 80%");
    return false;
  }

  const uint32_t compareTicks =
      static_cast<uint32_t>(PWM_PERIOD_TICKS * percent / 100.0f);

  const esp_err_t error =
      mcpwm_comparator_set_compare_value(comparator, compareTicks);

  if (error != ESP_OK) {
    Serial.print("Duty update failed: ");
    Serial.println(esp_err_to_name(error));
    return false;
  }

  Serial.printf("Nominal high-side duty = %.1f%%\n", percent);
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  setupConverterPwm();
}

void loop() {
  if (Serial.available()) {
    const char command = Serial.read();

    switch (command) {
      case 'u':
      case 'U':
        setDemoDuty(25.0f);
        Serial.println("Target: approximately TOP 7.5 V / BOTTOM 2.5 V");
        break;

      case 'b':
      case 'B':
        setDemoDuty(50.0f);
        Serial.println("Target: approximately TOP 5 V / BOTTOM 5 V");
        break;

      case 'l':
      case 'L':
        setDemoDuty(75.0f);
        Serial.println("Target: approximately TOP 2.5 V / BOTTOM 7.5 V");
        break;

      case '?':
        Serial.println("u: upper capacitor higher");
        Serial.println("b: equal split");
        Serial.println("l: lower capacitor higher");
        Serial.println("No software STOP is implemented in this patch.");
        break;

      default:
        break;
    }
  }

  delay(1);
}
