#pragma once
#include "ApolloTypes.h"
#include <M5Unified.h>

namespace Apollo {

class AudioSys {
public:
    static void init() {
        // Speaker initialized by M5Cardputer.begin()
    }

    static void playQuindarTone(bool soundEnabled = true) {
        if (!soundEnabled) return;
        M5.Speaker.tone(2524, 120); // 2524 Hz authentic Apollo Quindar tone
    }

    static void playStagingClick(bool soundEnabled = true) {
        if (!soundEnabled) return;
        M5.Speaker.tone(550, 70);
    }

    static void playMasterAlarm(bool soundEnabled = true) {
        if (!soundEnabled) return;
        M5.Speaker.tone(1600, 200);
    }

    static void updateEngineHum(double throttle, bool soundEnabled = true) {
        (void)throttle;
        (void)soundEnabled;
    }
};

} // namespace Apollo
