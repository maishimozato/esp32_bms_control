#include <iostream>
#include <iomanip>
#include <string>
#include "../src/balancing/balancing.h"

void printSet(int n, const SetResult& s) {
    std::cout << "Set " << n
              << ": upper " << s.upperVoltage
              << " V, lower " << s.lowerVoltage << " V\n";

    std::cout << "  Direction: "
              << (s.referenceCurrent > 0 ? "UPPER TO LOWER" :
                  s.referenceCurrent < 0 ? "LOWER TO UPPER" :
                  "NONE")
              << " | Iref: "
              << s.referenceCurrent << " A\n";
}

int main() {
    Balancer balancer;
    std::string command;

    while (std::cin >> command) {

        if (command == "reset") {
            balancer = Balancer();
            continue;
        }

        if (command != "data") return 1;

        BatteryData b = {};

        if (!(std::cin >> b.soc
                       >> b.fault
                       >> b.valid
                       >> b.minTemp
                       >> b.maxTemp
                       >> b.current1
                       >> b.current2
                       >> b.daysSinceSession
                       >> b.level1)) {
            return 1;
        }

        for (auto& v : b.cellVoltages) {
            if (!(std::cin >> v)) return 1;
        }

        auto r = balancer.update(b);

        std::cout << std::fixed << std::setprecision(2);

        std::cout << "SOC: " << b.soc
                  << "% | Cell spread: "
                  << r.cellSpread << " V\n";

        std::cout << "Imbalance: "
                  << (r.imbalance ? "YES" : "NO") << "\n";

        printSet(1, r.set1);
        printSet(2, r.set2);

        std::cout << "Status: " << r.status
                  << "\nEND\n" << std::flush;
    }
}