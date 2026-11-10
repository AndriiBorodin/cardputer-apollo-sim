#include <Arduino.h>
#include <M5Unified.h>
#include <M5Cardputer.h>
#include "ApolloTypes.h"
#include "OrbitalPhysics.h"
#include "OrbitPredictor.h"
#include "DisplayRenderer.h"
#include "InputManager.h"
#include "AudioSys.h"
#include "LedController.h"

using namespace Apollo;

// Global Application State
SimConfig simConfig;
RocketState rocket;
TrajectoryForecast forecast;
DisplayRenderer renderer;

AppScreen currentScreen = AppScreen::MAIN_MENU;
int activeMenuTile = 0; // Default to Tile 0 (Missions)
bool menuInSub = false; // Level 1 (Tile Carousel) vs Level 2 (Sub-menu)
int menuSubRow = 0;     // Submenu selection index
int timeWarp = 1;
double missionTime = 0.0;
bool showAbortModal = false;

// Map View Camera Controls
double mapZoom = 1.0;
MapCameraFocus mapFocus = MapCameraFocus::SYSTEM_FIT;
Vector2D manualPanOffset(0, 0);

// Dual-Core Trajectory Worker Synchronization
TaskHandle_t trajectoryTaskHandle = nullptr;
SemaphoreHandle_t stateMutex = nullptr;
MissionPhase prevDemoPhase = MissionPhase::PRELAUNCH_PAD;
bool demoTliSlowed = false;
bool demoLoiSlowed = false;
bool demoTeiSlowed = false;
bool demoMoonApproachSlowed = false;
bool demoReentryApproachSlowed = false;
PhysicsScale savedUserPhysicsScale = PhysicsScale::ARCADE_FAST;

void startNewMission() {
    savedUserPhysicsScale = simConfig.physicsScale;
    if (simConfig.controlMode == ControlMode::AUTONOMOUS_DEMO) {
        simConfig.physicsScale = PhysicsScale::ARCADE_FAST;
    }
    simConfig.applyScale();
    rocket = RocketState{};
    rocket.scenario = simConfig.scenario;
    rocket.isDemoFlight = (simConfig.controlMode == ControlMode::AUTONOMOUS_DEMO);
    OrbitalPhysics::initScenario(rocket, simConfig, simConfig.scenario);
    rocket.isDemoFlight = (simConfig.controlMode == ControlMode::AUTONOMOUS_DEMO);
    rocket.demoNoticeTimer = 0;
    prevDemoPhase = MissionPhase::PRELAUNCH_PAD;
    demoTliSlowed = false;
    demoLoiSlowed = false;
    demoTeiSlowed = false;
    demoMoonApproachSlowed = false;
    demoReentryApproachSlowed = false;
    missionTime = 0.0;
    timeWarp = 1;
    mapZoom = 1.0;
    mapFocus = MapCameraFocus::SYSTEM_FIT;
    manualPanOffset = Vector2D(0, 0);
    showAbortModal = false;
    currentScreen = AppScreen::DSKY_COMPUTER;
}

void returnToMainMenu() {
    currentScreen = AppScreen::MAIN_MENU;
    if (simConfig.physicsScale != savedUserPhysicsScale) {
        simConfig.physicsScale = savedUserPhysicsScale;
        simConfig.applyScale();
    }
}

// Background Task on ESP32-S3 Core 0: Continuous Ballistic Orbit Prediction
void TrajectoryWorkerTask(void* parameter) {
    TrajectoryForecast localForecast;
    for (;;) {
        if (currentScreen == AppScreen::DSKY_COMPUTER || currentScreen == AppScreen::ORBIT_MAP) {
            RocketState rCopy;
            SimConfig cfgCopy;
            double tCopy;

            if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(15)) == pdTRUE) {
                rCopy = rocket;
                cfgCopy = simConfig;
                tCopy = missionTime;
                xSemaphoreGive(stateMutex);

                OrbitPredictor::computeTrajectory(rCopy, cfgCopy, tCopy, localForecast);

                if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(15)) == pdTRUE) {
                    forecast = localForecast;
                    xSemaphoreGive(stateMutex);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(35)); // ~28 Hz forecast updates
    }
}

void setup() {
    auto cfg = M5.config();
    cfg.internal_imu = true;
    M5Cardputer.begin(cfg, true);

    AudioSys::init();
    LedController::init();

    simConfig.scenario = MissionScenario::AUTONOMOUS_DEMO;
    simConfig.physicsScale = PhysicsScale::ARCADE_FAST;
    simConfig.controlMode = ControlMode::FLIGHT_DIRECTOR;
    simConfig.applyScale();
    OrbitalPhysics::initScenario(rocket, simConfig, MissionScenario::AUTONOMOUS_DEMO);
    rocket.isDemoFlight = true;
    renderer.init();

    // Authentic Apollo 11 AGC DSKY Boot & Self-Test Animation (with instant key-skip)
    renderer.playBootSequence(simConfig.soundEnabled);

    stateMutex = xSemaphoreCreateMutex();

    xTaskCreatePinnedToCore(
        TrajectoryWorkerTask,
        "TrajectoryWorker",
        8192,
        nullptr,
        1,
        &trajectoryTaskHandle,
        0 // Core 0
    );
}

void loop() {
    static unsigned long lastMillis = millis();
    unsigned long now = millis();
    double dtReal = (now - lastMillis) / 1000.0;
    lastMillis = now;
    if (dtReal > 0.08) dtReal = 0.08;

    LedController::update(rocket, simConfig, currentScreen, now);

    if (currentScreen == AppScreen::MAIN_MENU) {
        bool launchRequested = false;
        InputManager::handleMainMenu(simConfig, activeMenuTile, menuInSub, menuSubRow, launchRequested, renderer);
        if (launchRequested) {
            if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                startNewMission();
                xSemaphoreGive(stateMutex);
            }
        } else {
            renderer.renderMainMenu(simConfig, activeMenuTile, menuInSub, menuSubRow);
        }
        delay(16);
        return;
    }

    if (currentScreen == AppScreen::MISSION_SUMMARY) {
        InputManager::handleSummaryInput(currentScreen, renderer);
        if (currentScreen == AppScreen::MAIN_MENU) {
            returnToMainMenu();
            return;
        }
        renderer.renderMissionSummary(rocket, missionTime, simConfig);
        delay(16);
        return;
    }

    // Autonomous Time Warp Director for Demo Flight & Houston Mode (event-driven transitions so user [1-5] overrides persist)
    if (rocket.isDemoFlight || simConfig.controlMode == ControlMode::FLIGHT_DIRECTOR) {
        bool phaseChanged = (rocket.phase != prevDemoPhase);
        prevDemoPhase = rocket.phase;

        if (phaseChanged) {
            bool isRealistic = (simConfig.physicsScale == PhysicsScale::REALISTIC);
            if (rocket.phase == MissionPhase::PRELAUNCH_PAD ||
                rocket.phase == MissionPhase::LAUNCH_ASCENT) {
                timeWarp = 1;
                if (rocket.isDemoFlight) {
                    mapFocus = MapCameraFocus::CENTER_EARTH;
                    mapZoom = 3.5;
                }
            } else if (rocket.phase == MissionPhase::LOW_EARTH_ORBIT) {
                timeWarp = isRealistic ? 10 : 5;
                demoTliSlowed = false;
                if (rocket.isDemoFlight) {
                    mapFocus = MapCameraFocus::CENTER_EARTH;
                    mapZoom = 3.0;
                }
            } else if (rocket.phase == MissionPhase::TRANSLUNAR_COAST) {
                timeWarp = isRealistic ? 2000 : 100;
                demoMoonApproachSlowed = false;
                if (rocket.isDemoFlight) {
                    mapFocus = MapCameraFocus::SYSTEM_FIT;
                    mapZoom = 1.0;
                }
            } else if (rocket.phase == MissionPhase::LUNAR_FLYBY_SOI) {
                timeWarp = isRealistic ? 100 : 25;
                demoLoiSlowed = false;
                if (rocket.isDemoFlight) {
                    mapFocus = MapCameraFocus::CENTER_MOON;
                    mapZoom = 4.0;
                }
            } else if (rocket.phase == MissionPhase::LUNAR_ORBIT) {
                timeWarp = isRealistic ? 50 : 10;
                demoTeiSlowed = false;
                if (rocket.isDemoFlight) {
                    mapFocus = MapCameraFocus::CENTER_MOON;
                    mapZoom = 4.0;
                }
            } else if (rocket.phase == MissionPhase::TRANSEARTH_COAST) {
                timeWarp = isRealistic ? 2000 : 100;
                demoReentryApproachSlowed = false;
                if (rocket.isDemoFlight) {
                    mapFocus = MapCameraFocus::SYSTEM_FIT;
                    mapZoom = 1.0;
                }
            } else if (rocket.phase == MissionPhase::ATMOSPHERIC_REENTRY) {
                timeWarp = 1;
                if (rocket.isDemoFlight) {
                    mapFocus = MapCameraFocus::CENTER_EARTH;
                    mapZoom = 3.5;
                }
            }
        } else {
            if (rocket.phase == MissionPhase::LOW_EARTH_ORBIT) {
                if (!demoTliSlowed && rocket.tliCountdown > 0.0 && rocket.tliCountdown <= 8.0) {
                    demoTliSlowed = true;
                    timeWarp = 1;
                }
            } else if (rocket.phase == MissionPhase::TRANSLUNAR_COAST && !rocket.autoBurnActive) {
                Vector2D moonPos = OrbitalPhysics::getMoonPos(simConfig, missionTime);
                double distM = (rocket.pos - moonPos).magnitude();
                if (!demoMoonApproachSlowed && distM <= simConfig.moonSOI * 1.35) {
                    demoMoonApproachSlowed = true;
                    int targetWarp = (simConfig.physicsScale == PhysicsScale::REALISTIC) ? 100 : 25;
                    if (timeWarp > targetWarp) timeWarp = targetWarp;
                }
            } else if (rocket.phase == MissionPhase::LUNAR_FLYBY_SOI) {
                Vector2D moonPos = OrbitalPhysics::getMoonPos(simConfig, missionTime);
                Vector2D moonVel = OrbitalPhysics::getMoonVel(simConfig, missionTime);
                Vector2D relPosM = rocket.pos - moonPos;
                Vector2D relVelM = rocket.vel - moonVel;
                double dM = relPosM.magnitude();
                double vRadM = (dM > 10.0) ? relVelM.dot(relPosM * (1.0 / dM)) : 0.0;
                if (!demoLoiSlowed && (vRadM >= -80.0 || rocket.activeProg == AgcProgram::P21_LOI_BURN)) {
                    demoLoiSlowed = true;
                    timeWarp = 1;
                }
            } else if (rocket.phase == MissionPhase::LUNAR_ORBIT) {
                if (!demoTeiSlowed && (rocket.teiWindowOpen || rocket.activeProg == AgcProgram::P30_TEI_BURN)) {
                    demoTeiSlowed = true;
                    timeWarp = 1;
                }
            } else if (rocket.phase == MissionPhase::TRANSEARTH_COAST) {
                double altE = rocket.pos.magnitude() - simConfig.earthRadius;
                if (!demoReentryApproachSlowed && altE <= simConfig.atmosphereHeight * 3.5) {
                    demoReentryApproachSlowed = true;
                    timeWarp = 1; // Drop to 1x real-time for atmospheric approach & re-entry
                }
            }
        }
    }

    // Universal Safety: Automatic stepped warp reduction when approaching Earth atmosphere across all modes
    if (rocket.phase == MissionPhase::TRANSEARTH_COAST) {
        double altE = rocket.pos.magnitude() - simConfig.earthRadius;
        if (altE <= 5000000.0 && timeWarp > 100) {
            timeWarp = 100;
        }
        if (altE <= 1500000.0 && timeWarp > 25) {
            timeWarp = 25;
        }
        if (altE <= 500000.0 && timeWarp > 5) {
            timeWarp = 5;
        }
        if (altE <= 250000.0 && timeWarp > 1) {
            timeWarp = 1;
            demoReentryApproachSlowed = true;
        }
    } else if (rocket.phase == MissionPhase::ATMOSPHERIC_REENTRY) {
        timeWarp = 1;
    }

    // --- Active Flight Simulation (DSKY Computer or Orbit Map) ---
    InputManager::handleFlightInput(
        rocket, simConfig, currentScreen, timeWarp, mapZoom, mapFocus, manualPanOffset, showAbortModal, renderer
    );

    if (currentScreen == AppScreen::MAIN_MENU) {
        returnToMainMenu();
        return;
    }

    // Only progress physics if NOT paused in Abort Modal
    if (!showAbortModal) {
        double dtSim = dtReal * timeWarp;
        double maxSubDt = (rocket.throttle > 0.01 ||
                           rocket.autoBurnActive ||
                           rocket.phase == MissionPhase::LAUNCH_ASCENT ||
                           rocket.phase == MissionPhase::LOW_EARTH_ORBIT ||
                           rocket.phase == MissionPhase::ATMOSPHERIC_REENTRY) ? 0.15 : 2.5;
        int minSteps = static_cast<int>(std::ceil(dtSim / maxSubDt));
        int subSteps = std::min(120, std::max(1, minSteps));
        double subDt = dtSim / subSteps;

        if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            for (int i = 0; i < subSteps; ++i) {
                OrbitalPhysics::stepRK4(rocket, simConfig, missionTime, subDt);
                missionTime += subDt;
            }
            xSemaphoreGive(stateMutex);
        }
    }

    // Check if mission ended (Splashdown or Crash)
    if (rocket.splashedDown || rocket.crashed) {
        currentScreen = AppScreen::MISSION_SUMMARY;
        return;
    }

    // Copy latest forecast safely (static to avoid stack thrash & guarantee valid state)
    static TrajectoryForecast trajCopy;
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        trajCopy = forecast;
        xSemaphoreGive(stateMutex);
    }

    if (currentScreen == AppScreen::DSKY_COMPUTER) {
        renderer.renderDskyComputer(rocket, simConfig, trajCopy, missionTime, timeWarp, showAbortModal);
    } else if (currentScreen == AppScreen::ORBIT_MAP) {
        renderer.renderOrbitMap(rocket, simConfig, trajCopy, missionTime, timeWarp, mapZoom, mapFocus, manualPanOffset, showAbortModal);
    }

    delay(15); // ~60 FPS
}
