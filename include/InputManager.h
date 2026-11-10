#pragma once
#include "ApolloTypes.h"
#include "OrbitalPhysics.h"
#include "DisplayRenderer.h"
#include <M5Cardputer.h>
#include <algorithm>

namespace Apollo {

class InputManager {
public:
    static void handleMainMenu(SimConfig& cfg,
                               int& activeTile,
                               bool& inSubMenu,
                               int& subMenuIndex,
                               bool& launchRequested,
                               DisplayRenderer& renderer) {
        M5Cardputer.update();

        // Hardware button G0
        if (M5Cardputer.BtnA.wasPressed()) {
            if (!inSubMenu) {
                inSubMenu = true;
                subMenuIndex = 0;
            } else {
                if (activeTile == 0) {
                    cfg.scenario = static_cast<MissionScenario>(subMenuIndex);
                    launchRequested = true;
                    return;
                }
            }
        }

        if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
            return;
        }

        auto status = M5Cardputer.Keyboard.keysState();

        // BACKSPACE / DEL goes back to Tiles Carousel Level 1!
        if (status.del) {
            if (inSubMenu) {
                inSubMenu = false;
                return;
            }
        }

        // Swipe Tiles with TAB (only on Carousel)
        if (status.tab) {
            if (!inSubMenu) {
                activeTile = (activeTile + 1) % 4;
            }
            return;
        }

        if (status.enter || status.space) {
            if (!inSubMenu) {
                // Dive in!
                inSubMenu = true;
                subMenuIndex = 0;
            } else {
                // Inside Sub-menu:
                if (activeTile == 0) {
                    // Start selected flight mode!
                    if (subMenuIndex == 0) {
                        cfg.controlMode = ControlMode::FLIGHT_DIRECTOR;
                        cfg.scenario = MissionScenario::FULL_MISSION;
                    } else if (subMenuIndex == 1) {
                        cfg.controlMode = ControlMode::SMART_ASSIST;
                        cfg.scenario = MissionScenario::FULL_MISSION;
                    } else {
                        cfg.controlMode = ControlMode::AUTONOMOUS_DEMO;
                        cfg.scenario = MissionScenario::AUTONOMOUS_DEMO;
                    }
                    launchRequested = true;
                    return;
                } else if (activeTile == 1) {
                    // Select Physics Scale
                    cfg.physicsScale = (subMenuIndex == 0) ? PhysicsScale::ARCADE_FAST : PhysicsScale::REALISTIC;
                    inSubMenu = false;
                } else if (activeTile == 2) {
                    // Toggle Audio
                    cfg.soundEnabled = (subMenuIndex == 0);
                    inSubMenu = false;
                } else if (activeTile == 3) {
                    // Theme select
                    cfg.themePreset = static_cast<ThemePreset>(subMenuIndex);
                    inSubMenu = false;
                }
            }
            return;
        }

        for (char c : status.word) {
            switch (c) {
                // Swipe Left / Right: A/D or , / /
                case 'a':
                case 'A':
                case ',':
                case '<':
                    if (!inSubMenu) {
                        activeTile = (activeTile + 3) % 4; // Previous tile
                    }
                    break;

                case 'd':
                case 'D':
                case '/':
                case '?':
                    if (!inSubMenu) {
                        activeTile = (activeTile + 1) % 4; // Next tile
                    }
                    break;

                // Move Subrow: W/S or ; / .
                case 'w':
                case 'W':
                case ';':
                case ':':
                    if (inSubMenu) {
                        int count = (activeTile == 0) ? 3 : (activeTile == 1 ? 2 : (activeTile == 2 ? 2 : 4));
                        subMenuIndex = (subMenuIndex + count - 1) % count;
                    }
                    break;

                case 's':
                case 'S':
                case '.':
                case '>':
                    if (inSubMenu) {
                        int count = (activeTile == 0) ? 3 : (activeTile == 1 ? 2 : (activeTile == 2 ? 2 : 4));
                        subMenuIndex = (subMenuIndex + 1) % count;
                    }
                    break;

                // Direct jump
                case '1':
                    if (!inSubMenu) activeTile = 0;
                    else subMenuIndex = 0;
                    break;
                case '2':
                    if (!inSubMenu) activeTile = 1;
                    else subMenuIndex = 1;
                    break;
                case '3':
                    if (!inSubMenu) activeTile = 2;
                    else subMenuIndex = (activeTile == 0 || activeTile == 3) ? 2 : 0;
                    break;
                case '4':
                    if (!inSubMenu) activeTile = 3;
                    else subMenuIndex = (activeTile == 3) ? 3 : 0;
                    break;


            }
        }
    }

    static void handleFlightInput(RocketState& r,
                                  const SimConfig& cfg,
                                  AppScreen& screen,
                                  int& timeWarp,
                                  double& mapZoom,
                                  MapCameraFocus& camFocus,
                                  Vector2D& manualPanOffset,
                                  bool& showAbortModal,
                                  DisplayRenderer& renderer) {
        M5Cardputer.update();

        // If Abort Confirmation Modal is active, intercept inputs!
        if (showAbortModal) {
            handleAbortModal(screen, showAbortModal);
            return;
        }

        // Top hardware button G0 toggles between DSKY Computer and Orbit Map
        if (M5Cardputer.BtnA.wasPressed()) {
            screen = (screen == AppScreen::DSKY_COMPUTER) ? AppScreen::ORBIT_MAP : AppScreen::DSKY_COMPUTER;
        }

        if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
            return;
        }

        auto status = M5Cardputer.Keyboard.keysState();

        // BACKSPACE / DEL triggers the confirmation modal
        if (status.del) {
            showAbortModal = true;
            return;
        }

        // TAB toggles between DSKY and MAP
        if (status.tab) {
            screen = (screen == AppScreen::DSKY_COMPUTER) ? AppScreen::ORBIT_MAP : AppScreen::DSKY_COMPUTER;
            return;
        }

        bool allowedKey = false;

        // [ENTER] -> Confirm GO / Burn in Houston & Semi-Auto modes
        if (status.enter) {
            allowedKey = true;
            if (!r.isDemoFlight) {
                if (r.onPad) {
                    if (cfg.controlMode == ControlMode::SMART_ASSIST && r.selectedProg != AgcProgram::P11_EARTH_ORB) {
                        AudioSys::playMasterAlarm(cfg.soundEnabled);
                        snprintf(r.actionPrompt, sizeof(r.actionPrompt), "SELECT P11 FIRST (N)");
                    } else {
                        OrbitalPhysics::liftoffFromPad(r, cfg);
                        r.throttle = 1.0;
                        r.autoBurnActive = true;
                        AudioSys::playQuindarTone(cfg.soundEnabled);
                    }
                } else if (r.phase == MissionPhase::LAUNCH_ASCENT) {
                    if (cfg.controlMode != ControlMode::SMART_ASSIST && r.stage < 2 && r.fuelKg <= 1.0) {
                        OrbitalPhysics::triggerStage(r, cfg);
                    } else if (r.throttle == 0.0 && r.fuelKg > 1.0) {
                        r.throttle = 1.0;
                    }
                } else if (cfg.controlMode == ControlMode::SMART_ASSIST) {
                    if (r.phase == MissionPhase::LUNAR_ORBIT) {
                        if (r.selectedProg == AgcProgram::P30_TEI_BURN) {
                            if (r.teiWindowOpen) {
                                OrbitalPhysics::executeAgcProgram(r, cfg, AgcProgram::P30_TEI_BURN);
                            } else {
                                snprintf(r.actionPrompt, sizeof(r.actionPrompt), "WAIT FOR TEI WINDOW");
                            }
                        } else if (r.selectedProg == AgcProgram::P21_LOI_BURN) {
                            r.selectedProg = AgcProgram::P30_TEI_BURN;
                            snprintf(r.actionPrompt, sizeof(r.actionPrompt), "P30 SEL: [ENTER] TEI");
                        } else {
                            OrbitalPhysics::executeAgcProgram(r, cfg, r.selectedProg);
                        }
                    } else if (r.phase == MissionPhase::TRANSEARTH_COAST) {
                        if (r.selectedProg == AgcProgram::P63_REENTRY) {
                            OrbitalPhysics::executeAgcProgram(r, cfg, AgcProgram::P63_REENTRY);
                        } else {
                            r.selectedProg = AgcProgram::P63_REENTRY;
                            snprintf(r.actionPrompt, sizeof(r.actionPrompt), "P63 SEL: [ENTER] REENTRY");
                        }
                    } else {
                        OrbitalPhysics::executeAgcProgram(r, cfg, r.selectedProg);
                    }
                } else if (r.phase == MissionPhase::LOW_EARTH_ORBIT) {
                    OrbitalPhysics::executeAgcProgram(r, cfg, AgcProgram::P15_TLI_BURN);
                } else if (r.phase == MissionPhase::LUNAR_FLYBY_SOI) {
                    OrbitalPhysics::executeAgcProgram(r, cfg, AgcProgram::P21_LOI_BURN);
                } else if (r.phase == MissionPhase::LUNAR_ORBIT) {
                    if (r.teiWindowOpen) {
                        OrbitalPhysics::executeAgcProgram(r, cfg, AgcProgram::P30_TEI_BURN);
                    }
                } else if (r.phase == MissionPhase::TRANSEARTH_COAST) {
                    OrbitalPhysics::executeAgcProgram(r, cfg, AgcProgram::P63_REENTRY);
                }
            }
        }

        // [SPACE] -> Manual Staging / Separation
        if (status.space) {
            allowedKey = true;
            if (!r.isDemoFlight && r.stage < 3 && !r.onPad) {
                OrbitalPhysics::triggerStage(r, cfg);
                AudioSys::playStagingClick(cfg.soundEnabled);
            }
        }

        for (char c : status.word) {
            switch (c) {
                // Next AGC Program in Semi-Auto mode
                case 'n':
                case 'N':
                    if (cfg.controlMode == ControlMode::SMART_ASSIST && !r.isDemoFlight) {
                        OrbitalPhysics::cycleNextProgram(r);
                        AudioSys::playQuindarTone(cfg.soundEnabled);
                    }
                    allowedKey = true;
                    break;


                // View toggle
                case 'm':
                case 'M':
                    screen = (screen == AppScreen::DSKY_COMPUTER) ? AppScreen::ORBIT_MAP : AppScreen::DSKY_COMPUTER;
                    allowedKey = true;
                    break;

                // Time Warp Levels [1-5]
                case '1': timeWarp = 1; allowedKey = true; break;
                case '2': timeWarp = (cfg.physicsScale == PhysicsScale::REALISTIC) ? 10 : 5; allowedKey = true; break;
                case '3': timeWarp = (cfg.physicsScale == PhysicsScale::REALISTIC) ? 100 : 25; allowedKey = true; break;
                case '4': timeWarp = (cfg.physicsScale == PhysicsScale::REALISTIC) ? 1000 : 100; allowedKey = true; break;
                case '5': timeWarp = (cfg.physicsScale == PhysicsScale::REALISTIC) ? 5000 : 300; allowedKey = true; break;

                // Map Zoom In / Out
                case '=':
                case '+':
                case 'i':
                case 'I':
                    mapZoom = std::min(35.0, mapZoom * 1.45);
                    allowedKey = true;
                    break;
                case '-':
                case '_':
                case 'o':
                case 'O':
                    mapZoom = std::max(0.4, mapZoom / 1.45);
                    allowedKey = true;
                    break;

                // Cycle Map Camera Focus (SYS -> SHIP -> EARTH -> MOON)
                case 'c':
                case 'C': {
                    int nextF = (static_cast<int>(camFocus) + 1) % 4;
                    camFocus = static_cast<MapCameraFocus>(nextF);
                    manualPanOffset = Vector2D(0, 0);
                    allowedKey = true;
                    break;
                }

                // Map Panning
                case ';':
                case ':':
                    if (screen == AppScreen::ORBIT_MAP)
                        manualPanOffset.y += (cfg.moonDistance * 0.08) / mapZoom;
                    allowedKey = true;
                    break;
                case '.':
                case '>':
                    if (screen == AppScreen::ORBIT_MAP)
                        manualPanOffset.y -= (cfg.moonDistance * 0.08) / mapZoom;
                    allowedKey = true;
                    break;
                case ',':
                case '<':
                    if (screen == AppScreen::ORBIT_MAP)
                        manualPanOffset.x -= (cfg.moonDistance * 0.08) / mapZoom;
                    allowedKey = true;
                    break;
                case '/':
                case '?':
                    if (screen == AppScreen::ORBIT_MAP)
                        manualPanOffset.x += (cfg.moonDistance * 0.08) / mapZoom;
                    allowedKey = true;
                    break;


            }
        }

        // If user pressed any unauthorized key during Demo Flight:
        // Trigger 3-second Demo Notice Popup!
        if (r.isDemoFlight && !allowedKey) {
            r.demoNoticeTimer = millis() + 3000;
        }
    }

    static void handleAbortModal(AppScreen& screen, bool& showAbortModal) {
        if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
            return;
        }
        auto status = M5Cardputer.Keyboard.keysState();

        // [ENTER] -> Confirm abort and return to menu
        if (status.enter) {
            showAbortModal = false;
            screen = AppScreen::MAIN_MENU;
            return;
        }

        // [DEL] or [SPACE] or ESC -> Cancel and continue flight
        if (status.del || status.space) {
            showAbortModal = false;
            return;
        }

        for (char c : status.word) {
            if (c == 'y' || c == 'Y') {
                showAbortModal = false;
                screen = AppScreen::MAIN_MENU;
                return;
            }
            if (c == 'n' || c == 'N' || c == 'c' || c == 'C') {
                showAbortModal = false;
                return;
            }
        }
    }

    static void handleSummaryInput(AppScreen& screen, DisplayRenderer& renderer) {
        M5Cardputer.update();
        if (M5Cardputer.BtnA.wasPressed()) {
            screen = AppScreen::MAIN_MENU;
            return;
        }
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            auto status = M5Cardputer.Keyboard.keysState();
            if (status.enter || status.space || status.del) {
                screen = AppScreen::MAIN_MENU;
                return;
            }
            for (char c : status.word) {
                if (c == 'm' || c == 'M' || c == 'r' || c == 'R') {
                    screen = AppScreen::MAIN_MENU;
                    return;
                }

            }
        }
    }

private:
    static void toggleSettingsOption(SimConfig& cfg, int row) {
        if (row == 0) {
            int nextMode = (static_cast<int>(cfg.controlMode) + 1) % 3;
            cfg.controlMode = static_cast<ControlMode>(nextMode);
        } else if (row == 1) {
            cfg.physicsScale = (cfg.physicsScale == PhysicsScale::ARCADE_FAST)
                ? PhysicsScale::REALISTIC
                : PhysicsScale::ARCADE_FAST;
        } else if (row == 2) {
            cfg.soundEnabled = !cfg.soundEnabled;
        }
    }

    static void cycleTheme(SimConfig& cfg, int dir) {
        int nextTheme = (static_cast<int>(cfg.themePreset) + dir + 4) % 4;
        cfg.themePreset = static_cast<ThemePreset>(nextTheme);
    }

    static void cycleNextProgram(RocketState& r) {
        switch (r.selectedProg) {
            case AgcProgram::P11_EARTH_ORB:  r.selectedProg = AgcProgram::P15_TLI_BURN;   break;
            case AgcProgram::P15_TLI_BURN:   r.selectedProg = AgcProgram::P20_COAST_MOON; break;
            case AgcProgram::P20_COAST_MOON: r.selectedProg = AgcProgram::P21_LOI_BURN;   break;
            case AgcProgram::P21_LOI_BURN:   r.selectedProg = AgcProgram::P30_TEI_BURN;   break;
            case AgcProgram::P30_TEI_BURN:   r.selectedProg = AgcProgram::P63_REENTRY;    break;
            case AgcProgram::P63_REENTRY:    r.selectedProg = AgcProgram::P11_EARTH_ORB;  break;
            default:                         r.selectedProg = AgcProgram::P11_EARTH_ORB;  break;
        }
    }
};

} // namespace Apollo
