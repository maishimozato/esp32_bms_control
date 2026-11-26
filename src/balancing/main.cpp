#include <Arduino.h>
#include "balancing.h"

Balancer balancer;

void setup() {
    Serial.begin(115200);
    Serial.println("Balancing simulation only");
    Serial.println("u/l: start a demo, f: alarm, r: reset");
}

void loop() {
    if (!Serial.available()) return;

    char c = Serial.read();

    if (c == 'r') {
        balancer = Balancer();
        return;
    }

    if (c != 'u' && c != 'l' && c != 'f') return;

    BatteryData b = {};

    b.valid = true;
    b.soc = 60;
    b.daysSinceSession = 14;
    b.minTemp = 25;
    b.maxTemp = 25;

    for (int i = 0; i < 160; i++) {
        bool upper = i < 48 || (i >= 96 && i < 128);

        b.cellVoltages[i] =
            (upper == (c != 'l')) ? 3350 : 3250;
    }

    b.cellVoltages[c == 'l' ? 0 : 48] = 2800;
    b.cellVoltages[c == 'l' ? 48 : 0] = 3550;

    b.level1 = c == 'f';

    auto r = balancer.update(b);

    Serial.printf("%s\n", r.status);
    Serial.printf("Set 1 Iref: %.2f A\n",
                  r.set1.referenceCurrent);
    Serial.printf("Set 2 Iref: %.2f A\n",
                  r.set2.referenceCurrent);
}