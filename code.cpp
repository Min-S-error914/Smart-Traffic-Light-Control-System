// adaptive_traffic_light.cpp
// Simulates an adaptive traffic light controller for a 4-way intersection.
// Compile: g++ -std=c++17 adaptive_traffic_light.cpp -o adaptive_traffic_light

#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <iomanip>
#include <string>
#include <algorithm>

using namespace std;
using namespace std::chrono_literals;

enum class LightState { RED, GREEN, YELLOW };

struct TrafficLight {
    string name;
    LightState state = LightState::RED;
    int green_duration = 10; // seconds
    int yellow_duration = 3; // seconds
    int red_duration = 0;    // computed externally

    TrafficLight(const string& n) : name(n) {}
    static string state_to_string(LightState s) {
        switch (s) {
            case LightState::RED: return "RED";
            case LightState::GREEN: return "GREEN";
            case LightState::YELLOW: return "YELLOW";
        }
        return "UNKNOWN";
    }
};

class IntersectionController {
private:
    TrafficLight ns; // North-South
    TrafficLight ew; // East-West
    int min_green, max_green;
    int yellow_time;
    int all_red_time;
    int cycles;
    bool realtime_delay; // whether to sleep during simulation

public:
    IntersectionController(int minG=5, int maxG=40, int yellow=3, int allred=1, int cycles_=5, bool realtime=true)
        : ns("North-South"), ew("East-West"), min_green(minG), max_green(maxG),
          yellow_time(yellow), all_red_time(allred), cycles(cycles_), realtime_delay(realtime)
    {
        ns.yellow_duration = yellow_time;
        ew.yellow_duration = yellow_time;
    }

    void simulate_with_inputs(int ns_density, int ew_density) {
        cout << "\nSimulating " << cycles << " cycles with densities: NS=" << ns_density << "  EW=" << ew_density << "\n\n";
        for (int c = 1; c <= cycles; ++c) {
            cout << "=== Cycle " << c << " ===\n";
            compute_and_set_green(ns_density, ew_density);
            run_phase(ns, ew);
            // swap roles next cycle (we'll compute again but logically NS and EW alternate giving green)
            // For clarity we keep NS green first always; green durations recomputed each cycle.
            cout << "\n";
        }
    }

private:
    void compute_and_set_green(int ns_density, int ew_density) {
        // Avoid divide-by-zero: if both zero, give equal minimal green
        double nsd = ns_density;
        double ewd = ew_density;
        double total = nsd + ewd;
        if (total <= 0.0) {
            ns.green_duration = min_green;
            ew.green_duration = min_green;
            return;
        }
        // We allocate a green budget: base + proportional extra
        int base = min_green;
        int extra_budget = max_green - min_green; // maximum extra to distribute
        // compute proportional share
        ns.green_duration = base + static_cast<int>(round((nsd / total) * extra_budget));
        ew.green_duration = base + static_cast<int>(round((ewd / total) * extra_budget));

        // enforce bounds
        ns.green_duration = clamp(ns.green_duration, min_green, max_green);
        ew.green_duration = clamp(ew.green_duration, min_green, max_green);
        ns.yellow_duration = yellow_time;
        ew.yellow_duration = yellow_time;
    }

    void run_phase(TrafficLight& firstGreen, TrafficLight& second) {
        // First direction green -> yellow -> all-red -> second direction green ...
        set_and_print(firstGreen, LightState::GREEN);
        wait_and_print(firstGreen.green_duration);

        set_and_print(firstGreen, LightState::YELLOW);
        wait_and_print(firstGreen.yellow_duration);

        set_and_print(firstGreen, LightState::RED);
        // all-red gap
        wait_and_print(all_red_time);

        // Second direction green
        set_and_print(second, LightState::GREEN);
        wait_and_print(second.green_duration);

        set_and_print(second, LightState::YELLOW);
        wait_and_print(second.yellow_duration);

        set_and_print(second, LightState::RED);
        // all-red gap
        wait_and_print(all_red_time);
    }

    void set_and_print(TrafficLight& tl, LightState s) {
        tl.state = s;
        cout << "[" << setw(3) << current_time_str() << "] " << tl.name << " -> " << TrafficLight::state_to_string(s)
             << " (will last ";
        if (s == LightState::GREEN) cout << tl.green_duration;
        else if (s == LightState::YELLOW) cout << tl.yellow_duration;
        else cout << "until other gets green";
        cout << "s)\n";
    }

    void wait_and_print(int seconds) {
        if (seconds <= 0) return;
        // print a short progress (no heavy logs)
        if (realtime_delay) {
            std::this_thread::sleep_for(std::chrono::seconds(seconds));
        } else {
            // fast simulation: print simulated wait info
            cout << "   (simulated " << seconds << "s)\n";
        }
    }

    string current_time_str() {
        // return a simple HH:MM:SS using system_clock
        auto now = chrono::system_clock::now();
        time_t tnow = chrono::system_clock::to_time_t(now);
        tm local_tm;
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&local_tm, &tnow);
#else
        localtime_r(&tnow, &local_tm);
#endif
        char buf[9];
        strftime(buf, sizeof(buf), "%H:%M:%S", &local_tm);
        return string(buf);
    }
};

int main() {
    cout << "Adaptive Traffic Light Simulator\n";
    cout << "--------------------------------\n";
    int cycles;
    cout << "Enter number of cycles to simulate (e.g., 3): ";
    if (!(cin >> cycles)) return 0;

    int mode;
    cout << "Choose input mode: 1) Manual densities  2) Random densities\nEnter 1 or 2: ";
    cin >> mode;

    bool realtime = false;
    cout << "Run in real-time (sleep between phases)? 1=Yes 0=No (choose 0 for fast output): ";
    cin >> realtime;

    IntersectionController controller(5, 40, 3, 1, cycles, realtime);

    if (mode == 1) {
        int ns_d, ew_d;
        cout << "Enter NS (North-South) traffic density (non-negative integer): ";
        cin >> ns_d;
        cout << "Enter EW (East-West) traffic density (non-negative integer): ";
        cin >> ew_d;
        controller.simulate_with_inputs(ns_d, ew_d);
    } else {
        // random densities
        std::mt19937 rng((unsigned)chrono::system_clock::now().time_since_epoch().count());
        std::uniform_int_distribution<int> dist(0, 100);
        for (int c = 0; c < cycles; ++c) {
            int ns_d = dist(rng);
            int ew_d = dist(rng);
            cout << "\n--- Random densities for cycle " << (c+1) << " ---\n";
            controller.simulate_with_inputs(ns_d, ew_d);
        }
    }

    cout << "\nSimulation finished.\n";
    return 0;
}

