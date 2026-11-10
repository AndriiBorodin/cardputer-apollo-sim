#pragma once
#include "ApolloTypes.h"
#include "OrbitalPhysics.h"
#include <algorithm>

namespace Apollo {

class OrbitPredictor {
public:
    static void computeTrajectory(const RocketState& r,
                                  const SimConfig& cfg,
                                  double currentMissionTime,
                                  TrajectoryForecast& out) {
        out.count = 0;
        out.apoapsisAlt = 0.0;
        out.periapsisAlt = 0.0;
        out.closestMoonAlt = 1e15;
        out.entersMoonSOI = false;
        out.freeReturnDetected = false;
        out.impactsEarth = false;
        out.impactsMoon = false;

        if (r.onPad || r.crashed || r.splashedDown) {
            out.pts[0] = r.pos;
            out.count = 1;
            out.apoapsisPos = r.pos;
            out.periapsisPos = r.pos;
            return;
        }

        double rE0 = r.pos.magnitude();
        double alt0 = rE0 - cfg.earthRadius;
        double vMag0 = r.vel.magnitude();
        double specEnergy = 0.5 * vMag0 * vMag0 - cfg.muEarth / rE0;

        Vector2D p = r.pos;
        Vector2D v = r.vel;
        double m = r.totalMass();
        double t = currentMissionTime;

        out.pts[0] = p;
        out.count = 1;
        out.apoapsisAlt = alt0;
        out.periapsisAlt = alt0;
        out.apoapsisPos = p;
        out.periapsisPos = p;

        // Determine if we are in a closed low/medium Earth orbit vs high/translunar trajectory
        bool isLowClosedOrbit = (specEnergy < -0.35 * (cfg.muEarth / cfg.earthRadius)) && (rE0 < cfg.earthRadius * 2.2);
        double fixedOrbitalDt = 10.0;
        if (isLowClosedOrbit) {
            double semiMajorA = -cfg.muEarth / (2.0 * specEnergy);
            if (semiMajorA > cfg.earthRadius * 0.5) {
                double period = 2.0 * M_PI * std::sqrt((semiMajorA * semiMajorA * semiMajorA) / cfg.muEarth);
                fixedOrbitalDt = std::max(1.5, (period * 1.02) / TRAJ_POINTS);
            }
        }

        bool descendingInAtmo = (alt0 < cfg.atmosphereHeight * 2.0 && r.vel.dot(r.pos.normalized()) < -5.0);
        bool lunarOrbitStabilize = (r.phase == MissionPhase::LUNAR_ORBIT && r.activeProg != AgcProgram::P30_TEI_BURN && !r.autoBurnActive);
        double maxAdaptiveDt = (cfg.physicsScale == PhysicsScale::REALISTIC) ? 8000.0 : 420.0;
        bool wasNearMoon = false;

        for (int i = 1; i < TRAJ_POINTS; ++i) {
            Vector2D mStart = OrbitalPhysics::getMoonPos(cfg, t);
            double rE = p.magnitude();
            double dM = (p - mStart).magnitude();
            double vCur = v.magnitude();

            double dt;
            if (descendingInAtmo) {
                dt = (cfg.physicsScale == PhysicsScale::REALISTIC) ? 4.5 : 1.5;
            } else if (isLowClosedOrbit && dM > cfg.moonSOI * 1.5) {
                dt = fixedOrbitalDt;
            } else {
                double dMin = std::min(rE, dM);
                double stepFactor = (cfg.physicsScale == PhysicsScale::REALISTIC) ? 0.22 : 0.16;
                double rawDt = stepFactor * dMin / std::max(100.0, vCur);
                dt = std::max(2.0, std::min(maxAdaptiveDt, rawDt));
            }

            Vector2D mMid = OrbitalPhysics::getMoonPos(cfg, t + dt * 0.5);
            Vector2D mEnd = OrbitalPhysics::getMoonPos(cfg, t + dt);

            Vector2D mS = mStart;
            Vector2D mM = mMid;
            Vector2D mE = mEnd;

            // Ballistic coasting (thrust = 0)
            Vector2D a1 = OrbitalPhysics::computeAcceleration(cfg, p, v, mS, 0.0, 0.0, m, lunarOrbitStabilize);

            Vector2D p2 = p + v * (dt * 0.5);
            Vector2D v2 = v + a1 * (dt * 0.5);
            Vector2D a2 = OrbitalPhysics::computeAcceleration(cfg, p2, v2, mM, 0.0, 0.0, m, lunarOrbitStabilize);

            Vector2D p3 = p + v2 * (dt * 0.5);
            Vector2D v3 = v + a2 * (dt * 0.5);
            Vector2D a3 = OrbitalPhysics::computeAcceleration(cfg, p3, v3, mM, 0.0, 0.0, m, lunarOrbitStabilize);

            Vector2D p4 = p + v3 * dt;
            Vector2D v4 = v + a3 * dt;
            Vector2D a4 = OrbitalPhysics::computeAcceleration(cfg, p4, v4, mE, 0.0, 0.0, m, lunarOrbitStabilize);

            p = p + (v + v2 * 2.0 + v3 * 2.0 + v4) * (dt / 6.0);
            v = v + (a1 + a2 * 2.0 + a3 * 2.0 + a4) * (dt / 6.0);
            t += dt;

            out.pts[out.count++] = p;

            double altE = p.magnitude() - cfg.earthRadius;
            if (altE > out.apoapsisAlt) {
                out.apoapsisAlt = altE;
                out.apoapsisPos = p;
            }
            if (altE < out.periapsisAlt) {
                out.periapsisAlt = altE;
                out.periapsisPos = p;
            }

            double distM = (p - mEnd).magnitude();
            double altM = distM - cfg.moonRadius;
            if (altM < out.closestMoonAlt) {
                out.closestMoonAlt = altM;
                out.closestMoonPos = p;
            }

            if (distM < cfg.moonSOI) {
                out.entersMoonSOI = true;
                wasNearMoon = true;
            } else if (wasNearMoon && altE < cfg.earthRadius * 2.0) {
                out.freeReturnDetected = true;
            }

            if (altE <= 0.0 || (altE < cfg.atmosphereHeight * 0.35 && v.dot(p.normalized()) < 0.0)) {
                out.impactsEarth = true;
                break;
            }
            if (altM <= 0.0) {
                out.impactsMoon = true;
                break;
            }
        }

        // Refine apoapsis & periapsis analytically with Keplerian 2-body mechanics when in Earth orbit outside Moon SOI
        Vector2D moonNow = OrbitalPhysics::getMoonPos(cfg, currentMissionTime);
        if (!out.entersMoonSOI && (r.pos - moonNow).magnitude() > cfg.moonSOI && specEnergy < -10.0) {
            double semiMajorA = -cfg.muEarth / (2.0 * specEnergy);
            double h = r.pos.x * r.vel.y - r.pos.y * r.vel.x; // Specific angular momentum
            double eccSq = 1.0 + (2.0 * specEnergy * h * h) / (cfg.muEarth * cfg.muEarth);
            double ecc = std::sqrt(std::max(0.0, eccSq));
            out.apoapsisAlt = semiMajorA * (1.0 + ecc) - cfg.earthRadius;
            out.periapsisAlt = semiMajorA * (1.0 - ecc) - cfg.earthRadius;
        }
    }
};

} // namespace Apollo
