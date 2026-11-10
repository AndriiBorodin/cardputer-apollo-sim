#pragma once
#include "ApolloTypes.h"
#include "AudioSys.h"
#include <cmath>
#include <algorithm>

namespace Apollo {

class OrbitalPhysics {
public:
    static constexpr double TARGET_LEO_ALT = 85000.0; // 85 km circular parking orbit

    static double getInitialMoonPhase(const SimConfig& cfg) {
        return (cfg.physicsScale == PhysicsScale::REALISTIC) ? 4.33 : 4.23;
    }

    static double getOptimalTliLead(const SimConfig& cfg) {
        return (cfg.physicsScale == PhysicsScale::REALISTIC) ? 2.308 : 2.420;
    }

    static Vector2D getMoonPos(const SimConfig& cfg, double missionTime) {
        double initialPhase = getInitialMoonPhase(cfg);
        double angle = initialPhase + cfg.moonAngularVel * missionTime;
        return Vector2D(std::cos(angle) * cfg.moonDistance, std::sin(angle) * cfg.moonDistance);
    }

    static Vector2D getMoonVel(const SimConfig& cfg, double missionTime) {
        double initialPhase = getInitialMoonPhase(cfg);
        double angle = initialPhase + cfg.moonAngularVel * missionTime;
        double vMag = cfg.moonDistance * cfg.moonAngularVel;
        return Vector2D(-std::sin(angle) * vMag, std::cos(angle) * vMag);
    }

    static void liftoffFromPad(RocketState& r, const SimConfig& cfg) {
        if (!r.onPad) return;
        r.onPad = false;
        r.phase = MissionPhase::LAUNCH_ASCENT;
        r.activeProg = AgcProgram::P11_EARTH_ORB;
        r.selectedProg = AgcProgram::P11_EARTH_ORB;
        double surfaceRotSpeed = (cfg.physicsScale == PhysicsScale::REALISTIC) ? 465.0 : 80.0;
        Vector2D upE = r.pos.normalized();
        Vector2D eastE(-upE.y, upE.x);
        r.pos = upE * (cfg.earthRadius + 15.0); // Clear pad elevation
        r.vel = eastE * surfaceRotSpeed + upE * 15.0; // Positive vertical liftoff velocity
        r.throttle = 1.0;
        r.autoBurnActive = true;
        r.angleRad = std::atan2(upE.y, upE.x);
        r.targetAngleRad = r.angleRad;
    }

    static void initRocket(RocketState& r, const SimConfig& cfg) {
        initScenario(r, cfg, MissionScenario::AUTONOMOUS_DEMO);
    }

    static void initScenario(RocketState& r, const SimConfig& cfg, MissionScenario scenario = MissionScenario::AUTONOMOUS_DEMO) {
        (void)scenario;
        r = RocketState{};
        r.scenario = scenario;
        r.isDemoFlight = (cfg.controlMode == ControlMode::AUTONOMOUS_DEMO);

        // Standard Pad launch at Cape Canaveral (top of Earth)
        r.pos = Vector2D(0.0, cfg.earthRadius + 2.0);
        r.vel = Vector2D(0.0, 0.0); // Surface-relative 0 m/s while sitting on pad
        r.angleRad = M_PI / 2.0;    // Pointing straight up from pad
        r.targetAngleRad = M_PI / 2.0;
        r.throttle = 0.0;
        r.stage = 0;
        r.onPad = true;
        r.crashed = false;
        r.splashedDown = false;
        r.phase = MissionPhase::PRELAUNCH_PAD;
        r.activeProg = AgcProgram::P00_IDLE;
        r.selectedProg = AgcProgram::P11_EARTH_ORB;
        r.verb = 6;
        r.noun = 36;
        if (cfg.controlMode == ControlMode::FLIGHT_DIRECTOR) {
            snprintf(r.actionPrompt, sizeof(r.actionPrompt), "[ENTER] GO FOR LAUNCH");
        } else if (cfg.controlMode == ControlMode::SMART_ASSIST) {
            snprintf(r.actionPrompt, sizeof(r.actionPrompt), "[ENTER] IGNITION (P11)");
        } else {
            snprintf(r.actionPrompt, sizeof(r.actionPrompt), "DEMO: P11 LIFTOFF");
        }
        loadStageSpecs(r, cfg, 0);
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

    static void loadStageSpecs(RocketState& r, const SimConfig& cfg, int stageIdx) {
        r.stage = stageIdx;
        if (cfg.physicsScale == PhysicsScale::REALISTIC) {
            // Authentic Saturn V specifications calibrated for real 1:1 scale (384,400 km)
            if (stageIdx == 0) { // STG 1: S-IC First Stage (0 -> ~68 km)
                r.dryMassKg = 130000.0;
                r.fuelKg = r.maxFuelKg = 2150000.0;
                r.maxThrustN = 36000000.0;
                r.isp = 285.0;
            } else if (stageIdx == 1) { // STG 2: S-II Second Stage (~68 km -> ~85 km sub-orbital flameout)
                r.dryMassKg = 40000.0;
                r.fuelKg = r.maxFuelKg = 380000.0; // Calibrated flameout at ~6,870 m/s for manual [SPACE] prompt
                r.maxThrustN = 5200000.0;
                r.isp = 425.0;
            } else if (stageIdx == 2) { // STG 3: S-IVB + CSM (Orbit insertion + TLI, LOI, TEI)
                r.dryMassKg = 14000.0;
                r.fuelKg = r.maxFuelKg = 120000.0;
                r.maxThrustN = 1100000.0;
                r.isp = 435.0;
            } else { // STG 4: Command Module Capsule (CM)
                r.dryMassKg = 5800.0;
                r.fuelKg = r.maxFuelKg = 2000.0;
                r.maxThrustN = 98000.0;
                r.isp = 315.0;
            }
        } else {
            // ARCADE_FAST: Balanced 4-stage progression for 12,000 km scale
            if (stageIdx == 0) { // STG 1: S-IC First Stage (0 -> ~22 km)
                r.dryMassKg = 52000.0;
                r.fuelKg = r.maxFuelKg = 42000.0;
                r.maxThrustN = 2600000.0;
                r.isp = 280.0;
            } else if (stageIdx == 1) { // STG 2: S-II Second Stage (~22 km -> ~65 km)
                r.dryMassKg = 14000.0;
                r.fuelKg = r.maxFuelKg = 13500.0; // Burns to ~65 km, requiring manual [SPACE] to separate STG 2
                r.maxThrustN = 1100000.0;
                r.isp = 340.0;
            } else if (stageIdx == 2) { // STG 3: S-IVB + CSM (TLI, LOI, TEI)
                r.dryMassKg = 2200.0;
                r.fuelKg = r.maxFuelKg = 16000.0;
                r.maxThrustN = 220000.0;
                r.isp = 420.0;
            } else { // STG 4: Command Module Capsule (Re-entry & Chutes)
                r.dryMassKg = 1000.0;
                r.fuelKg = r.maxFuelKg = 100.0; // RCS only
                r.maxThrustN = 0.0; // Aerobraking & Chutes
                r.isp = 300.0;
            }
        }
        updateDeltaV(r);
    }

    static void updateDeltaV(RocketState& r) {
        double mTotal = r.totalMass();
        if (mTotal > r.dryMassKg && r.dryMassKg > 0.0) {
            r.deltaVRemaining = r.isp * 9.80665 * std::log(mTotal / r.dryMassKg);
        } else {
            r.deltaVRemaining = 0.0;
        }
    }

    static void triggerStage(RocketState& r, const SimConfig& cfg) {
        if (r.stage < 3) {
            bool keepThrust = (r.phase == MissionPhase::LAUNCH_ASCENT) ||
                              (r.autoBurnActive &&
                               r.activeProg != AgcProgram::P20_COAST_MOON &&
                               r.activeProg != AgcProgram::P63_REENTRY);
            loadStageSpecs(r, cfg, r.stage + 1);
            if (keepThrust && r.stage <= 2) {
                r.throttle = 1.0;
            }
            AudioSys::playStagingClick(cfg.soundEnabled);
        }
    }

    static Vector2D computeAcceleration(const SimConfig& cfg,
                                        const Vector2D& pos,
                                        const Vector2D& vel,
                                        const Vector2D& moonPos,
                                        double thrustN,
                                        double angleRad,
                                        double totalMass,
                                        bool lunarOrbitStabilize = false) {
        // 1. Earth gravity (when in stable Lunar Orbit, match Moon's kinematic orbital acceleration to eliminate artificial tidal drift)
        double rE_sq = pos.magnitudeSq();
        double rE = std::sqrt(rE_sq);
        Vector2D aEarth(0, 0);
        if (lunarOrbitStabilize) {
            aEarth = moonPos * (-(cfg.moonAngularVel * cfg.moonAngularVel));
        } else if (rE > 10.0) {
            aEarth = pos * (-cfg.muEarth / (rE_sq * rE));
        }

        // 2. Moon gravity
        Vector2D relMoon = pos - moonPos;
        double rM_sq = relMoon.magnitudeSq();
        double rM = std::sqrt(rM_sq);
        Vector2D aMoon(0, 0);
        if (rM > 10.0) {
            aMoon = relMoon * (-cfg.muMoon / (rM_sq * rM));
        }

        // 3. Engine Thrust
        Vector2D aThrust(0, 0);
        if (totalMass > 0.0 && thrustN > 0.0) {
            double mag = thrustN / totalMass;
            aThrust = Vector2D(std::cos(angleRad) * mag, std::sin(angleRad) * mag);
        }

        // 4. Atmospheric Drag
        Vector2D aDrag(0, 0);
        double altE = rE - cfg.earthRadius;
        if (altE >= 0.0 && altE < cfg.atmosphereHeight) {
            double rho = 1.225 * std::exp(-altE / cfg.scaleHeight);
            double vSq = vel.magnitudeSq();
            if (vSq > 0.1 && totalMass > 0.0) {
                double dragArea = (cfg.physicsScale == PhysicsScale::REALISTIC) ? 12.0 : 2.5;
                double fDrag = 0.5 * rho * vSq * dragArea;
                // Clamp drag deceleration to physically realistic limit (avoid numerical instability)
                double dragDecel = std::min(250.0, fDrag / totalMass);
                aDrag = vel.normalized() * (-dragDecel);
            }
        }

        return aEarth + aMoon + aThrust + aDrag;
    }

    // Closed-loop vector gravity turn guidance (avoids +/-pi atan2 branch-cut glitches)
    static double computeAscentGuidanceAngle(const RocketState& r, const SimConfig& cfg) {
        double rE = r.pos.magnitude();
        double altE = rE - cfg.earthRadius;
        Vector2D upE = r.pos.normalized();
        Vector2D eastE(-upE.y, upE.x);

        if (altE < 2500.0) {
            return std::atan2(upE.y, upE.x);
        }

        double vRad = r.vel.dot(upE);
        double vTan = r.vel.dot(eastE);
        double m = std::max(100.0, r.totalMass());
        double acc = std::max(1.0, r.maxThrustN / m);
        double gEff = (cfg.muEarth / (rE * rE)) - (vTan * vTan / rE);

        double maxClimb = (cfg.physicsScale == PhysicsScale::REALISTIC) ? 680.0 : 1400.0;
        double kP       = (cfg.physicsScale == PhysicsScale::REALISTIC) ? 0.026 : 0.062;

        double desVrad = std::max(-100.0, std::min(maxClimb, (TARGET_LEO_ALT - altE) * kP));
        double reqArad = gEff + (desVrad - vRad) * 1.5;
        double sinP = std::max(-0.85, std::min(0.96, reqArad / acc));
        double pitchFromHoriz = std::asin(sinP);

        Vector2D dir = upE * std::sin(pitchFromHoriz) + eastE * std::cos(pitchFromHoriz);
        return std::atan2(dir.y, dir.x);
    }

    static void updateVerbNoun(RocketState& r) {
        if (r.onPad) {
            r.verb = 6;
            r.noun = 36; // Standby clock
            return;
        }
        switch (r.activeProg) {
            case AgcProgram::P11_EARTH_ORB:
                r.verb = 16;
                r.noun = (r.phase == MissionPhase::LAUNCH_ASCENT) ? 62 : 44;
                break;
            case AgcProgram::P15_TLI_BURN:
            case AgcProgram::P21_LOI_BURN:
            case AgcProgram::P30_TEI_BURN:
                r.verb = 16;
                r.noun = 40; // Burn velocity / remaining Delta-V
                break;
            case AgcProgram::P20_COAST_MOON:
                r.verb = 16;
                r.noun = 44; // Apocenter / Pericenter / SOI navigation
                break;
            case AgcProgram::P63_REENTRY:
                r.verb = 16;
                r.noun = 68; // Re-entry deceleration & chutes
                break;
            default:
                r.verb = 16;
                r.noun = 36;
                break;
        }
    }

    // Ensure clean atmospheric entry corridor (periapsis ~ 28 km) once on Transearth Coast outside Moon SOI
    // Targets a safe prograde entry corridor curving to the right of Earth for Demo, Houston, and Semi-Auto modes.
    static void alignEarthReturnCorridor(RocketState& r, const SimConfig& cfg) {
        double rE = r.pos.magnitude();
        if (rE <= cfg.earthRadius + 150000.0) return; // Inside 150 km, preserve natural orbital entry mechanics
        double vSq = r.vel.magnitudeSq();
        double specE = 0.5 * vSq - cfg.muEarth / rE;
        double hSigned = r.pos.x * r.vel.y - r.pos.y * r.vel.x;
        double h = std::abs(hSigned);
        double rp = rE;
        if (std::abs(specE) > 1e-3) {
            double a = -cfg.muEarth / (2.0 * specE);
            double eccSq = std::max(0.0, 1.0 + (2.0 * specE * h * h) / (cfg.muEarth * cfg.muEarth));
            rp = a * (1.0 - std::sqrt(eccSq));
        }
        Vector2D upE = r.pos.normalized();
        Vector2D eastE(-upE.y, upE.x);
        double vRad = r.vel.dot(upE);
        double minSafeRp = cfg.earthRadius + 18000.0;
        double maxSafeRp = cfg.earthRadius + 38000.0;
        if (rp < minSafeRp || rp > maxSafeRp || vRad > 20.0 || hSigned < 0.0) {
            double targetRp = cfg.earthRadius + 28000.0;
            double rAp = (cfg.physicsScale == PhysicsScale::REALISTIC)
                ? (cfg.moonDistance * 1.85) // Authentic Apollo 11 hyperbolic departure: 62.7 hours return transit time
                : (cfg.moonDistance * 1.35);
            double aTrans = (rAp + targetRp) * 0.5;
            double eSpecReq = -cfg.muEarth / (2.0 * aTrans);
            double vSqReq = 2.0 * (eSpecReq + cfg.muEarth / rE);
            if (vSqReq > 10.0) {
                double vReq = std::sqrt(vSqReq);
                double vp = std::sqrt(2.0 * (eSpecReq + cfg.muEarth / targetRp));
                double hReq = targetRp * vp;
                double vTanReq = std::min(vReq * 0.999, hReq / rE);
                double vRadReq = -std::sqrt(std::max(0.0, vReq * vReq - vTanReq * vTanReq));
                r.vel = upE * vRadReq + eastE * vTanReq;
            }
        }
    }

    static bool isProgramValidForPhase(AgcProgram prog, MissionPhase phase) {
        switch (prog) {
            case AgcProgram::P00_IDLE:
                return true;
            case AgcProgram::P11_EARTH_ORB:
                return (phase == MissionPhase::PRELAUNCH_PAD || phase == MissionPhase::LAUNCH_ASCENT);
            case AgcProgram::P15_TLI_BURN:
                return (phase == MissionPhase::LOW_EARTH_ORBIT);
            case AgcProgram::P20_COAST_MOON:
                return (phase == MissionPhase::TRANSLUNAR_COAST ||
                        phase == MissionPhase::LUNAR_FLYBY_SOI ||
                        phase == MissionPhase::TRANSEARTH_COAST);
            case AgcProgram::P21_LOI_BURN:
                return (phase == MissionPhase::LUNAR_FLYBY_SOI || phase == MissionPhase::LUNAR_ORBIT);
            case AgcProgram::P30_TEI_BURN:
                return (phase == MissionPhase::LUNAR_ORBIT);
            case AgcProgram::P63_REENTRY:
                return (phase == MissionPhase::TRANSEARTH_COAST ||
                        phase == MissionPhase::ATMOSPHERIC_REENTRY ||
                        phase == MissionPhase::LOW_EARTH_ORBIT); // Emergency de-orbit from LEO
            default:
                return false;
        }
    }

    // Activate selected AGC Program
    static void executeAgcProgram(RocketState& r, const SimConfig& cfg, AgcProgram prog) {
        r.selectedProg = prog;
        executeAgcProgram(r, cfg);
    }

    static void executeAgcProgram(RocketState& r, const SimConfig& cfg) {
        // Interlock: Prevent running invalid programs outside their legitimate mission phase
        if (!r.isDemoFlight && !isProgramValidForPhase(r.selectedProg, r.phase)) {
            AudioSys::playMasterAlarm(cfg.soundEnabled);
            snprintf(r.actionPrompt, sizeof(r.actionPrompt), "OPR ERR: INVALID PHASE");
            return;
        }

        r.activeProg = r.selectedProg;
        AudioSys::playQuindarTone(cfg.soundEnabled);

        switch (r.activeProg) {
            case AgcProgram::P11_EARTH_ORB:
                if (r.onPad) {
                    liftoffFromPad(r, cfg);
                }
                r.throttle = 1.0;
                r.autoBurnActive = true;
                break;

            case AgcProgram::P15_TLI_BURN: {
                if (cfg.controlMode != ControlMode::SMART_ASSIST && r.stage < 2) {
                    loadStageSpecs(r, cfg, 2); // Ensure S-IVB (STG 3) is active for TLI in Houston/Demo
                }
                double currentSpd = r.vel.magnitude();
                if (cfg.physicsScale == PhysicsScale::REALISTIC) {
                    r.burnTargetDv = 3189.0;
                } else {
                    // Vis-Viva transfer formula to Moon distance (with 2% margin to penetrate deep into Moon SOI)
                    double rLeo = cfg.earthRadius + TARGET_LEO_ALT;
                    double raTarget = cfg.moonDistance * 1.02;
                    double aTrans = (rLeo + raTarget) * 0.5;
                    double targetSpd = std::sqrt(2.0 * cfg.muEarth * (1.0 / rLeo - 1.0 / (2.0 * aTrans))) + 8.0;
                    r.burnTargetDv = std::max(50.0, targetSpd - currentSpd);
                }
                r.burnRemainingDv = r.burnTargetDv;
                r.autoBurnActive = true;
                r.throttle = 1.0;
                break;
            }

            case AgcProgram::P20_COAST_MOON:
                r.autoBurnActive = false;
                r.throttle = 0.0;
                break;

            case AgcProgram::P21_LOI_BURN: {
                if (r.stage < 2) {
                    loadStageSpecs(r, cfg, 2);
                }
                r.burnTargetDv = (cfg.physicsScale == PhysicsScale::REALISTIC) ? 920.0 : 320.0;
                r.burnRemainingDv = r.burnTargetDv;
                r.autoBurnActive = true;
                r.throttle = 1.0;
                break;
            }

            case AgcProgram::P30_TEI_BURN: {
                if (r.stage < 2) {
                    loadStageSpecs(r, cfg, 2);
                }
                r.burnTargetDv = (cfg.physicsScale == PhysicsScale::REALISTIC) ? 900.0 : 420.0;
                r.burnRemainingDv = r.burnTargetDv;
                r.autoBurnActive = true;
                r.throttle = 1.0;
                break;
            }

            case AgcProgram::P63_REENTRY: {
                r.autoBurnActive = false;
                r.throttle = 0.0;
                if (r.stage < 3) {
                    loadStageSpecs(r, cfg, 3); // Jettison SM -> Command Module Capsule (STG 4)
                }
                break;
            }

            default:
                break;
        }
        updateVerbNoun(r);
    }

    // Unified Autonomous Mission Director (P11 -> P63 Demo Flight)
    static void updateFlightDirector(RocketState& r, const SimConfig& cfg, double missionTime, double dt) {
        double rE = r.pos.magnitude();
        double altE = rE - cfg.earthRadius;
        Vector2D moonPos = getMoonPos(cfg, missionTime);
        Vector2D moonVel = getMoonVel(cfg, missionTime);
        double distM = (r.pos - moonPos).magnitude();
        Vector2D relPosM = r.pos - moonPos;
        Vector2D relVelM = r.vel - moonVel;
        double vRadM = (distM > 10.0) ? relVelM.dot(relPosM * (1.0 / distM)) : 0.0;

        Vector2D upE = r.pos.normalized();
        Vector2D eastE(-upE.y, upE.x);
        double vTan = r.vel.dot(eastE);
        double vCircLeo = std::sqrt(cfg.muEarth / (cfg.earthRadius + TARGET_LEO_ALT));

        // Zero fuel cutoff failsafe
        if (r.fuelKg <= 0.0 && !r.onPad) {
            r.throttle = 0.0;
            r.autoBurnActive = false;
        }

        // 1. Pad status: lock vertical & launch
        if (r.onPad) {
            double upAngle = std::atan2(upE.y, upE.x);
            r.angleRad = upAngle;
            r.targetAngleRad = upAngle;
            if (r.isDemoFlight) {
                r.throttle = 1.0;
                r.selectedProg = AgcProgram::P11_EARTH_ORB;
                executeAgcProgram(r, cfg);
                snprintf(r.actionPrompt, sizeof(r.actionPrompt), "DEMO: P11 LIFTOFF");
            } else if (cfg.controlMode == ControlMode::FLIGHT_DIRECTOR) {
                r.throttle = 0.0;
                r.selectedProg = AgcProgram::P11_EARTH_ORB;
                snprintf(r.actionPrompt, sizeof(r.actionPrompt), "[ENTER] GO FOR LAUNCH");
            } else {
                r.throttle = 0.0;
                if (r.selectedProg == AgcProgram::P11_EARTH_ORB) {
                    snprintf(r.actionPrompt, sizeof(r.actionPrompt), "[ENTER] IGNITION (P11)");
                } else {
                    snprintf(r.actionPrompt, sizeof(r.actionPrompt), "SELECT P11 (KEY N)");
                }
            }
            return;
        }

        // 2. Ascent phase: closed-loop gravity turn & sequential staging (STG 1 -> STG 2 -> STG 3)
        if (r.phase == MissionPhase::LAUNCH_ASCENT) {
            r.angleRad = computeAscentGuidanceAngle(r, cfg);
            r.targetAngleRad = r.angleRad;

            // Sequential Saturn V Staging: S-IC -> S-II, or S-II -> S-IVB
            // In Houston/Demo: LVDC automatically stages upon fuel burnout
            // In Semi-Auto: Engine flames out (0% thrust), flashing orange LED (3 Hz), prompt [SPACE] STAGE 1/2 SEP.
            // The pilot MUST press [SPACE] to separate and fire the next stage!
            if (r.fuelKg <= 1.0 && r.stage < 2) {
                if (cfg.controlMode == ControlMode::SMART_ASSIST) {
                    r.throttle = 0.0; // Flameout
                    if (r.stage == 0) {
                        snprintf(r.actionPrompt, sizeof(r.actionPrompt), "[SPACE] STAGE 1 SEP");
                    } else {
                        snprintf(r.actionPrompt, sizeof(r.actionPrompt), "[SPACE] STAGE 2 SEP");
                    }
                } else {
                    triggerStage(r, cfg);
                }
            } else if (cfg.controlMode != ControlMode::SMART_ASSIST && r.throttle < 0.99 && r.fuelKg > 1.0) {
                r.throttle = 1.0;
            }

            // Insertion into circular 85 km Earth Parking Orbit
            // In Semi-Auto mode: must be on Stage 2 (S-IVB); lower stages must flame out and require manual [SPACE]
            bool stageReady = (cfg.controlMode != ControlMode::SMART_ASSIST || r.stage >= 2);
            if (stageReady && altE >= TARGET_LEO_ALT - 3000.0 && vTan >= vCircLeo * 0.97) {
                r.throttle = 0.0;
                r.autoBurnActive = false;
                r.phase = MissionPhase::LOW_EARTH_ORBIT;
                r.activeProg = AgcProgram::P11_EARTH_ORB;
                if (cfg.controlMode != ControlMode::SMART_ASSIST) {
                    r.selectedProg = AgcProgram::P15_TLI_BURN;
                    if (r.stage < 2) {
                        loadStageSpecs(r, cfg, 2); // Ready S-IVB (STG 3) for TLI in Houston/Demo
                    }
                }
                r.pos = upE * (cfg.earthRadius + TARGET_LEO_ALT);
                r.vel = eastE * vCircLeo;
                r.angleRad = std::atan2(r.vel.y, r.vel.x);
                r.targetAngleRad = r.angleRad;
                AudioSys::playQuindarTone(cfg.soundEnabled);
            } else if (r.fuelKg > 1.0) {
                snprintf(r.actionPrompt, sizeof(r.actionPrompt), "ASCENT: STG%d (%dkm)", r.stage + 1, static_cast<int>(altE / 1000.0));
            }
            updateVerbNoun(r);
            return;
        }

        // 3. Low Earth Parking Orbit (LEO): Wait for TLI Window with live countdown
        if (r.phase == MissionPhase::LOW_EARTH_ORBIT) {
            r.angleRad = std::atan2(r.vel.y, r.vel.x);
            r.targetAngleRad = r.angleRad;
            double shipAng = std::atan2(r.pos.y, r.pos.x);
            double moonAng = std::atan2(moonPos.y, moonPos.x);
            double lead = moonAng - shipAng;
            while (lead < 0) lead += 2.0 * M_PI;
            while (lead >= 2.0 * M_PI) lead -= 2.0 * M_PI;

            double targetLead = getOptimalTliLead(cfg);
            double diff = lead - targetLead;
            while (diff < -0.02) diff += 2.0 * M_PI;
            double rLeo = r.pos.magnitude();
            double omegaShip = std::sqrt(cfg.muEarth / (rLeo * rLeo * rLeo));
            double omegaRel = std::max(1e-6, omegaShip - cfg.moonAngularVel);
            r.tliCountdown = std::max(0.0, diff / omegaRel);

            if (!r.autoBurnActive && r.activeProg != AgcProgram::P15_TLI_BURN) {
                if (r.isDemoFlight) {
                    if (diff <= 0.008 || (r.tliCountdown <= 0.4 && diff < 0.05)) {
                        r.selectedProg = AgcProgram::P15_TLI_BURN;
                        executeAgcProgram(r, cfg);
                    } else {
                        int cdM = static_cast<int>(r.tliCountdown / 60);
                        int cdS = static_cast<int>(r.tliCountdown) % 60;
                        snprintf(r.actionPrompt, sizeof(r.actionPrompt), "TLI IN %02d:%02d", cdM, cdS);
                    }
                } else if (cfg.controlMode == ControlMode::FLIGHT_DIRECTOR) {
                    if (r.tliCountdown <= 15.0) {
                        snprintf(r.actionPrompt, sizeof(r.actionPrompt), "[ENTER] GO FOR TLI BURN");
                    } else {
                        int cdM = static_cast<int>(r.tliCountdown / 60);
                        int cdS = static_cast<int>(r.tliCountdown) % 60;
                        snprintf(r.actionPrompt, sizeof(r.actionPrompt), "TLI IN %02d:%02d", cdM, cdS);
                    }
                } else { // SMART_ASSIST
                    int cdM = static_cast<int>(r.tliCountdown / 60);
                    int cdS = static_cast<int>(r.tliCountdown) % 60;
                    if (r.tliCountdown <= 15.0) {
                        if (r.selectedProg == AgcProgram::P15_TLI_BURN) {
                            snprintf(r.actionPrompt, sizeof(r.actionPrompt), "TLI WIN OPEN: [ENTER]");
                        } else {
                            snprintf(r.actionPrompt, sizeof(r.actionPrompt), "TLI WIN: SELECT P15 (N)");
                        }
                    } else {
                        snprintf(r.actionPrompt, sizeof(r.actionPrompt), "LEO: TLI T-%02d:%02d", cdM, cdS);
                    }
                }
            }
            // If TLI burn is active, execute burn
            if (r.autoBurnActive && r.activeProg == AgcProgram::P15_TLI_BURN) {
                r.throttle = 1.0;
                double accel = (r.fuelKg > 0.0) ? ((r.maxThrustN * r.throttle) / r.totalMass()) : 0.0;
                r.burnRemainingDv -= accel * dt;
                snprintf(r.actionPrompt, sizeof(r.actionPrompt), "TLI BURN IN PROGRESS");
                if (r.burnRemainingDv <= 0.0) {
                    r.burnRemainingDv = 0.0;
                    r.throttle = 0.0;
                    r.autoBurnActive = false;
                    r.phase = MissionPhase::TRANSLUNAR_COAST;
                    r.activeProg = AgcProgram::P20_COAST_MOON;
                    r.selectedProg = (cfg.controlMode == ControlMode::FLIGHT_DIRECTOR || r.isDemoFlight)
                        ? AgcProgram::P21_LOI_BURN : AgcProgram::P20_COAST_MOON;
                    r.tliCountdown = -1.0;
                    AudioSys::playQuindarTone(cfg.soundEnabled);
                }
            }
            updateVerbNoun(r);
            return;
        }

        // 4. Translunar Coast: cruise towards Moon
        if (r.phase == MissionPhase::TRANSLUNAR_COAST) {
            r.angleRad = std::atan2(r.vel.y, r.vel.x);
            r.targetAngleRad = r.angleRad;
            r.tliCountdown = -1.0;
            r.throttle = 0.0;
            r.activeProg = AgcProgram::P20_COAST_MOON;
            if (cfg.controlMode != ControlMode::SMART_ASSIST) {
                r.selectedProg = AgcProgram::P21_LOI_BURN;
                snprintf(r.actionPrompt, sizeof(r.actionPrompt), "P20: TRANSLUNAR COAST");
            } else {
                if (r.selectedProg == AgcProgram::P20_COAST_MOON) {
                    snprintf(r.actionPrompt, sizeof(r.actionPrompt), "P20: TRANSLUNAR COAST");
                } else if (r.selectedProg == AgcProgram::P21_LOI_BURN) {
                    snprintf(r.actionPrompt, sizeof(r.actionPrompt), "COAST: P21 ARMED");
                } else {
                    snprintf(r.actionPrompt, sizeof(r.actionPrompt), "COAST: SEL P21 (N)");
                }
            }

            if (distM < cfg.moonSOI) {
                r.phase = MissionPhase::LUNAR_FLYBY_SOI;
                AudioSys::playQuindarTone(cfg.soundEnabled);
            }
            updateVerbNoun(r);
            return;
        }

        // 5. Lunar Approach inside Moon SOI: detect Perilune and trigger P21 LOI
        if (r.phase == MissionPhase::LUNAR_FLYBY_SOI) {
            if (!r.autoBurnActive) {
                r.angleRad = std::atan2(r.vel.y, r.vel.x);
                r.targetAngleRad = r.angleRad;
                r.throttle = 0.0;
                r.activeProg = AgcProgram::P20_COAST_MOON;
                if (cfg.controlMode != ControlMode::SMART_ASSIST) {
                    r.selectedProg = AgcProgram::P21_LOI_BURN;
                }

                // Perilune reached when radial velocity relative to Moon becomes >= -20 m/s inside Moon SOI
                bool atPerilune = (vRadM >= -20.0 && distM <= cfg.moonSOI * 0.85);
                if (r.isDemoFlight) {
                    if (atPerilune) {
                        r.selectedProg = AgcProgram::P21_LOI_BURN;
                        executeAgcProgram(r, cfg);
                    } else {
                        snprintf(r.actionPrompt, sizeof(r.actionPrompt), "P20: APPROACHING MOON");
                    }
                } else if (cfg.controlMode == ControlMode::FLIGHT_DIRECTOR) {
                    if (atPerilune) {
                        snprintf(r.actionPrompt, sizeof(r.actionPrompt), "[ENTER] GO FOR LOI BURN");
                    } else {
                        snprintf(r.actionPrompt, sizeof(r.actionPrompt), "P20: APPROACHING MOON");
                    }
                } else { // SMART_ASSIST
                    if (atPerilune) {
                        if (r.selectedProg == AgcProgram::P21_LOI_BURN) {
                            snprintf(r.actionPrompt, sizeof(r.actionPrompt), "PERILUNE: [ENTER] LOI");
                        } else {
                            snprintf(r.actionPrompt, sizeof(r.actionPrompt), "PERILUNE! SELECT P21 (N)");
                        }
                    } else {
                        double periDistKm = std::max(0.0, (distM - cfg.moonRadius) / 1000.0);
                        snprintf(r.actionPrompt, sizeof(r.actionPrompt), "PERILUNE DIST: %.0f KM", periDistKm);
                    }
                }
            } else if (r.activeProg == AgcProgram::P21_LOI_BURN) {
                // Dynamically steer toward exact circular Lunar Orbit velocity vector
                double dM = std::max(cfg.moonRadius + 40000.0, relPosM.magnitude());
                Vector2D upM = relPosM.normalized();
                Vector2D ccwM(-upM.y, upM.x);
                Vector2D tanM = (relVelM.dot(ccwM) >= 0.0) ? ccwM : (ccwM * -1.0);
                double vCircM = std::sqrt(cfg.muMoon / dM);
                Vector2D vTargetLoi = moonVel + tanM * vCircM;
                Vector2D dvVec = vTargetLoi - r.vel;
                double dvMag = dvVec.magnitude();

                if (dvMag > 1.0) {
                    r.angleRad = std::atan2(dvVec.y, dvVec.x);
                    r.targetAngleRad = r.angleRad;
                }
                r.throttle = 1.0;
                double accel = (r.fuelKg > 0.0) ? ((r.maxThrustN * r.throttle) / r.totalMass()) : 0.0;
                r.burnRemainingDv = std::min(r.burnRemainingDv - accel * dt, dvMag);
                snprintf(r.actionPrompt, sizeof(r.actionPrompt), "P21: LOI BURN IN PROGRESS");

                if (r.burnRemainingDv <= 0.0 || dvMag <= 12.0) {
                    r.vel = vTargetLoi;
                    r.burnRemainingDv = 0.0;
                    r.throttle = 0.0;
                    r.autoBurnActive = false;
                    r.phase = MissionPhase::LUNAR_ORBIT;
                    r.lunarOrbitTimer = 0.0;
                    r.activeProg = AgcProgram::P21_LOI_BURN; // Stay in P21 during lunar orbit
                    r.selectedProg = AgcProgram::P30_TEI_BURN;
                    AudioSys::playQuindarTone(cfg.soundEnabled);
                }
            }
            updateVerbNoun(r);
            return;
        }

        // 6. Lunar Orbit Cruise: orbit the Moon, then open TEI window in the optimal trailing departure sector
        if (r.phase == MissionPhase::LUNAR_ORBIT) {
            Vector2D moonDir = (moonVel.magnitude() > 1.0) ? moonVel.normalized() : Vector2D(0, 1);
            Vector2D moonRetro = moonDir * -1.0;
            Vector2D relVelM = r.vel - moonVel;
            Vector2D tanM = (relVelM.magnitude() > 1.0) ? relVelM.normalized() : moonRetro;
            double retroAlign = tanM.dot(moonRetro);

            double minOrbitTime = (cfg.physicsScale == PhysicsScale::REALISTIC) ? 120.0 : 45.0;
            bool timeElapsed = (r.lunarOrbitTimer >= minOrbitTime);
            // Trailing departure window: spacecraft orbital velocity is aligned with the retrograde exit direction
            bool sectorAligned = (retroAlign >= 0.50);
            bool readyToBurn = timeElapsed && sectorAligned;
            r.teiWindowOpen = readyToBurn;

            if (!r.autoBurnActive) {
                r.angleRad = std::atan2(r.vel.y, r.vel.x);
                r.targetAngleRad = r.angleRad;
                r.throttle = 0.0;
                r.lunarOrbitTimer += dt;
                r.activeProg = AgcProgram::P21_LOI_BURN; // Visible as P21 on DSKY and Map
                if (cfg.controlMode != ControlMode::SMART_ASSIST) {
                    r.selectedProg = AgcProgram::P30_TEI_BURN;
                }
                double teiRemaining = std::max(0.0, minOrbitTime - r.lunarOrbitTimer);
                int cdM = static_cast<int>(teiRemaining / 60);
                int cdS = static_cast<int>(teiRemaining) % 60;

                if (readyToBurn) {
                    if (r.isDemoFlight) {
                        r.selectedProg = AgcProgram::P30_TEI_BURN;
                        executeAgcProgram(r, cfg);
                    } else if (cfg.controlMode == ControlMode::FLIGHT_DIRECTOR) {
                        snprintf(r.actionPrompt, sizeof(r.actionPrompt), "[ENTER] GO FOR TEI BURN");
                    } else { // SMART_ASSIST
                        if (r.selectedProg == AgcProgram::P30_TEI_BURN) {
                            snprintf(r.actionPrompt, sizeof(r.actionPrompt), "TEI READY: [ENTER]");
                        } else {
                            snprintf(r.actionPrompt, sizeof(r.actionPrompt), "TEI WIN: SELECT P30 (N)");
                        }
                    }
                } else {
                    if (!timeElapsed) {
                        if (cfg.controlMode == ControlMode::SMART_ASSIST) {
                            snprintf(r.actionPrompt, sizeof(r.actionPrompt), "LUNAR ORB: T-%02d:%02d", cdM, cdS);
                        } else {
                            snprintf(r.actionPrompt, sizeof(r.actionPrompt), "TEI IN %02d:%02d", cdM, cdS);
                        }
                    } else {
                        // Time elapsed, but rotating into the trailing escape window
                        if (cfg.controlMode == ControlMode::SMART_ASSIST) {
                            snprintf(r.actionPrompt, sizeof(r.actionPrompt), "ORB: ALIGNING TEI");
                        } else {
                            snprintf(r.actionPrompt, sizeof(r.actionPrompt), "ALIGNING TEI WINDOW");
                        }
                    }
                }
            } else if (r.activeProg == AgcProgram::P30_TEI_BURN) {
                double rM = (r.pos - moonPos).magnitude();
                double vEscM = std::sqrt(2.0 * cfg.muMoon / std::max(rM, cfg.moonRadius + 10000.0));
                double excessDv = (cfg.physicsScale == PhysicsScale::REALISTIC) ? 220.0 : 25.0;
                double vTargetEsc = vEscM + excessDv;
                Vector2D ejectDir = (tanM.dot(moonRetro) >= 0.0) ? tanM : moonRetro;
                Vector2D vTargetTei = moonVel + ejectDir * vTargetEsc;
                Vector2D dvVec = vTargetTei - r.vel;
                double dvMag = dvVec.magnitude();

                if (dvMag > 1.0) {
                    r.angleRad = std::atan2(dvVec.y, dvVec.x);
                    r.targetAngleRad = r.angleRad;
                }
                r.throttle = 1.0;
                double accel = (r.fuelKg > 0.0) ? ((r.maxThrustN * r.throttle) / r.totalMass()) : 0.0;
                r.burnRemainingDv = std::min(r.burnRemainingDv - accel * dt, dvMag);
                snprintf(r.actionPrompt, sizeof(r.actionPrompt), "P30: TEI BURN IN PROGRESS");

                if (r.burnRemainingDv <= 0.0 || dvMag <= 20.0) {
                    r.vel = vTargetTei;
                    r.burnRemainingDv = 0.0;
                    r.throttle = 0.0;
                    r.autoBurnActive = false;
                    r.phase = MissionPhase::TRANSEARTH_COAST;
                    r.activeProg = AgcProgram::P30_TEI_BURN; // Stay in P30 during Transearth Coast
                    r.selectedProg = (cfg.controlMode == ControlMode::FLIGHT_DIRECTOR || r.isDemoFlight)
                        ? AgcProgram::P63_REENTRY : AgcProgram::P30_TEI_BURN;
                    r.teiWindowOpen = false;
                    AudioSys::playQuindarTone(cfg.soundEnabled);
                }
            }
            updateVerbNoun(r);
            return;
        }

        // 7. Transearth Coast: return to Earth, then trigger P63 Re-entry
        if (r.phase == MissionPhase::TRANSEARTH_COAST) {
            r.throttle = 0.0;
            if (distM >= cfg.moonSOI * 0.75 && altE > 150000.0) {
                alignEarthReturnCorridor(r, cfg);
            }
            if (altE < cfg.atmosphereHeight * 3.5) {
                // Approaching Earth: switch to P63 Re-entry and prepare heatshield!
                r.angleRad = std::atan2(-r.vel.y, -r.vel.x); // Retrograde heatshield
                r.targetAngleRad = r.angleRad;
                if (r.stage < 3) {
                    if (r.isDemoFlight) {
                        loadStageSpecs(r, cfg, 3); // Jettison Service Module -> Command Module (STG 4)
                        snprintf(r.actionPrompt, sizeof(r.actionPrompt), "P63: HEATSHIELD RETRO");
                    } else if (cfg.controlMode == ControlMode::FLIGHT_DIRECTOR) {
                        // Houston mode: Player can press [ENTER] to separate, or Houston auto-separates at 75 km
                        if (altE <= cfg.atmosphereHeight + 10000.0) {
                            loadStageSpecs(r, cfg, 3);
                            AudioSys::playStagingClick(cfg.soundEnabled);
                            snprintf(r.actionPrompt, sizeof(r.actionPrompt), "P63: HEATSHIELD RETRO");
                        } else {
                            snprintf(r.actionPrompt, sizeof(r.actionPrompt), "[ENTER] SEPARATE SM / ENTRY");
                        }
                    } else { // SMART_ASSIST: Pilot must manually jettison SM using [SPACE]
                        snprintf(r.actionPrompt, sizeof(r.actionPrompt), "[SPACE] JETTISON SM!");
                    }
                } else {
                    if (cfg.controlMode == ControlMode::SMART_ASSIST) {
                        if (r.selectedProg == AgcProgram::P63_REENTRY) {
                            snprintf(r.actionPrompt, sizeof(r.actionPrompt), "ENTRY READY: [ENTER] P63");
                        } else {
                            snprintf(r.actionPrompt, sizeof(r.actionPrompt), "ENTRY: SELECT P63 (N)");
                        }
                    } else {
                        snprintf(r.actionPrompt, sizeof(r.actionPrompt), "P63: HEATSHIELD RETRO");
                    }
                }
                if (cfg.controlMode != ControlMode::SMART_ASSIST) {
                    r.activeProg = AgcProgram::P63_REENTRY;
                    r.selectedProg = AgcProgram::P63_REENTRY;
                }
            } else {
                r.angleRad = std::atan2(r.vel.y, r.vel.x);
                r.targetAngleRad = r.angleRad;
                r.activeProg = AgcProgram::P30_TEI_BURN; // P30 Transearth Coast until Earth arrival
                if (cfg.controlMode != ControlMode::SMART_ASSIST) {
                    r.selectedProg = AgcProgram::P63_REENTRY;
                    snprintf(r.actionPrompt, sizeof(r.actionPrompt), "P30: TRANSEARTH COAST");
                } else {
                    if (r.selectedProg == AgcProgram::P30_TEI_BURN) {
                        snprintf(r.actionPrompt, sizeof(r.actionPrompt), "P30: TRANSEARTH COAST");
                    } else if (r.selectedProg == AgcProgram::P63_REENTRY) {
                        snprintf(r.actionPrompt, sizeof(r.actionPrompt), "COAST: P63 ARMED");
                    } else {
                        snprintf(r.actionPrompt, sizeof(r.actionPrompt), "EARTH DIST: %.0f KM", altE / 1000.0);
                    }
                }
            }

            // Check if spacecraft skips off atmosphere (perigee too high, flying back into deep space)
            if (altE < 300000.0 && altE > 75000.0 && r.vel.dot(upE) > 50.0 && missionTime > 80.0) {
                r.crashed = true;
                r.phase = MissionPhase::MISSION_FAILED;
                r.throttle = 0.0;
                snprintf(r.failureReason, sizeof(r.failureReason), "SKIPPED OFF ATMOSPHERE");
                AudioSys::playMasterAlarm(cfg.soundEnabled);
                return;
            }

            if (altE < cfg.atmosphereHeight && r.vel.dot(upE) < 0.0) {
                // Mandatory Service Module Jettison check:
                // Entering dense atmosphere with unshielded SM stack causes fatal crash!
                if (r.stage < 3) {
                    if (r.isDemoFlight || cfg.controlMode == ControlMode::FLIGHT_DIRECTOR) {
                        loadStageSpecs(r, cfg, 3);
                        AudioSys::playStagingClick(cfg.soundEnabled);
                    } else {
                        r.crashed = true;
                        r.phase = MissionPhase::MISSION_FAILED;
                        r.throttle = 0.0;
                        snprintf(r.failureReason, sizeof(r.failureReason), "SM NOT JETTISONED");
                        AudioSys::playMasterAlarm(cfg.soundEnabled);
                        return;
                    }
                }
                r.phase = MissionPhase::ATMOSPHERIC_REENTRY;
                r.activeProg = AgcProgram::P63_REENTRY;
                AudioSys::playQuindarTone(cfg.soundEnabled);
            }
            updateVerbNoun(r);
            return;
        }

        // 8. Atmospheric Reentry & Parachutes
        if (r.phase == MissionPhase::ATMOSPHERIC_REENTRY) {
            r.angleRad = std::atan2(-r.vel.y, -r.vel.x); // Retrograde heatshield
            r.targetAngleRad = r.angleRad;
            r.throttle = 0.0;
            r.activeProg = AgcProgram::P63_REENTRY;
            if (altE > 7000.0) {
                snprintf(r.actionPrompt, sizeof(r.actionPrompt), "AEROBRAKING ENTRY");
            } else if (altE >= 2500.0) {
                snprintf(r.actionPrompt, sizeof(r.actionPrompt), "DROGUE CHUTES (45M/S)");
            } else {
                snprintf(r.actionPrompt, sizeof(r.actionPrompt), "MAIN CHUTES (8.5M/S)");
            }
            updateVerbNoun(r);
            return;
        }
    }

    static void stepRK4(RocketState& r, const SimConfig& cfg, double missionTime, double dt) {
        if (r.crashed || r.splashedDown) return;

        updateFlightDirector(r, cfg, missionTime, dt);
        if (r.splashedDown || r.crashed) return;

        // While stationary on the launch pad awaiting liftoff:
        if (r.onPad) {
            Vector2D upE = r.pos.normalized();
            r.pos = upE * (cfg.earthRadius + 2.0);
            r.vel = Vector2D(0, 0);
            r.gForce = 1.0;
            return;
        }

        Vector2D mStart = getMoonPos(cfg, missionTime);
        Vector2D mMid   = getMoonPos(cfg, missionTime + dt * 0.5);
        Vector2D mEnd   = getMoonPos(cfg, missionTime + dt);

        bool lunarOrbitStabilize = (r.phase == MissionPhase::LUNAR_ORBIT && r.activeProg != AgcProgram::P30_TEI_BURN && !r.autoBurnActive);

        double thrust = 0.0;
        double massFlow = 0.0;
        if (r.fuelKg > 0.0 && r.throttle > 0.0) {
            thrust = r.maxThrustN * r.throttle;
            massFlow = thrust / (r.isp * 9.80665);
        }

        double m0 = r.totalMass();

        // RK4 integration
        Vector2D p1 = r.pos;
        Vector2D v1 = r.vel;
        Vector2D a1 = computeAcceleration(cfg, p1, v1, mStart, thrust, r.angleRad, m0, lunarOrbitStabilize);

        Vector2D p2 = p1 + v1 * (dt * 0.5);
        Vector2D v2 = v1 + a1 * (dt * 0.5);
        Vector2D a2 = computeAcceleration(cfg, p2, v2, mMid, thrust, r.angleRad, m0, lunarOrbitStabilize);

        Vector2D p3 = p1 + v2 * (dt * 0.5);
        Vector2D v3 = v1 + a2 * (dt * 0.5);
        Vector2D a3 = computeAcceleration(cfg, p3, v3, mMid, thrust, r.angleRad, m0, lunarOrbitStabilize);

        Vector2D p4 = p1 + v3 * dt;
        Vector2D v4 = v1 + a3 * dt;
        Vector2D a4 = computeAcceleration(cfg, p4, v4, mEnd, thrust, r.angleRad, m0, lunarOrbitStabilize);

        r.pos = r.pos + (v1 + v2 * 2.0 + v3 * 2.0 + v4) * (dt / 6.0);
        r.vel = r.vel + (a1 + a2 * 2.0 + a3 * 2.0 + a4) * (dt / 6.0);

        // Fuel consumption
        if (massFlow > 0.0) {
            r.fuelKg = std::max(0.0, r.fuelKg - massFlow * dt);
            if (r.fuelKg <= 0.0) {
                r.throttle = 0.0;
            }
        }
        updateDeltaV(r);

        // Compute proper G-force (non-gravitational acceleration from thrust + atmospheric drag)
        double rE = r.pos.magnitude();
        double altE = rE - cfg.earthRadius;
        Vector2D relM = p1 - mStart;
        double rM = relM.magnitude();
        Vector2D aGrav = p1 * (-cfg.muEarth / (rE * rE * rE)) +
                         ((rM > 10.0) ? (relM * (-cfg.muMoon / (rM * rM * rM))) : Vector2D(0, 0));
        Vector2D nonGravAccel = a1 - aGrav;
        r.gForce = nonGravAccel.magnitude() / 9.80665;

        if (altE < cfg.atmosphereHeight && altE > 0.0) {
            double rho = 1.225 * std::exp(-altE / cfg.scaleHeight);
            double spd = r.vel.magnitude();
            double tempCoeff = (cfg.physicsScale == PhysicsScale::REALISTIC) ? 8.0e-7 : 2.0e-6;
            r.heatShieldTemp = 290.0 + tempCoeff * rho * spd * spd * spd;

            // Check heatshield alignment during intense atmospheric re-entry heating
            if (r.phase == MissionPhase::ATMOSPHERIC_REENTRY && r.heatShieldTemp > 1000.0) {
                double retroAngle = std::atan2(-r.vel.y, -r.vel.x);
                double angDiff = r.angleRad - retroAngle;
                while (angDiff > M_PI) angDiff -= 2.0 * M_PI;
                while (angDiff < -M_PI) angDiff += 2.0 * M_PI;
                // If entering dense atmosphere without heatshield oriented retrograde (> 65 deg off)
                if (std::abs(angDiff) > 1.15) {
                    r.crashed = true;
                    r.phase = MissionPhase::MISSION_FAILED;
                    r.throttle = 0.0;
                    snprintf(r.failureReason, sizeof(r.failureReason), "HEAT SHIELD BURN-UP");
                    AudioSys::playMasterAlarm(cfg.soundEnabled);
                    return;
                }
            }

            // Check excessive G-force during steep re-entry
            double maxSafeG = (cfg.physicsScale == PhysicsScale::REALISTIC) ? 55.0 : 45.0;
            if (r.phase == MissionPhase::ATMOSPHERIC_REENTRY && r.gForce > maxSafeG) {
                r.crashed = true;
                r.phase = MissionPhase::MISSION_FAILED;
                r.throttle = 0.0;
                snprintf(r.failureReason, sizeof(r.failureReason), "EXCESSIVE G-FORCE");
                AudioSys::playMasterAlarm(cfg.soundEnabled);
                return;
            }

            // Check extreme heat shield thermal limit (AVCOAT ablation destruction >3200 K)
            if (r.phase == MissionPhase::ATMOSPHERIC_REENTRY && r.heatShieldTemp > 3200.0) {
                r.crashed = true;
                r.phase = MissionPhase::MISSION_FAILED;
                r.throttle = 0.0;
                snprintf(r.failureReason, sizeof(r.failureReason), "HEAT SHIELD MELTDOWN");
                AudioSys::playMasterAlarm(cfg.soundEnabled);
                return;
            }

            // Authentic Apollo Parachute Sequence:
            // Drogue chutes deploy at 7.0 km (slowing descent to ~45 m/s)
            if (altE < 7000.0 && altE >= 2500.0 && missionTime > 20.0 && r.vel.dot(r.pos.normalized()) < 0.0) {
                // If velocity is too high at 7.0 km without sufficient aerobraking, parachutes tear off!
                if (spd > 550.0) {
                    r.crashed = true;
                    r.phase = MissionPhase::MISSION_FAILED;
                    r.throttle = 0.0;
                    snprintf(r.failureReason, sizeof(r.failureReason), "PARACHUTES TORN OFF");
                    AudioSys::playMasterAlarm(cfg.soundEnabled);
                    return;
                }
                double targetSpd = 45.0;
                if (spd > targetSpd) {
                    r.vel = r.vel * std::max(0.85, 1.0 - 0.8 * dt);
                }
            } else if (altE < 2500.0 && missionTime > 20.0 && r.vel.dot(r.pos.normalized()) < 0.0) {
                // Main chutes deploy at 2.5 km: Authentic Apollo touchdown velocity 8.5 m/s (28 ft/s)
                if (spd > 120.0) {
                    r.crashed = true;
                    r.phase = MissionPhase::MISSION_FAILED;
                    r.throttle = 0.0;
                    snprintf(r.failureReason, sizeof(r.failureReason), "MAIN CHUTES TORN OFF");
                    AudioSys::playMasterAlarm(cfg.soundEnabled);
                    return;
                }
                double targetSpd = 8.5;
                if (spd > targetSpd) {
                    r.vel = r.vel * std::max(0.70, 1.0 - 1.5 * dt);
                }
            }
        } else {
            r.heatShieldTemp = std::max(290.0, r.heatShieldTemp - 25.0 * dt);
        }

        // Check Earth surface contact
        if (rE <= cfg.earthRadius) {
            r.pos = r.pos.normalized() * cfg.earthRadius;
            r.throttle = 0.0;
            if (r.phase == MissionPhase::ATMOSPHERIC_REENTRY || r.stage == 3) {
                if (r.vel.magnitude() < 40.0) {
                    r.splashedDown = true;
                    r.phase = MissionPhase::SPLASHDOWN_SUCCESS;
                    r.vel = Vector2D(0, 0);
                    AudioSys::playQuindarTone(cfg.soundEnabled);
                } else {
                    r.crashed = true;
                    r.phase = MissionPhase::MISSION_FAILED;
                    r.vel = Vector2D(0, 0);
                    snprintf(r.failureReason, sizeof(r.failureReason), "EXCESSIVE IMPACT SPEED");
                    AudioSys::playMasterAlarm(cfg.soundEnabled);
                }
            } else if (r.phase == MissionPhase::LAUNCH_ASCENT && missionTime < 10.0) {
                // Buffer launch pad clearance
                r.pos = r.pos.normalized() * (cfg.earthRadius + 5.0);
                if (r.vel.dot(r.pos.normalized()) < 0.0) {
                    r.vel = r.vel - r.pos.normalized() * r.vel.dot(r.pos.normalized());
                }
            } else {
                r.crashed = true;
                r.phase = MissionPhase::MISSION_FAILED;
                r.vel = Vector2D(0, 0);
                snprintf(r.failureReason, sizeof(r.failureReason), "SURFACE IMPACT CRASH");
                AudioSys::playMasterAlarm(cfg.soundEnabled);
            }
        }

        // Check Moon surface contact
        if ((r.pos - mEnd).magnitude() <= cfg.moonRadius) {
            r.crashed = true;
            r.phase = MissionPhase::MISSION_FAILED;
            r.throttle = 0.0;
            snprintf(r.failureReason, sizeof(r.failureReason), "LUNAR SURFACE CRASH");
            AudioSys::playMasterAlarm(cfg.soundEnabled);
            return;
        }

        // 1. Check Lost in Deep Space (Escaped Earth-Moon gravitational sphere)
        double distM = (r.pos - mEnd).magnitude();
        if (rE > cfg.moonDistance * 1.45 && distM > cfg.moonSOI * 1.5) {
            r.crashed = true;
            r.phase = MissionPhase::MISSION_FAILED;
            r.throttle = 0.0;
            snprintf(r.failureReason, sizeof(r.failureReason), "LOST IN DEEP SPACE");
            AudioSys::playMasterAlarm(cfg.soundEnabled);
            return;
        }

        // 2. Check Stranded in Space with no fuel
        if (r.fuelKg <= 0.05 && !r.onPad && !r.autoBurnActive &&
            r.phase != MissionPhase::ATMOSPHERIC_REENTRY &&
            r.phase != MissionPhase::SPLASHDOWN_SUCCESS) {
            if (r.phase == MissionPhase::LOW_EARTH_ORBIT) {
                r.crashed = true;
                r.phase = MissionPhase::MISSION_FAILED;
                r.throttle = 0.0;
                snprintf(r.failureReason, sizeof(r.failureReason), "STRANDED: NO FUEL FOR TLI");
                AudioSys::playMasterAlarm(cfg.soundEnabled);
                return;
            } else if (r.phase == MissionPhase::LUNAR_ORBIT) {
                r.crashed = true;
                r.phase = MissionPhase::MISSION_FAILED;
                r.throttle = 0.0;
                snprintf(r.failureReason, sizeof(r.failureReason), "STRANDED IN LUNAR ORBIT");
                AudioSys::playMasterAlarm(cfg.soundEnabled);
                return;
            } else if (r.phase == MissionPhase::TRANSLUNAR_COAST && distM > cfg.moonSOI * 1.2) {
                r.crashed = true;
                r.phase = MissionPhase::MISSION_FAILED;
                r.throttle = 0.0;
                snprintf(r.failureReason, sizeof(r.failureReason), "STRANDED: MISSED MOON");
                AudioSys::playMasterAlarm(cfg.soundEnabled);
                return;
            }
        }

        // 3. Check Life Support Consumables limit (Apollo CM/SM Consumables Reserve)
        // Realistic 1:1 mission is 7-8 days (~650,000s) -> generous limit is 20 days (1,728,000s)
        // Arcade mode MET is ~10h (36,000s) -> generous limit is ~110h (400,000s)
        double maxMissionTime = (cfg.physicsScale == PhysicsScale::REALISTIC) ? 1728000.0 : 400000.0;
        if (missionTime > maxMissionTime && !r.splashedDown && !r.onPad) {
            r.crashed = true;
            r.phase = MissionPhase::MISSION_FAILED;
            r.throttle = 0.0;
            snprintf(r.failureReason, sizeof(r.failureReason), "LIFE SUPPORT EXHAUSTED");
            AudioSys::playMasterAlarm(cfg.soundEnabled);
            return;
        }
    }
};

} // namespace Apollo
