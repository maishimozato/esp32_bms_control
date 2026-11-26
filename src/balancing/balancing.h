#ifndef BALANCING_H
#define BALANCING_H

#include <stdint.h>

struct BatteryData {
    uint16_t cellVoltages[160];
    float soc;
    bool fault;
    bool valid;
    float minTemp, maxTemp;
    float current1, current2;
    float daysSinceSession;
    bool level1;
};

struct SetResult {
    float upperVoltage, lowerVoltage;
    float referenceCurrent;
    bool enabled;
};

struct BalanceResult {
    SetResult set1, set2;
    float cellSpread;
    bool imbalance;
    const char* status;
};

// One simulated session. A stop stays latched until reset.
class Balancer {
    int direction1 = 0, direction2 = 0;
    bool stopped = false;

public:
    BalanceResult update(const BatteryData& battery);
};

#endif