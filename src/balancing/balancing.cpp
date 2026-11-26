#include "balancing.h"
#include <cmath>

static float sumVoltage(const BatteryData& b, int start, int count) {
    unsigned sum = 0;

    for (int i = start; i < start + count; i++) {
        sum += b.cellVoltages[i];
    }

    return sum / 1000.0f;
}

BalanceResult Balancer::update(const BatteryData& b) {
    BalanceResult r = {};
    r.status = "WAITING";

    r.set1.upperVoltage = sumVoltage(b, 0, 48);
    r.set1.lowerVoltage = sumVoltage(b, 48, 48);

    r.set2.upperVoltage = sumVoltage(b, 96, 32);
    r.set2.lowerVoltage = sumVoltage(b, 128, 32);

    bool active = direction1 || direction2;

    bool invalid = !b.valid
        || !std::isfinite(b.soc)
        || b.soc < 0 || b.soc > 100
        || !std::isfinite(b.daysSinceSession)
        || b.daysSinceSession < 0
        || !std::isfinite(b.minTemp)
        || !std::isfinite(b.maxTemp)
        || !std::isfinite(b.current1)
        || !std::isfinite(b.current2)
        || b.minTemp > b.maxTemp;

    unsigned minCell = 65535;
    unsigned maxCell = 0;

    for (auto v : b.cellVoltages) {
        if (!v) invalid = true;
        if (v < minCell) minCell = v;
        if (v > maxCell) maxCell = v;
    }

    r.cellSpread = (maxCell - minCell) / 1000.0f;
    r.imbalance = maxCell - minCell > 740;

    const char* stop = nullptr;

    if (invalid) {
        stop = "INVALID DATA";
    }
    else if (b.fault) {
        stop = "FAULT";
    }
    else if (b.level1) {
        stop = "LEVEL 1 - TRANSFER STOPPED";
    }
    else if (b.minTemp < 15 || b.maxTemp > 30) {
        stop = "TEMPERATURE LIMIT";
    }
    else if (std::fabs(b.current1) >= 10 ||
             std::fabs(b.current2) >= 10) {
        stop = "CURRENT LIMIT";
    }
    else {
        for (int i = 0; i < 160; i += 16) {
            float v = sumVoltage(b, i, 16);

            if (v < 44.8f || v > 57.6f) {
                stop = "MODULE VOLTAGE LIMIT";
            }
        }
    }

    if (stop) {
        if (active) stopped = true;
        r.status = stop;
        return r;
    }

    if (stopped) {
        r.status = "SESSION STOPPED";
        return r;
    }

    if (!active) {
        if (b.daysSinceSession < 14) {
            r.status = "WAITING - SESSION INTERVAL";
            return r;
        }

        if (b.soc < 55) {
            r.status = "WAITING - LOW SOC";
            return r;
        }

        if (!r.imbalance) {
            r.status = "NO IMBALANCE FLAG";
            return r;
        }

        // Concentrate charge in the higher-voltage half for the demo.
        direction1 = r.set1.upperVoltage >= r.set1.lowerVoltage ? -1 : 1;
        direction2 = r.set2.upperVoltage >= r.set2.lowerVoltage ? -1 : 1;
    }

    r.set1.referenceCurrent = direction1 * 1.0f;
    r.set2.referenceCurrent = direction2 * 1.0f;

    r.set1.enabled = true;
    r.set2.enabled = true;

    r.status = "BALANCING (SIMULATED)";

    return r;
}