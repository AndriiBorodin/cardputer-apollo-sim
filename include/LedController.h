#pragma once
#include <Arduino.h>
#include "ApolloTypes.h"
#include <cmath>

namespace Apollo {

class LedController {
private:
    static constexpr uint8_t LED_PIN = 21; // M5StampS3 / Cardputer built-in WS2812 RGB LED

    static void writeRgb(uint8_t r, uint8_t g, uint8_t b) {
#ifdef neopixelWrite
        neopixelWrite(LED_PIN, r, g, b);
#else
        neopixelWrite(LED_PIN, r, g, b);
#endif
    }

public:
    static void init() {
        pinMode(LED_PIN, OUTPUT);
        off();
    }

    static void off() {
        writeRgb(0, 0, 0);
    }

    static void setRgb(uint8_t r, uint8_t g, uint8_t b) {
        writeRgb(r, g, b);
    }

    static void update(const RocketState& r, const SimConfig& cfg, AppScreen screen, uint32_t nowMillis) {
        if (screen == AppScreen::MAIN_MENU) {
            off();
            return;
        }

        if (screen == AppScreen::MISSION_SUMMARY) {
            if (r.splashedDown) {
                // Bright Emerald / Cyan victory pulse
                uint8_t pulse = 60 + static_cast<uint8_t>(50 * (0.5 + 0.5 * std::sin(nowMillis * 0.005)));
                writeRgb(0, pulse, pulse / 2);
            } else {
                // Red catastrophe
                writeRgb(90, 0, 0);
            }
            return;
        }

        // Active Flight (DSKY or MAP):
        // 1. Crash Emergency (Master Alarm, 5 Hz strobe)
        if (r.crashed) {
            bool blink = (nowMillis % 200) < 100;
            writeRgb(blink ? 140 : 0, 0, 0);
            return;
        }

        if (r.splashedDown) {
            writeRgb(0, 80, 40);
            return;
        }

        // 2. Stage separation needed (fuel empty in stage 0 or 1, and not on pad)
        // -> Flashing Bright Orange / Amber (3 Hz): indicates PRESS [SPACE]
        if (r.fuelKg <= 1.0 && r.stage < 2 && !r.onPad) {
            bool blink = (nowMillis % 333) < 166;
            writeRgb(blink ? 140 : 0, blink ? 45 : 0, 0);
            return;
        }

        // 3. User Action Required in Houston or Semi-Auto Mode (Awaiting [ENTER] for GO / Ignition)
        bool awaitingAction = false;
        if (!r.isDemoFlight) {
            if (r.onPad) {
                awaitingAction = true;
            } else if (r.actionPrompt[0] == '[' && (r.actionPrompt[1] == 'E' || r.actionPrompt[1] == 'e')) {
                awaitingAction = true;
            } else if (r.phase == MissionPhase::LOW_EARTH_ORBIT && r.tliCountdown > 0.0 && r.tliCountdown <= 20.0) {
                awaitingAction = true;
            } else if (r.phase == MissionPhase::LUNAR_ORBIT && r.actionPrompt[0] == '[') {
                awaitingAction = true;
            }
        }

        if (awaitingAction) {
            // Warm Amber / Yellow breathing pulse (1.5 Hz): indicates PRESS [ENTER]
            double wave = 0.5 + 0.5 * std::sin(nowMillis * 0.007);
            uint8_t rVal = static_cast<uint8_t>(60 + 95 * wave);
            uint8_t gVal = static_cast<uint8_t>(40 + 70 * wave);
            writeRgb(rVal, gVal, 0);
            return;
        }

        // 4. Engine Burn active
        if (r.throttle > 0.01 && r.fuelKg > 0.0) {
            // Solid Green thrust indicator
            uint8_t bright = static_cast<uint8_t>(30 + 70 * r.throttle);
            writeRgb(0, bright, 0);
            return;
        }

        // 5. Atmospheric Reentry / Parachutes
        if (r.phase == MissionPhase::ATMOSPHERIC_REENTRY) {
            double altE = r.pos.magnitude() - cfg.earthRadius;
            if (altE < 7000.0) {
                // Sky Blue parachutes active
                writeRgb(0, 60, 90);
                return;
            } else {
                // Atmospheric aerobraking glow (Heat shield orange)
                writeRgb(90, 30, 0);
                return;
            }
        }

        // 6. Normal Coasting in Space (Demo or User Modes)
        // Soft Blue breathing pulse (peaceful orbital cruise)
        double wave = 0.5 + 0.5 * std::sin(nowMillis * 0.003);
        uint8_t bVal = static_cast<uint8_t>(20 + 40 * wave);
        writeRgb(0, bVal / 2, bVal);
    }
};

} // namespace Apollo
