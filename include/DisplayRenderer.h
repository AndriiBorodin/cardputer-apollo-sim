#pragma once
#include "ApolloTypes.h"
#include "OrbitPredictor.h"
#include <M5Cardputer.h>
#include <M5Unified.h>
#include <algorithm>
#include <cmath>

namespace Apollo {

class DisplayRenderer {
private:
    M5Canvas canvas;
    static constexpr int W = 240;
    static constexpr int H = 135;

public:
    DisplayRenderer() : canvas(&M5Cardputer.Display) {}

    M5Canvas& getCanvas() {
        return canvas;
    }


    void init() {
        M5Cardputer.Display.setRotation(0);
        if (M5Cardputer.Display.width() < M5Cardputer.Display.height()) {
            M5Cardputer.Display.setRotation(1);
        }

        canvas.setColorDepth(16);
        canvas.createSprite(W, H);
        canvas.setTextWrap(false);
    }

    // Centered text and button helpers
    void drawCenteredText(int x, int y, int w, const char* str, uint16_t color, uint8_t size = 1) {
        int strW = strlen(str) * 6 * size;
        int cx = x + (w - strW) / 2;
        canvas.setTextSize(size);
        canvas.setTextColor(color);
        canvas.setCursor(cx, y);
        canvas.print(str);
    }

    void drawCenteredButton(int x, int y, int w, int h, const char* label, bool active, const ColorTheme& theme) {
        uint16_t bg = active ? theme.primary : canvas.color565(14, 22, 32);
        uint16_t border = active ? TFT_WHITE : theme.border;
        uint16_t fg = active ? TFT_BLACK : theme.primary;
        canvas.fillRoundRect(x, y, w, h, 3, bg);
        canvas.drawRoundRect(x, y, w, h, 3, border);
        drawCenteredText(x, y + (h - 8) / 2, w, label, fg, 1);
    }

    void drawTileIcon(int tileIdx, int cx, int cy, const ColorTheme& theme) {
        if (tileIdx == 0) {
            // ROCKET ICON (Flight Modes)
            canvas.fillRoundRect(cx - 4, cy - 9, 8, 16, 2, theme.primary);
            canvas.fillTriangle(cx - 5, cy - 9, cx + 5, cy - 9, cx, cy - 17, TFT_WHITE);
            canvas.fillTriangle(cx - 4, cy + 2, cx - 11, cy + 8, cx - 4, cy + 8, theme.secondary);
            canvas.fillTriangle(cx + 4, cy + 2, cx + 11, cy + 8, cx + 4, cy + 8, theme.secondary);
            canvas.fillTriangle(cx - 3, cy + 7, cx + 3, cy + 7, cx, cy + 14, TFT_ORANGE);
        } else if (tileIdx == 1) {
            // CLOCK / ORBIT ICON (Physics & Time)
            canvas.drawCircle(cx, cy - 1, 9, theme.primary);
            canvas.fillCircle(cx + 5, cy - 6, 3, TFT_CYAN);
            canvas.drawFastHLine(cx - 2, cy - 1, 6, theme.secondary);
            canvas.drawFastVLine(cx, cy - 5, 5, theme.secondary);
        } else if (tileIdx == 2) {
            // SPEAKER ICON (Audio Settings)
            canvas.fillRect(cx - 7, cy - 4, 4, 8, theme.primary);
            canvas.fillTriangle(cx - 4, cy - 4, cx + 2, cy - 8, cx + 2, cy + 8, theme.primary);
            canvas.fillTriangle(cx - 4, cy + 4, cx + 2, cy - 8, cx + 2, cy + 8, theme.primary);
            canvas.drawCircle(cx + 4, cy, 4, TFT_YELLOW);
            canvas.drawCircle(cx + 6, cy, 7, theme.secondary);
        } else if (tileIdx == 3) {
            // PALETTE / CRT MONITOR ICON (Themes)
            canvas.drawRoundRect(cx - 11, cy - 8, 22, 16, 2, theme.primary);
            canvas.fillRect(cx - 8, cy - 5, 4, 10, TFT_GREEN);
            canvas.fillRect(cx - 2, cy - 5, 4, 10, TFT_ORANGE);
            canvas.fillRect(cx + 4, cy - 5, 4, 10, TFT_CYAN);
            canvas.drawFastHLine(cx - 6, cy + 9, 12, theme.secondary);
        }
    }

    // =========================================================================
    // 1. MOBILE PHONE STYLE MAIN MENU SCREEN (4 Tiles + Detail Submenus)
    // =========================================================================
    void renderMainMenu(const SimConfig& cfg, int activeTile, bool inSubMenu, int subMenuIndex) {
        ColorTheme theme = ColorTheme::get(cfg.themePreset);
        canvas.fillScreen(canvas.color565(4, 8, 14));

        if (!inSubMenu) {
            // LEVEL 1: CAROUSEL HOME SCREEN (Swipable App Tiles)
            canvas.fillRect(0, 0, W, 18, theme.bgPanel);
            canvas.drawFastHLine(0, 18, W, theme.border);
            canvas.setTextSize(1);
            canvas.setTextColor(theme.primary);
            canvas.setCursor(8, 5);
            canvas.print("APOLLO OS // CARDPUTER");

            canvas.setTextColor(theme.secondary);
            canvas.setCursor(W - 68, 5);
            canvas.printf("< %d / 4 >", activeTile + 1);

            // Active Card (x = 18, y = 23, w = 204, h = 94)
            int cx = 18;
            int cy = 23;
            int cw = 204;
            int ch = 94;

            canvas.fillRoundRect(cx, cy, cw, ch, 4, theme.bgPanel);
            canvas.drawRoundRect(cx, cy, cw, ch, 4, theme.primary);

            // Left & Right Swipe Arrows
            canvas.setTextColor(activeTile > 0 ? theme.primary : theme.dim);
            canvas.setCursor(5, 62);
            canvas.print("<");
            canvas.setTextColor(activeTile < 3 ? theme.primary : theme.dim);
            canvas.setCursor(228, 62);
            canvas.print(">");

            // Centered Tile Icon
            drawTileIcon(activeTile, cx + cw / 2, cy + 22, theme);

            // Centered Title & Status
            if (activeTile == 0) {
                drawCenteredText(cx, cy + 42, cw, "1. FLIGHT MODES", theme.primary, 1);
                const char* mStr = (cfg.controlMode == ControlMode::FLIGHT_DIRECTOR) ? "Houston (Director)"
                                 : ((cfg.controlMode == ControlMode::SMART_ASSIST) ? "Semi-Auto (AGC)" : "Autonomous Demo");
                drawCenteredText(cx, cy + 54, cw, mStr, TFT_WHITE, 1);
            } else if (activeTile == 1) {
                drawCenteredText(cx, cy + 42, cw, "2. PHYSICS & TIME", theme.primary, 1);
                const char* pStr = (cfg.physicsScale == PhysicsScale::ARCADE_FAST) ? "Scale: Arcade (Fast)" : "Scale: Realistic (1:1)";
                drawCenteredText(cx, cy + 54, cw, pStr, TFT_WHITE, 1);
            } else if (activeTile == 2) {
                drawCenteredText(cx, cy + 42, cw, "3. AUDIO SETTINGS", theme.primary, 1);
                const char* aStr = cfg.soundEnabled ? "Audio: ENABLED (Quindar)" : "Audio: MUTED";
                drawCenteredText(cx, cy + 54, cw, aStr, TFT_WHITE, 1);
            } else if (activeTile == 3) {
                drawCenteredText(cx, cy + 42, cw, "4. DISPLAY THEME", theme.primary, 1);
                drawCenteredText(cx, cy + 54, cw, theme.name, TFT_WHITE, 1);
            }

            // Centered Button
            drawCenteredButton(cx + 22, cy + 68, cw - 44, 18, "[ ENTER ] OPEN TILE", true, theme);

            // Bottom Navigation Dots
            canvas.fillRect(0, H - 15, W, 15, canvas.color565(6, 12, 18));
            canvas.drawFastHLine(0, H - 15, W, canvas.color565(30, 45, 60));
            int dotCenterX = W / 2;
            int dotY = H - 8;
            for (int i = 0; i < 4; ++i) {
                int dx = dotCenterX - 21 + i * 14;
                if (i == activeTile) {
                    canvas.fillCircle(dx, dotY, 3, theme.primary);
                } else {
                    canvas.drawCircle(dx, dotY, 2, theme.dim);
                }
            }
            canvas.setTextColor(canvas.color565(120, 150, 170));
            canvas.setCursor(6, H - 11);
            canvas.print("[A/D] Swipe");
            canvas.setCursor(W - 74, H - 11);
            canvas.print("[ENT] Open");

        } else {
            // LEVEL 2: DIVE-IN DETAIL SUB-MENU (Mobile Sub-screen)
            canvas.fillRect(0, 0, W, 18, theme.bgPanel);
            canvas.drawFastHLine(0, 18, W, theme.border);

            // Category Title (Centered across full width)
            if (activeTile == 0) drawCenteredText(0, 5, W, "SELECT FLIGHT MODE", theme.primary, 1);
            else if (activeTile == 1) drawCenteredText(0, 5, W, "PHYSICS & TIME SCALE", theme.primary, 1);
            else if (activeTile == 2) drawCenteredText(0, 5, W, "AUDIO / SOUND SETTINGS", theme.primary, 1);
            else if (activeTile == 3) drawCenteredText(0, 5, W, "COLOR PALETTE THEME", theme.primary, 1);

            int bx = 12;
            int bw = W - 24; // 216 px

            if (activeTile == 0) {
                // 3 Flight Modes
                const char* mNames[3] = {
                    "HOUSTON (FLIGHT DIRECTOR)",
                    "SEMI-AUTO (AGC PILOT)",
                    "AUTONOMOUS DEMO FLIGHT"
                };
                int y = 26;
                for (int i = 0; i < 3; ++i) {
                    char label[44];
                    bool isCur = (static_cast<int>(cfg.controlMode) == i);
                    snprintf(label, sizeof(label), "%s %s", isCur ? "(o)" : "( )", mNames[i]);
                    drawCenteredButton(bx, y, bw, 22, label, subMenuIndex == i, theme);
                    y += 26;
                }
            } else if (activeTile == 1) {
                // 2 Physics Scales
                const char* pNames[2] = {
                    "ARCADE SCALE (8-10h GET)",
                    "REALISTIC 1:1 (7-8 DAYS)"
                };
                int y = 32;
                for (int i = 0; i < 2; ++i) {
                    char label[44];
                    bool isCur = (static_cast<int>(cfg.physicsScale) == i);
                    snprintf(label, sizeof(label), "%s %s", isCur ? "(o)" : "( )", pNames[i]);
                    drawCenteredButton(bx, y, bw, 26, label, subMenuIndex == i, theme);
                    y += 32;
                }
            } else if (activeTile == 2) {
                // 2 Audio Options
                const char* aNames[2] = {
                    "AUDIO: ON (QUINDAR BEEP)",
                    "AUDIO: MUTE (SILENT)"
                };
                int y = 32;
                for (int i = 0; i < 2; ++i) {
                    char label[44];
                    bool isCur = (i == 0) ? cfg.soundEnabled : !cfg.soundEnabled;
                    snprintf(label, sizeof(label), "%s %s", isCur ? "(o)" : "( )", aNames[i]);
                    drawCenteredButton(bx, y, bw, 26, label, subMenuIndex == i, theme);
                    y += 32;
                }
            } else if (activeTile == 3) {
                // 4 Themes
                const char* tNames[4] = {
                    "APOLLO CRT GREEN",
                    "AMBER TERMINAL",
                    "CYAN AVIONICS",
                    "RED/ORANGE ALERT"
                };
                int y = 24;
                for (int i = 0; i < 4; ++i) {
                    char label[40];
                    snprintf(label, sizeof(label), "%s %s", (static_cast<int>(cfg.themePreset) == i) ? "(o)" : "( )", tNames[i]);
                    drawCenteredButton(bx, y, bw, 18, label, subMenuIndex == i, theme);
                    y += 23;
                }
            }

            // Bottom Bar for Sub-menu
            canvas.fillRect(0, H - 15, W, 15, canvas.color565(6, 12, 18));
            canvas.drawFastHLine(0, H - 15, W, canvas.color565(30, 45, 60));
            canvas.setTextColor(canvas.color565(120, 150, 170));
            canvas.setCursor(6, H - 11);
            canvas.print("[W/S] Move");
            drawCenteredText(0, H - 11, W, "[ENT] Select", theme.secondary, 1);
            canvas.setCursor(W - 68, H - 11);
            canvas.print("[DEL] Back");
        }


        canvas.pushSprite(0, 0);
    }

public:
    // =========================================================================
    // 2. APOLLO AGC / DSKY COMPUTER INTERFACE (Themed)
    // =========================================================================
    void renderDskyComputer(const RocketState& r,
                            const SimConfig& cfg,
                            const TrajectoryForecast& traj,
                            double missionTime,
                            int timeWarp,
                            bool showAbortModal = false) {
        ColorTheme theme = ColorTheme::get(cfg.themePreset);
        canvas.fillScreen(canvas.color565(6, 8, 12));
        renderTopStatusBar(r, missionTime, timeWarp, true, theme);

        // --- LEFT PANEL: FDAI NavBall & Ship Status ---
        int leftW = 102;
        canvas.drawRect(1, 15, leftW, H - 16, theme.border);

        // NavBall circle
        int navX = 51;
        int navY = 56;
        int navR = 34;
        canvas.fillCircle(navX, navY, navR, canvas.color565(8, 14, 20));
        canvas.drawCircle(navX, navY, navR, theme.dim);
        // Crosshairs
        canvas.drawFastHLine(navX - navR, navY, navR * 2, theme.dim);
        canvas.drawFastVLine(navX, navY - navR, navR * 2, theme.dim);

        // Local Horizon vector
        Vector2D moonPos = OrbitalPhysics::getMoonPos(cfg, missionTime);
        bool inMoonSOI = ((r.pos - moonPos).magnitude() < cfg.moonSOI);
        Vector2D relBody = inMoonSOI ? (r.pos - moonPos) : r.pos;
        double upAngle = std::atan2(relBody.y, relBody.x);
        int ux = navX + static_cast<int>(std::cos(upAngle) * (navR - 6));
        int uy = navY - static_cast<int>(std::sin(upAngle) * (navR - 6));
        canvas.drawLine(navX, navY, ux, uy, canvas.color565(40, 90, 180));

        // Prograde / Retrograde (relative to active gravity body)
        Vector2D relVel = inMoonSOI ? (r.vel - OrbitalPhysics::getMoonVel(cfg, missionTime)) : r.vel;
        if (relVel.magnitude() > 5.0) {
            double proAngle = std::atan2(relVel.y, relVel.x);
            int px = navX + static_cast<int>(std::cos(proAngle) * (navR - 7));
            int py = navY - static_cast<int>(std::sin(proAngle) * (navR - 7));
            canvas.drawCircle(px, py, 3, TFT_GREEN);

            double retAngle = proAngle + M_PI;
            int rx = navX + static_cast<int>(std::cos(retAngle) * (navR - 7));
            int ry = navY - static_cast<int>(std::sin(retAngle) * (navR - 7));
            canvas.drawLine(rx - 2, ry - 2, rx + 2, ry + 2, TFT_YELLOW);
            canvas.drawLine(rx - 2, ry + 2, rx + 2, ry - 2, TFT_YELLOW);
        }

        // Ship Nose needle
        int sx = navX + static_cast<int>(std::cos(r.angleRad) * (navR - 4));
        int sy = navY - static_cast<int>(std::sin(r.angleRad) * (navR - 4));
        canvas.drawLine(navX, navY, sx, sy, TFT_WHITE);
        canvas.fillCircle(sx, sy, 2, TFT_RED);
        canvas.fillCircle(navX, navY, 2, theme.primary);

        // Gauges
        int gy = 95;
        canvas.setTextSize(1);
        canvas.setTextColor(canvas.color565(180, 200, 220));
        canvas.setCursor(5, gy);
        canvas.printf("THR:%3d%%", static_cast<int>(r.throttle * 100));
        canvas.drawRect(56, gy + 1, 42, 6, theme.border);
        canvas.fillRect(57, gy + 2, static_cast<int>(40 * r.throttle), 4, TFT_GREEN);

        gy += 11;
        double fuelFrac = (r.maxFuelKg > 0) ? (r.fuelKg / r.maxFuelKg) : 0.0;
        fuelFrac = std::min(1.0, std::max(0.0, fuelFrac));
        canvas.setTextColor(canvas.color565(180, 200, 220));
        canvas.setCursor(5, gy);
        canvas.printf("STG %d FL", r.stage + 1);
        canvas.drawRect(56, gy + 1, 42, 6, theme.border);
        // Bright Orange fuel fill regardless of theme!
        canvas.fillRect(57, gy + 2, static_cast<int>(40 * fuelFrac), 4, TFT_ORANGE);

        gy += 11;
        canvas.setTextColor(TFT_YELLOW);
        canvas.setCursor(5, gy);
        canvas.printf("dV:%4.0f  G:%.1f", r.deltaVRemaining, r.gForce);

        // --- RIGHT PANEL: DSKY DIGITAL REGISTERS ---
        int dskyX = 105;
        int dskyW = W - dskyX - 1;
        canvas.fillRect(dskyX, 15, dskyW, H - 16, theme.bgPanel);
        canvas.drawRect(dskyX, 15, dskyW, H - 16, theme.border);

        // Status Lamps
        bool autoLampOn = r.autoBurnActive || r.isDemoFlight || (cfg.controlMode != ControlMode::AUTONOMOUS_DEMO);
        drawDskyLamp(dskyX + 3,  18, "BURN", r.throttle > 0.01, TFT_ORANGE);
        drawDskyLamp(dskyX + 35, 18, "AUTO", autoLampOn, TFT_GREEN);
        drawDskyLamp(dskyX + 67, 18, "SOI",  inMoonSOI || traj.entersMoonSOI, TFT_MAGENTA);
        drawDskyLamp(dskyX + 96, 18, "HEAT", r.heatShieldTemp > 600.0, TFT_RED);

        // PROG / VERB / NOUN
        canvas.drawFastHLine(dskyX + 2, 29, dskyW - 4, theme.dim);
        canvas.setTextColor(theme.dim);
        canvas.setCursor(dskyX + 4, 32);
        canvas.print("PROG   VERB   NOUN");

        canvas.setTextColor(theme.primary);
        canvas.setCursor(dskyX + 4, 41);
        canvas.printf("P%02d    V%02d    N%02d",
                      static_cast<int>(r.activeProg), r.verb, r.noun);

        canvas.drawFastHLine(dskyX + 2, 51, dskyW - 4, theme.dim);

        // Registers
        double currentAltKm = inMoonSOI
            ? ((r.pos - moonPos).magnitude() - cfg.moonRadius) / 1000.0
            : (r.pos.magnitude() - cfg.earthRadius) / 1000.0;
        double currentVel = inMoonSOI
            ? (r.vel - OrbitalPhysics::getMoonVel(cfg, missionTime)).magnitude()
            : r.vel.magnitude();

        int ry = 55;
        canvas.setTextColor(theme.primary);
        canvas.setCursor(dskyX + 4, ry);
        if (std::abs(currentAltKm) >= 10000.0) {
            canvas.printf("R1 %s:%+8.0f km", inMoonSOI ? "ALTM" : "ALTE", currentAltKm);
        } else {
            canvas.printf("R1 %s:%+8.1f km", inMoonSOI ? "ALTM" : "ALTE", currentAltKm);
        }

        ry += 13;
        canvas.setCursor(dskyX + 4, ry);
        const char* vRef = inMoonSOI ? "V_M" : "V_E";
        if (currentVel >= 10000.0) {
            canvas.printf("R2 %s :%+8.0f m/s", vRef, currentVel);
        } else {
            canvas.printf("R2 %s :%+8.1f m/s", vRef, currentVel);
        }

        ry += 13;
        canvas.setCursor(dskyX + 4, ry);
        if (r.autoBurnActive && r.burnRemainingDv > 0.0) {
            canvas.setTextColor(TFT_YELLOW);
            canvas.printf("R3 dV  :%+8.1f m/s", r.burnRemainingDv);
        } else if (traj.entersMoonSOI) {
            canvas.setTextColor(TFT_MAGENTA);
            canvas.printf("R3 M.PE:%+8.0f km", traj.closestMoonAlt / 1000.0);
        } else {
            canvas.setTextColor(traj.periapsisAlt < 0 ? TFT_RED : TFT_CYAN);
            double apKm = traj.apoapsisAlt / 1000.0;
            double peKm = std::max(-999.0, traj.periapsisAlt / 1000.0);
            if (apKm >= 100000.0) {
                canvas.printf("R3 AP/PE:%3.0fk/%4.0fk", apKm / 1000.0, peKm);
            } else {
                canvas.printf("R3 AP/PE:%5.0f/%4.0fk", apKm, peKm);
            }
        }

        // Program Selector Box (max 21 chars per line inside 130px box)
        canvas.drawFastHLine(dskyX + 2, 94, dskyW - 4, theme.border);
        canvas.fillRect(dskyX + 2, 95, dskyW - 4, 22, theme.bgPanel);
        if (r.isDemoFlight) {
            drawCenteredText(dskyX + 2, 97, dskyW - 4, r.actionPrompt[0] ? r.actionPrompt : "AUTONOMOUS DEMO", theme.secondary, 1);
            char progStr[32];
            snprintf(progStr, sizeof(progStr), "Prog: P%02d %s", static_cast<int>(r.activeProg), getProgramTitle(r.activeProg));
            drawCenteredText(dskyX + 2, 107, dskyW - 4, progStr, theme.primary, 1);
            drawCenteredText(dskyX + 2, 120, dskyW - 4, "[M]Map [1-5]Wrp [DEL]", theme.dim, 1);
        } else if (cfg.controlMode == ControlMode::FLIGHT_DIRECTOR) {
            uint16_t promptCol = (r.actionPrompt[0] == '[') ? TFT_YELLOW : theme.secondary;
            drawCenteredText(dskyX + 2, 97, dskyW - 4, r.actionPrompt[0] ? r.actionPrompt : "HOUSTON: STANDBY", promptCol, 1);
            char progStr[32];
            snprintf(progStr, sizeof(progStr), "Prog: P%02d %s", static_cast<int>(r.activeProg), getProgramTitle(r.activeProg));
            drawCenteredText(dskyX + 2, 107, dskyW - 4, progStr, theme.primary, 1);
            drawCenteredText(dskyX + 2, 120, dskyW - 4, "[M]Map [1-5]Wrp [DEL]", theme.dim, 1);
        } else {
            char nextStr[32];
            if (r.selectedProg == r.activeProg) {
                snprintf(nextStr, sizeof(nextStr), "Prog: P%02d %s", static_cast<int>(r.activeProg), getProgramTitle(r.activeProg));
            } else {
                snprintf(nextStr, sizeof(nextStr), "P%02d > [N] P%02d %s", static_cast<int>(r.activeProg), static_cast<int>(r.selectedProg), getProgramTitle(r.selectedProg));
            }
            drawCenteredText(dskyX + 2, 97, dskyW - 4, nextStr, theme.secondary, 1);
            bool urgentPrompt = (r.actionPrompt[0] == '[' || strncmp(r.actionPrompt, "ORBIT", 5) == 0 || strstr(r.actionPrompt, "[ENTER]") != nullptr || strstr(r.actionPrompt, "READY") != nullptr);
            uint16_t pCol = urgentPrompt ? TFT_YELLOW : theme.primary;
            drawCenteredText(dskyX + 2, 107, dskyW - 4, r.actionPrompt[0] ? r.actionPrompt : "[N]Prg [ENT]Go [SPC]", pCol, 1);
            drawCenteredText(dskyX + 2, 120, dskyW - 4, "[N]Prg [Z/X]Thr [SPC]", theme.dim, 1);
        }

        // Overwrite with Abort Confirmation Modal if triggered
        if (showAbortModal) {
            renderAbortModal(theme);
        } else if (r.isDemoFlight && millis() < r.demoNoticeTimer) {
            renderDemoNoticePopup();
        }


        canvas.pushSprite(0, 0);
    }

    // =========================================================================
    // 3. INTERACTIVE ORBIT MAP VIEW (Earth-Moon colors preserved)
    // =========================================================================
    void renderOrbitMap(const RocketState& r,
                        const SimConfig& cfg,
                        const TrajectoryForecast& traj,
                        double missionTime,
                        int timeWarp,
                        double zoomFactor,
                        MapCameraFocus camFocus,
                        Vector2D manualPanOffset,
                        bool showAbortModal = false) {
        ColorTheme theme = ColorTheme::get(cfg.themePreset);
        canvas.fillScreen(canvas.color565(4, 6, 14));
        renderTopStatusBar(r, missionTime, timeWarp, false, theme);

        int mapW = 175;
        int mapH = H - 14;
        int mapCenterX = mapW / 2;
        int mapCenterY = 14 + mapH / 2;

        Vector2D moonPos = OrbitalPhysics::getMoonPos(cfg, missionTime);
        double baseScale = 110.0 / (cfg.moonDistance * 1.15);
        double scale = baseScale * zoomFactor;

        Vector2D worldFocus(0, 0);
        if (camFocus == MapCameraFocus::SYSTEM_FIT) {
            worldFocus = moonPos * 0.45;
        } else if (camFocus == MapCameraFocus::FOLLOW_SHIP) {
            worldFocus = r.pos;
        } else if (camFocus == MapCameraFocus::CENTER_EARTH) {
            worldFocus = Vector2D(0, 0);
        } else if (camFocus == MapCameraFocus::CENTER_MOON) {
            worldFocus = moonPos;
        }
        worldFocus = worldFocus + manualPanOffset;

        auto worldToScreenX = [&](double wx) -> int {
            return mapCenterX + static_cast<int>((wx - worldFocus.x) * scale);
        };
        auto worldToScreenY = [&](double wy) -> int {
            return mapCenterY - static_cast<int>((wy - worldFocus.y) * scale);
        };

        // Reference orbit ring
        int ex = worldToScreenX(0.0);
        int ey = worldToScreenY(0.0);
        int moonOrbitR = static_cast<int>(cfg.moonDistance * scale);
        if (moonOrbitR > 5 && moonOrbitR < 2000) {
            canvas.drawCircle(ex, ey, moonOrbitR, canvas.color565(25, 32, 48));
        }

        // Earth (Natural colors preserved)
        int earthPixR = std::max(4, static_cast<int>(cfg.earthRadius * scale));
        int atmPixR = std::max(earthPixR + 2, static_cast<int>((cfg.earthRadius + cfg.atmosphereHeight) * scale));
        if (ex > -200 && ex < mapW + 200 && ey > -200 && ey < H + 200) {
            canvas.drawCircle(ex, ey, atmPixR, canvas.color565(40, 120, 220));
            canvas.fillCircle(ex, ey, earthPixR, canvas.color565(25, 85, 210));
            if (earthPixR >= 6) {
                canvas.fillCircle(ex - earthPixR / 3, ey - earthPixR / 4, earthPixR / 3, canvas.color565(40, 160, 70));
            }
            canvas.setTextColor(canvas.color565(100, 180, 255));
            canvas.setCursor(ex - 6, ey + earthPixR + 3);
            canvas.print("E");
        }

        // Moon & Moon SOI (Natural colors preserved)
        int mx = worldToScreenX(moonPos.x);
        int my = worldToScreenY(moonPos.y);
        int moonPixR = std::max(3, static_cast<int>(cfg.moonRadius * scale));
        int soiPixR = std::max(moonPixR + 4, static_cast<int>(cfg.moonSOI * scale));
        if (mx > -200 && mx < mapW + 200 && my > -200 && my < H + 200) {
            canvas.drawCircle(mx, my, soiPixR, canvas.color565(65, 50, 80));
            canvas.fillCircle(mx, my, moonPixR, canvas.color565(200, 200, 210));
            canvas.setTextColor(canvas.color565(200, 200, 220));
            canvas.setCursor(mx - 3, my + moonPixR + 3);
            canvas.print("M");
        }

        // Trajectory
        for (int i = 0; i < traj.count - 1; ++i) {
            int x1 = worldToScreenX(traj.pts[i].x);
            int y1 = worldToScreenY(traj.pts[i].y);
            int x2 = worldToScreenX(traj.pts[i + 1].x);
            int y2 = worldToScreenY(traj.pts[i + 1].y);

            if ((x1 >= 0 && x1 < mapW && y1 >= 14 && y1 < H) ||
                (x2 >= 0 && x2 < mapW && y2 >= 14 && y2 < H)) {
                uint16_t col = (r.phase == MissionPhase::TRANSEARTH_COAST || traj.freeReturnDetected)
                    ? TFT_MAGENTA
                    : (traj.entersMoonSOI ? TFT_CYAN : TFT_GREEN);
                canvas.drawLine(x1, y1, x2, y2, col);
            }
        }

        // Apoapsis Marker
        if (!r.onPad && traj.apoapsisAlt > 1000.0) {
            int apx = worldToScreenX(traj.apoapsisPos.x);
            int apy = worldToScreenY(traj.apoapsisPos.y);
            if (apx >= 2 && apx < mapW - 15 && apy >= 16 && apy < H - 6) {
                canvas.fillRect(apx - 1, apy - 1, 3, 3, TFT_YELLOW);
                canvas.setTextColor(TFT_YELLOW);
                canvas.setCursor(apx + 3, apy - 3);
                canvas.print("Ap");
            }
        }

        // Ship
        int sx = worldToScreenX(r.pos.x);
        int sy = worldToScreenY(r.pos.y);
        if (sx >= 0 && sx < mapW && sy >= 14 && sy < H) {
            int nx = sx + static_cast<int>(std::cos(r.angleRad) * 7);
            int ny = sy - static_cast<int>(std::sin(r.angleRad) * 7);
            canvas.drawLine(sx, sy, nx, ny, TFT_YELLOW);
            canvas.fillCircle(sx, sy, 2, TFT_RED);
        }

        // Controls hint
        canvas.setTextColor(theme.dim);
        canvas.setCursor(3, H - 10);
        const char* focusNames[] = {"SYS", "SHIP", "EARTH", "MOON"};
        canvas.printf("[+/-]Zm:%.1fx [C]Cam:%s", zoomFactor, focusNames[static_cast<int>(camFocus)]);

        // --- RIGHT SIDEBAR TELEMETRY (Themed) ---
        int sideX = 175;
        canvas.fillRect(sideX, 14, W - sideX, H - 14, theme.bgPanel);
        canvas.drawFastVLine(sideX, 14, H - 14, theme.border);

        bool inMoonSOI = ((r.pos - moonPos).magnitude() < cfg.moonSOI);
        double altKm = inMoonSOI
            ? ((r.pos - moonPos).magnitude() - cfg.moonRadius) / 1000.0
            : (r.pos.magnitude() - cfg.earthRadius) / 1000.0;
        double velMs = inMoonSOI
            ? (r.vel - OrbitalPhysics::getMoonVel(cfg, missionTime)).magnitude()
            : r.vel.magnitude();

        int ty = 17;
        canvas.setTextColor(theme.dim);
        canvas.setCursor(sideX + 3, ty);
        canvas.print(inMoonSOI ? "M.ALT:" : "ALT:");
        ty += 9;
        canvas.setTextColor(TFT_WHITE);
        canvas.setCursor(sideX + 3, ty);
        canvas.printf("%.0f km", altKm);

        ty += 11;
        canvas.setTextColor(theme.dim);
        canvas.setCursor(sideX + 3, ty);
        canvas.print("VEL:");
        ty += 9;
        canvas.setTextColor(TFT_WHITE);
        canvas.setCursor(sideX + 3, ty);
        canvas.printf("%.0f m/s", velMs);

        ty += 11;
        canvas.setTextColor(theme.dim);
        canvas.setCursor(sideX + 3, ty);
        canvas.print("AP / PE:");
        ty += 9;
        canvas.setTextColor(traj.periapsisAlt < 0 ? TFT_RED : TFT_GREEN);
        canvas.setCursor(sideX + 3, ty);
        double apKm = traj.apoapsisAlt / 1000.0;
        double peKm = std::max(-999.0, traj.periapsisAlt / 1000.0);
        if (apKm >= 10000.0) {
            canvas.printf("%.0fk/%.0fk", apKm / 1000.0, peKm);
        } else {
            canvas.printf("%.0f/%.0fk", apKm, peKm);
        }

        ty += 11;
        canvas.setTextColor(theme.dim);
        canvas.setCursor(sideX + 3, ty);
        canvas.print("MOON DIST:");
        ty += 9;
        double mDistKm = ((r.pos - moonPos).magnitude() - cfg.moonRadius) / 1000.0;
        canvas.setTextColor(traj.entersMoonSOI ? TFT_MAGENTA : TFT_CYAN);
        canvas.setCursor(sideX + 3, ty);
        canvas.printf("%.0f km", mDistKm);

        ty += 12;
        canvas.setTextColor(theme.secondary);
        canvas.setCursor(sideX + 3, ty);
        if (r.autoBurnActive) {
            canvas.printf("P%02d RUN", static_cast<int>(r.activeProg));
        } else if (r.selectedProg != r.activeProg && cfg.controlMode == ControlMode::SMART_ASSIST) {
            canvas.printf("P%02d/P%02d", static_cast<int>(r.activeProg), static_cast<int>(r.selectedProg));
        } else {
            canvas.printf("P%02d RDY", static_cast<int>(r.activeProg));
        }

        ty += 10;
        if (traj.freeReturnDetected) {
            canvas.setTextColor(TFT_MAGENTA);
            canvas.setCursor(sideX + 3, ty);
            canvas.print("FREE-RET!");
        } else if (traj.entersMoonSOI) {
            canvas.setTextColor(TFT_CYAN);
            canvas.setCursor(sideX + 3, ty);
            canvas.print("MOON ENC!");
        } else {
            canvas.setTextColor(theme.dim);
            canvas.setCursor(sideX + 3, ty);
            canvas.print("[TAB]DSKY");
        }

        if (showAbortModal) {
            renderAbortModal(theme);
        } else if (r.isDemoFlight && millis() < r.demoNoticeTimer) {
            renderDemoNoticePopup();
        }


        canvas.pushSprite(0, 0);
    }

    // Render 3-second Demo Notice Popup if user tries to manually fly during Demo
    void renderDemoNoticePopup() {
        int pw = 210;
        int ph = 64;
        int px = (W - pw) / 2;
        int py = (H - ph) / 2;

        canvas.fillRect(px - 2, py - 2, pw + 4, ph + 4, TFT_BLACK);
        canvas.fillRoundRect(px, py, pw, ph, 4, canvas.color565(18, 32, 45));
        canvas.drawRoundRect(px, py, pw, ph, 4, TFT_CYAN);

        canvas.setTextSize(1);
        canvas.setTextColor(TFT_YELLOW);
        canvas.setCursor(px + 14, py + 10);
        canvas.print("=== AUTONOMOUS DEMO FLIGHT ===");

        canvas.setTextColor(TFT_WHITE);
        canvas.setCursor(px + 10, py + 26);
        canvas.print("Ship is controlled automatically!");
        canvas.setTextColor(canvas.color565(160, 200, 230));
        canvas.setCursor(px + 10, py + 38);
        canvas.print("You can only toggle [TAB] Map,");
        canvas.setCursor(px + 10, py + 49);
        canvas.print("change [1-5] Warp & [DEL] Quit.");
    }

    // =========================================================================
    // 4. MODAL: ABORT MISSION CONFIRMATION
    // =========================================================================
    void renderAbortModal(const ColorTheme& theme) {
        int mw = 208;
        int mh = 84;
        int mx = (W - mw) / 2; // 16
        int my = (H - mh) / 2; // 25

        // Modal backdrop and border
        canvas.fillRect(mx - 4, my - 4, mw + 8, mh + 8, canvas.color565(10, 12, 16));
        canvas.fillRect(mx, my, mw, mh, canvas.color565(25, 15, 20));
        canvas.drawRoundRect(mx, my, mw, mh, 4, TFT_RED);
        canvas.drawRoundRect(mx + 1, my + 1, mw - 2, mh - 2, 3, TFT_ORANGE);

        drawCenteredText(mx, my + 8, mw, "!!! ABORT MISSION WARNING !!!", TFT_RED, 1);
        drawCenteredText(mx, my + 24, mw, "Are you sure you want to abort?", TFT_WHITE, 1);
        drawCenteredText(mx, my + 37, mw, "Current flight will be lost!", canvas.color565(210, 180, 190), 1);

        // Buttons (strictly centered with balanced margins)
        int bw = 88;
        int bh = 20;
        int b1x = mx + 12;
        int b2x = mx + mw - 12 - bw; // mx + 108
        int by = my + 54;

        // Button 1: ABORT (Centered)
        canvas.fillRoundRect(b1x, by, bw, bh, 3, TFT_RED);
        canvas.drawRoundRect(b1x, by, bw, bh, 3, TFT_WHITE);
        drawCenteredText(b1x, by + 6, bw, "[ENTER] ABORT", TFT_WHITE, 1);

        // Button 2: CANCEL (Centered)
        canvas.fillRoundRect(b2x, by, bw, bh, 3, canvas.color565(35, 45, 55));
        canvas.drawRoundRect(b2x, by, bw, bh, 3, canvas.color565(80, 100, 120));
        drawCenteredText(b2x, by + 6, bw, "[DEL] CANCEL", TFT_WHITE, 1);
    }

    // =========================================================================
    // 5. MISSION SUMMARY SCREEN (Themed & Strictly Centered)
    // =========================================================================
    void renderMissionSummary(const RocketState& r, double missionTime, const SimConfig& cfg) {
        ColorTheme theme = ColorTheme::get(cfg.themePreset);
        canvas.fillScreen(canvas.color565(4, 8, 14));
        uint16_t borderCol = r.splashedDown ? theme.primary : TFT_RED;
        canvas.drawRoundRect(4, 4, W - 8, H - 8, 4, borderCol);
        canvas.drawRoundRect(6, 6, W - 12, H - 12, 3, theme.border);

        // Header Title (Centered)
        const char* titleStr = r.splashedDown
            ? "*** APOLLO SPLASHDOWN SUCCESS! ***"
            : "!!! MISSION FAILED: SPACECRAFT LOST !!!";
        drawCenteredText(0, 16, W, titleStr, borderCol, 1);

        // Decorative horizontal divider
        canvas.drawFastHLine(24, 30, W - 48, theme.border);

        int totalSecs = static_cast<int>(missionTime);
        int hrs = totalSecs / 3600;
        int mins = (totalSecs % 3600) / 60;
        int secs = totalSecs % 60;
        char timeBuf[48];
        if (hrs > 0) {
            snprintf(timeBuf, sizeof(timeBuf), "Mission Elapsed Time: %02dh %02dm %02ds", hrs, mins, secs);
        } else {
            snprintf(timeBuf, sizeof(timeBuf), "Mission Elapsed Time: %02dm %02ds", mins, secs);
        }
        drawCenteredText(0, 42, W, timeBuf, TFT_WHITE, 1);

        char stageBuf[64];
        if (r.splashedDown) {
            snprintf(stageBuf, sizeof(stageBuf), "Capsule: Command Module (CM) | Chutes OK");
        } else {
            snprintf(stageBuf, sizeof(stageBuf), "Final Stage: %d   Fuel Left: %.0f kg", r.stage + 1, r.fuelKg);
        }
        drawCenteredText(0, 58, W, stageBuf, theme.secondary, 1);

        // Scenario result / Specific Failure Reason
        char resStr[64];
        if (r.splashedDown) {
            snprintf(resStr, sizeof(resStr), "MISSION OBJECTIVES ACHIEVED");
        } else {
            snprintf(resStr, sizeof(resStr), "REASON: %s", r.failureReason[0] ? r.failureReason : "SPACECRAFT DESTROYED");
        }
        drawCenteredText(0, 74, W, resStr, r.splashedDown ? TFT_GREEN : TFT_ORANGE, 1);

        // Centered Button
        drawCenteredButton(24, 96, W - 48, 20, "[ ENTER / M ] RETURN TO MENU", true, theme);


        canvas.pushSprite(0, 0);
    }

    // =========================================================================
    // INTRO / BOOT ANIMATION: NASA AGC DSKY SELF-TEST & BOOT SEQUENCE (1969)
    // =========================================================================
    void playBootSequence(bool soundEnabled = true) {
        // Authentic Apollo DSKY annunciator lamps
        const char* lamps[14] = {
            "UPLINK", "NO ATT",  "STBY",   "KEY REL",
            "OPR ERR","TEMP",    "GIMBAL", "PROG",
            "RESTART","TRACKER", "VERB",   "NOUN",
            "BURN",   "AUTO"
        };

        // Total duration ~2.6 seconds divided into steps
        // Check for key press to allow instant skip
        auto checkSkip = []() -> bool {
            M5Cardputer.update();
            return M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed();
        };

        // --- STEP 1: Cold Power-On / CRT Phosphor Warm-Up (~300 ms) ---
        for (int frame = 0; frame < 6; ++frame) {
            if (checkSkip()) return;
            canvas.fillScreen(TFT_BLACK);
            // Draw expanding subtle green CRT scanline glow
            int scanH = (frame + 1) * 22;
            int yMid = H / 2;
            uint16_t glow = canvas.color565(0, 15 + frame * 8, 0);
            canvas.fillRect(0, yMid - scanH / 2, W, scanH, glow);
            canvas.drawFastHLine(0, yMid, W, canvas.color565(120, 255, 120));
            canvas.pushSprite(0, 0);
            delay(40);
        }

        // --- STEP 2: Lamp Test (All Annunciator Lamps + Segments 88888) (~700 ms) ---
        // Authentic Apollo AGC Lamp Test (VERB 35 / Self-Test)
        if (soundEnabled) {
            M5.Speaker.tone(1800, 60);
        }
        neopixelWrite(21, 50, 35, 0); // Amber warning LED on Cardputer

        for (int frame = 0; frame < 12; ++frame) {
            if (checkSkip()) { neopixelWrite(21, 0, 0, 0); return; }
            canvas.fillScreen(canvas.color565(8, 12, 10));

            // Top Header: AGC RAYTHEON BLOCK II
            canvas.fillRect(0, 0, W, 14, canvas.color565(18, 30, 22));
            canvas.drawFastHLine(0, 14, W, canvas.color565(60, 140, 80));
            drawCenteredText(0, 3, W, "APOLLO GUIDANCE COMPUTER // BLOCK II", canvas.color565(100, 255, 120), 1);

            // Left Box: 14 Annunciator Status Lamps in full test glow
            int lx = 8;
            int ly = 20;
            for (int i = 0; i < 14; ++i) {
                int col = i % 2;
                int row = i / 2;
                int bx = lx + col * 48;
                int by = ly + row * 13;
                bool blink = (frame % 3 != 0);
                uint16_t bg = blink ? TFT_ORANGE : canvas.color565(50, 30, 0);
                uint16_t fg = blink ? TFT_BLACK : canvas.color565(180, 140, 60);
                canvas.fillRect(bx, by, 45, 11, bg);
                canvas.drawRect(bx, by, 45, 11, TFT_WHITE);
                canvas.setTextColor(fg);
                canvas.setCursor(bx + 3, by + 2);
                canvas.print(lamps[i]);
            }

            // Right Box: DSKY Registers 88888 (Segment check)
            int rx = 112;
            int rw = W - rx - 8;
            canvas.drawRect(rx, 20, rw, 94, canvas.color565(60, 140, 80));
            canvas.fillRect(rx + 1, 21, rw - 2, 92, canvas.color565(4, 8, 6));

            canvas.setTextColor(canvas.color565(57, 255, 20));
            canvas.setCursor(rx + 6, 26);
            canvas.print("PROG  [ 35 ] SELF-TST");
            canvas.setCursor(rx + 6, 42);
            canvas.print("VERB  [ 35 ] NOUN [88]");
            canvas.drawFastHLine(rx + 4, 56, rw - 8, canvas.color565(30, 80, 50));
            canvas.setCursor(rx + 6, 62);
            canvas.print("R1 +88888.88 m");
            canvas.setCursor(rx + 6, 76);
            canvas.print("R2 +88888.88 m/s");
            canvas.setCursor(rx + 6, 90);
            canvas.print("R3 +88888.88 dV");

            // Subsystem verification line
            canvas.fillRect(0, H - 15, W, 15, canvas.color565(12, 18, 14));
            canvas.drawFastHLine(0, H - 15, W, canvas.color565(40, 80, 50));
            canvas.setTextColor(canvas.color565(255, 200, 40));
            canvas.setCursor(8, H - 11);
            canvas.print("LAMP & RELAY TEST IN PROGRESS...");

            canvas.pushSprite(0, 0);
            delay(50);
        }

        // --- STEP 3: Subsystem Diagnostics & Memory Check (~800 ms) ---
        const char* diagSteps[4] = {
            "ROPE CORE MEMORY 36KW : OK",
            "INERTIAL MEASUREMENT UNIT (IMU) : ALIGNED",
            "SATURN LVDC DIGITAL BUS : READY",
            "DSKY TELEMETRY DOWNLINK : LOCKED"
        };

        for (int s = 0; s < 4; ++s) {
            if (checkSkip()) { neopixelWrite(21, 0, 0, 0); return; }
            if (soundEnabled) {
                M5.Speaker.tone(1200 + s * 300, 40);
            }
            neopixelWrite(21, 0, 40 + s * 25, 30 + s * 20); // Cyan progression

            canvas.fillScreen(canvas.color565(4, 8, 6));
            canvas.drawRect(6, 6, W - 12, H - 12, canvas.color565(40, 100, 60));
            canvas.drawRect(8, 8, W - 16, H - 16, canvas.color565(20, 60, 40));

            // Top title
            canvas.fillRect(10, 10, W - 20, 16, canvas.color565(15, 35, 20));
            drawCenteredText(10, 14, W - 20, "APOLLO GUIDANCE COMPUTER (AGC)", canvas.color565(57, 255, 20), 1);

            // Progress text lines
            int ty = 34;
            for (int i = 0; i <= s; ++i) {
                canvas.setTextColor(canvas.color565(0, 229, 255));
                canvas.setCursor(14, ty);
                canvas.print("[OK] ");
                canvas.setTextColor(TFT_WHITE);
                canvas.print(diagSteps[i]);
                ty += 14;
            }

            // Progress bar
            int barW = W - 40;
            int fillW = (barW * (s + 1)) / 4;
            canvas.drawRect(20, H - 28, barW, 8, canvas.color565(60, 140, 80));
            canvas.fillRect(22, H - 26, fillW - 4, 4, canvas.color565(57, 255, 20));

            drawCenteredText(0, H - 17, W, "VERIFYING SATURN V AVIONICS...", canvas.color565(140, 180, 160), 1);
            canvas.pushSprite(0, 0);
            delay(160);
        }

        // --- STEP 4: NASA Apollo 11 Quindar Beep & "GO FOR LAUNCH" Banner (~800 ms) ---
        if (soundEnabled) {
            AudioSys::playQuindarTone(true); // 2524 Hz authentic NASA tone
        }
        neopixelWrite(21, 0, 120, 40); // Emerald green launch lock

        for (int frame = 0; frame < 15; ++frame) {
            if (checkSkip()) { neopixelWrite(21, 0, 0, 0); return; }
            canvas.fillScreen(canvas.color565(6, 10, 16));

            // Glowing border
            canvas.drawRect(4, 4, W - 8, H - 8, canvas.color565(0, 220, 255));
            canvas.drawRect(6, 6, W - 12, H - 12, canvas.color565(0, 100, 140));

            // Mission Emblem Banner
            drawCenteredText(0, 12, W, "★  APOLLO 11  ★", TFT_YELLOW, 2);
            drawCenteredText(0, 32, W, "LUNAR EXPEDITION SIMULATOR", canvas.color565(0, 229, 255), 1);
            drawCenteredText(0, 46, W, "CAPE CANAVERAL // PAD 39A", canvas.color565(160, 200, 220), 1);

            // Center Highlight Box
            int bw = 190;
            int bx = (W - bw) / 2;
            canvas.fillRoundRect(bx, 62, bw, 32, 4, canvas.color565(12, 28, 22));
            canvas.drawRoundRect(bx, 62, bw, 32, 4, canvas.color565(57, 255, 20));

            drawCenteredText(bx, 68, bw, "HOUSTON: WE ARE GO FOR FLIGHT", canvas.color565(57, 255, 20), 1);
            drawCenteredText(bx, 80, bw, "DSKY COMPUTER ONLINE", TFT_WHITE, 1);

            // Subtle Earth / Moon art line
            canvas.drawFastHLine(bx + 10, 104, bw - 20, canvas.color565(30, 60, 80));
            drawCenteredText(0, 110, W, "PRESS ANY KEY TO ENTER DSKY OS", canvas.color565(255, 176, 0), 1);
            drawCenteredText(0, 121, W, "M5STACK CARDPUTER ADV • RK4 3-BODY", canvas.color565(100, 130, 150), 1);

            canvas.pushSprite(0, 0);
            delay(50);
        }

        neopixelWrite(21, 0, 0, 0); // Turn off LED cleanly
    }

private:
    void renderTopStatusBar(const RocketState& r, double missionTime, int timeWarp, bool isDsky, const ColorTheme& theme) {
        canvas.fillRect(0, 0, W, 14, theme.bgPanel);
        canvas.drawFastHLine(0, 13, W, theme.border);
        canvas.setTextSize(1);

        canvas.setTextColor(theme.primary);
        canvas.setCursor(2, 3);
        canvas.print(getPhaseName(r.phase));

        int totalSecs = static_cast<int>(missionTime);
        int hrs = totalSecs / 3600;
        int mins = (totalSecs % 3600) / 60;
        int secs = totalSecs % 60;
        canvas.setTextColor(TFT_WHITE);
        canvas.setCursor(80, 3);
        if (hrs < 100) {
            canvas.printf("T+%02d:%02d:%02d", hrs, mins, secs);
        } else {
            canvas.printf("T+%03d:%02d:%02d", hrs, mins, secs);
        }

        canvas.setTextColor(timeWarp > 1 ? theme.secondary : theme.primary);
        canvas.setCursor(150, 3);
        canvas.printf("x%d", timeWarp);

        canvas.setTextColor(theme.secondary);
        canvas.setCursor(196, 3);
        canvas.print(isDsky ? "[DSKY]" : "[MAP]");
    }

    void drawDskyLamp(int x, int y, const char* text, bool active, uint16_t activeCol) {
        uint16_t bg = active ? activeCol : canvas.color565(18, 24, 20);
        uint16_t fg = active ? TFT_BLACK : canvas.color565(50, 75, 60);
        canvas.fillRect(x, y, 29, 9, bg);
        canvas.setTextColor(fg);
        canvas.setCursor(x + 2, y + 1);
        canvas.print(text);
    }

public:
    static const char* getProgramTitle(AgcProgram p) {
        switch (p) {
            case AgcProgram::P11_EARTH_ORB:  return "LAUNCH LEO";
            case AgcProgram::P15_TLI_BURN:   return "TLI BURN";
            case AgcProgram::P20_COAST_MOON: return "LUNAR COAST";
            case AgcProgram::P21_LOI_BURN:   return "LOI ORBIT";
            case AgcProgram::P30_TEI_BURN:   return "TEI RETURN";
            case AgcProgram::P63_REENTRY:    return "RE-ENTRY";
            default:                         return "IDLE";
        }
    }

    static const char* getPhaseName(MissionPhase ph) {
        switch (ph) {
            case MissionPhase::PRELAUNCH_PAD:       return "PAD: PRELAUNCH";
            case MissionPhase::LAUNCH_ASCENT:       return "ASCENT TO LEO";
            case MissionPhase::LOW_EARTH_ORBIT:     return "EARTH ORBIT";
            case MissionPhase::TRANSLUNAR_COAST:    return "TO THE MOON";
            case MissionPhase::LUNAR_FLYBY_SOI:     return "MOON FLYBY";
            case MissionPhase::LUNAR_ORBIT:         return "LUNAR ORBIT";
            case MissionPhase::TRANSEARTH_COAST:    return "EARTH RETURN";
            case MissionPhase::ATMOSPHERIC_REENTRY: return "RE-ENTRY";
            case MissionPhase::SPLASHDOWN_SUCCESS:  return "SPLASHDOWN!";
            case MissionPhase::MISSION_FAILED:      return "ABORT/LOST";
            default:                                return "APOLLO";
        }
    }
};

} // namespace Apollo
