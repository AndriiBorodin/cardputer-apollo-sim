#pragma once
#include <cmath>
#include <cstdint>

namespace Apollo {

// --- Game / Simulation Modes Selected in Main Menu ---
enum class ControlMode {
    FLIGHT_DIRECTOR = 0, // Houston Mode: Houston guides with GO/EXECUTE decisions via [ENTER]
    SMART_ASSIST    = 1, // Semi-auto: SAS auto-attitude, player selects AGC programs [N] and burns [ENTER]
    AUTONOMOUS_DEMO = 2  // Autonomous demo showcase
};

enum class MissionScenario {
    FULL_MISSION    = 0, // Full Cape Canaveral -> LEO -> Moon -> Splashdown
    AUTONOMOUS_DEMO = 1  // Autonomous demo flight
};

enum class PhysicsScale {
    ARCADE_FAST = 0, // Scaled for fun 3-5 min mission with responsive orbits
    REALISTIC   = 1  // Full Earth-Moon scale with high Time Warp (Apollo 11)
};

enum class ThemePreset : int {
    CRT_GREEN   = 0, // Classic Apollo CRT Phosphor (P1 green)
    AMBER_TERM  = 1, // Retro Amber Terminal
    CYAN_AVIO   = 2, // Neon Cyan Avionics
    RED_ALERT   = 3  // Deep Red / Orange Tactical
};

struct ColorTheme {
    uint16_t primary;     // Main glowing text / borders / reticle
    uint16_t secondary;   // Accent / secondary info
    uint16_t dim;         // Subdued labels / lines
    uint16_t bgPanel;     // Panel background
    uint16_t border;      // Panel borders
    const char* name;

    static ColorTheme get(ThemePreset preset) {
        // RGB565 color definitions
        switch (preset) {
            case ThemePreset::AMBER_TERM:
                return {
                    0xFD80, // Bright Amber (255, 176, 0)
                    0xFE00, // Gold Yellow
                    0x7A40, // Dim Amber
                    0x18A0, // Deep warm dark
                    0x9B40, // Amber border
                    "AMBER TERMINAL"
                };
            case ThemePreset::CYAN_AVIO:
                return {
                    0x07FF, // Bright Cyan (0, 255, 255)
                    0x5EBF, // Soft Blue/Cyan
                    0x0353, // Dim Cyan
                    0x08CD, // Deep navy background
                    0x0477, // Cyan border
                    "CYAN AVIONICS"
                };
            case ThemePreset::RED_ALERT:
                return {
                    0xF9A4, // Bright Orange/Red
                    0xFD00, // Orange
                    0x78A0, // Dim Red
                    0x1862, // Deep maroon background
                    0x8800, // Dark Red border
                    "RED/ORANGE ALERT"
                };
            case ThemePreset::CRT_GREEN:
            default:
                return {
                    0x37E4, // Phosphor Green (55, 255, 70)
                    0x97F2, // Mint / Pale Green
                    0x1B64, // Dim Green
                    0x0922, // Deep CRT green-black
                    0x2386, // Green border
                    "APOLLO CRT GREEN"
                };
        }
    }
};

enum class AppScreen {
    MAIN_MENU = 0,
    DSKY_COMPUTER = 1,
    ORBIT_MAP = 2,
    MISSION_SUMMARY = 3
};

enum class MapCameraFocus {
    SYSTEM_FIT = 0, // Fits both Earth and Moon nicely on the 240x135 screen
    FOLLOW_SHIP = 1,
    CENTER_EARTH = 2,
    CENTER_MOON = 3
};

// --- Apollo AGC Programs ---
enum class AgcProgram : int {
    P00_IDLE       = 0,  // CMC Idling / Pre-launch
    P11_EARTH_ORB  = 11, // Earth Orbit Insertion (Launch & Gravity Turn to LEO)
    P15_TLI_BURN   = 15, // Trans-Lunar Injection Burn (Free-Return Figure-8 setup)
    P20_COAST_MOON = 20, // Coasting to Moon / Lunar Flyby Tracking
    P21_LOI_BURN   = 21, // Optional Lunar Orbit Insertion (Retrograde capture at Moon)
    P30_TEI_BURN   = 30, // Trans-Earth Injection (Escape Moon back to Earth)
    P63_REENTRY    = 63  // Earth Atmospheric Entry & Drogue/Main Chute Splashdown
};

enum class MissionPhase {
    PRELAUNCH_PAD = 0,
    LAUNCH_ASCENT,
    LOW_EARTH_ORBIT,
    TRANSLUNAR_COAST,
    LUNAR_FLYBY_SOI,
    LUNAR_ORBIT,
    TRANSEARTH_COAST,
    ATMOSPHERIC_REENTRY,
    SPLASHDOWN_SUCCESS,
    MISSION_FAILED
};

struct SimConfig {
    ControlMode controlMode = ControlMode::FLIGHT_DIRECTOR;
    MissionScenario scenario = MissionScenario::FULL_MISSION;
    PhysicsScale physicsScale = PhysicsScale::ARCADE_FAST;
    bool soundEnabled = true;
    ThemePreset themePreset = ThemePreset::CRT_GREEN;

    // Scaled physical constants populated on mission start
    double muEarth;
    double muMoon;
    double earthRadius;
    double moonRadius;
    double moonDistance;
    double moonAngularVel;
    double atmosphereHeight;
    double scaleHeight;
    double moonSOI;

    void applyScale() {
        if (physicsScale == PhysicsScale::REALISTIC) {
            muEarth          = 3.986004418e14;
            muMoon           = 4.9048695e12;
            earthRadius      = 6.371e6;       // 6,371 km
            moonRadius       = 1.7374e6;      // 1,737 km
            moonDistance     = 384.4e6;       // 384,400 km
            moonAngularVel   = 2.6617e-6;     // rad/s (~27.3 days orbital period)
            atmosphereHeight = 65000.0;       // 65 km
            scaleHeight      = 7500.0;
            moonSOI          = 66100e3;       // 66,100 km
        } else {
            // ARCADE_FAST: Compact scale where Earth-Moon ratio is visually clear & fast to fly
            // Earth R = 600 km, Moon R = 180 km, Distance = 12,000 km
            muEarth          = 3.5e12;
            muMoon           = 1.8e11;
            earthRadius      = 600000.0;      // 600 km
            moonRadius       = 180000.0;      // 180 km
            moonDistance     = 12000000.0;    // 12,000 km
            moonAngularVel   = 2.5e-5;
            atmosphereHeight = 65000.0;       // 65 km
            scaleHeight      = 7000.0;
            moonSOI          = 2400000.0;     // 2,400 km
        }
    }
};

struct Vector2D {
    double x = 0.0;
    double y = 0.0;

    Vector2D() = default;
    Vector2D(double _x, double _y) : x(_x), y(_y) {}

    Vector2D operator+(const Vector2D& o) const { return {x + o.x, y + o.y}; }
    Vector2D operator-(const Vector2D& o) const { return {x - o.x, y - o.y}; }
    Vector2D operator*(double s) const { return {x * s, y * s}; }
    Vector2D operator/(double s) const { return {x / s, y / s}; }

    double magnitudeSq() const { return x * x + y * y; }
    double magnitude() const { return std::sqrt(magnitudeSq()); }
    double dot(const Vector2D& o) const { return x * o.x + y * o.y; }

    Vector2D normalized() const {
        double m = magnitude();
        return (m > 1e-9) ? Vector2D(x / m, y / m) : Vector2D(0, 0);
    }
};

struct RocketState {
    Vector2D pos;
    Vector2D vel;
    double angleRad = M_PI / 2.0; // Ship nose orientation
    double targetAngleRad = M_PI / 2.0;
    double throttle = 0.0;

    int stage = 0; // 0: S-IC, 1: S-II, 2: S-IVB/CSM, 3: Command Module (Re-entry)
    double fuelKg = 1000.0;
    double maxFuelKg = 1000.0;
    double dryMassKg = 200.0;
    double maxThrustN = 50000.0;
    double isp = 300.0;

    double gForce = 1.0;
    double heatShieldTemp = 290.0; // Kelvin
    double deltaVRemaining = 0.0;

    bool onPad = true;
    bool crashed = false;
    bool splashedDown = false;
    bool autoBurnActive = false;
    double burnTargetDv = 0.0;
    double burnRemainingDv = 0.0;

    bool isDemoFlight = false;
    uint32_t demoNoticeTimer = 0; // Millis when notice should expire
    MissionScenario scenario = MissionScenario::FULL_MISSION;

    // Flight Director guidance & prompt state
    char actionPrompt[32] = "";
    char failureReason[32] = "";
    double tliCountdown = -1.0; // Seconds until TLI ignition window
    double lunarOrbitTimer = 0.0; // Seconds in Lunar Orbit
    bool teiWindowOpen = false;  // True when optimal retrograde lunar departure window is reached
    bool promptBlink = false;

    MissionPhase phase = MissionPhase::PRELAUNCH_PAD;
    AgcProgram activeProg = AgcProgram::P00_IDLE;
    AgcProgram selectedProg = AgcProgram::P11_EARTH_ORB;
    int verb = 6;
    int noun = 36;

    double totalMass() const { return dryMassKg + fuelKg; }
};

constexpr int TRAJ_POINTS = 180;

struct TrajectoryForecast {
    Vector2D pts[TRAJ_POINTS];
    int count = 0;

    double apoapsisAlt = 0.0;
    double periapsisAlt = 0.0;
    Vector2D apoapsisPos;
    Vector2D periapsisPos;

    double closestMoonAlt = 1e12;
    Vector2D closestMoonPos;
    bool entersMoonSOI = false;
    bool freeReturnDetected = false;
    bool impactsEarth = false;
    bool impactsMoon = false;
};

} // namespace Apollo
