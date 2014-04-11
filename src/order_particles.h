/*
 * Complex particle system.
 * Basic rules give order to things.
 * Sometimes too rigid, like a crystal.
 *
 * (c) 2014 Micah Elizabeth Scott
 * http://creativecommons.org/licenses/by/3.0/
 */

#pragma once

#include <vector>
#include "lib/particle.h"
#include "lib/prng.h"
#include "lib/texture.h"


class OrderParticles : public ParticleEffect
{
public:
    OrderParticles();
    void reseed(unsigned seed);

    virtual void beginFrame(const FrameInfo &f);
    virtual void shader(Vec3& rgb, const PixelInfo& p) const;
    virtual void debug(const DebugInfo &di);

    Texture palette;
    float vibration;
    int symmetry;
    float colorCycle;

private:
    static const unsigned numParticles = 180;
    static const float relativeSize = 0.3;
    static const float intensity = 0.06;
    static const float stepSize = 1.0 / 500;
    static const float seedRadius = 2.0;
    static const float interactionSize = 0.5;
    static const float angleGain = 0.002;
    static const float colorRate = 0.03;

    float timeDeltaRemainder;

    void runStep(const FrameInfo &f);
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


inline OrderParticles::OrderParticles()
    : palette("data/mars-palette.png"),
      timeDeltaRemainder(0)
{
    reseed(42);
}

inline void OrderParticles::reseed(unsigned seed)
{
    vibration = 0;
    symmetry = 1000;

    appearance.resize(numParticles);

    PRNG prng;
    prng.seed(seed);

    colorCycle = prng.uniform(0, 2*M_PI);

    for (unsigned i = 0; i < appearance.size(); i++) {
        Vec2 p = prng.ringVector(1e-4, seedRadius);
        appearance[i].point = Vec3(p[0], 0, p[1]);

        // Assume plain white for now; we do our own coloration in shader()
        appearance[i].color = Vec3(1,1,1);
    }
}

inline void OrderParticles::beginFrame(const FrameInfo &f)
{    
    float t = f.timeDelta + timeDeltaRemainder;
    int steps = t / stepSize;
    timeDeltaRemainder = t - steps * stepSize;

    while (steps > 0) {
        runStep(f);
        steps--;
    }

    colorCycle = fmodf(colorCycle + f.timeDelta * colorRate, 2 * M_PI);
}

inline void OrderParticles::runStep(const FrameInfo &f)
{
    PRNG prng;
    prng.seed(20);

    // Update dynamics
    for (unsigned i = 0; i < appearance.size(); i++) {

        float fade = 1.0;

        appearance[i].intensity = intensity * fade;
        appearance[i].radius = f.modelDiameter * relativeSize * fade;

        prng.remix(appearance[i].point[0] * 1e8);
        prng.remix(appearance[i].point[2] * 1e8);

        // Vibration
        Vec2 v = prng.ringVector(0.5, 1) * vibration;
        appearance[i].point += Vec3(v[0], 0, v[1]);
    }

    // Rebuild index
    ParticleEffect::beginFrame(f);

    // Particle interactions
    for (unsigned i = 0; i < appearance.size(); i++) {

        Vec3 &p = appearance[i].point;
        std::vector<std::pair<size_t, Real> > hits;
        nanoflann::SearchParams params;
        params.sorted = false;

        float searchRadius2 = sq(interactionSize * f.modelDiameter);
        unsigned numHits = index.tree.radiusSearch(&p[0], searchRadius2, hits, params);

        for (unsigned i = 0; i < numHits; i++) {
            if (hits[i].first <= i) {
                // Only count each interaction once
                continue;
            }

            // Check distance
            ParticleAppearance &hit = appearance[hits[i].first];
            float q2 = hits[i].second / searchRadius2;
            if (q2 < 1.0f) {
                // These particles influence each other
                Vec3 d = hit.point - p;

                // Angular 'snap' force, operates at a distance
                float angle = atan2(d[2], d[0]);
                const float angleIncrement = 2 * M_PI / symmetry;
                float snapAngle = roundf(angle / angleIncrement) * angleIncrement;
                float angleDelta = fabsf(snapAngle - angle);

                // Spin perpendicular to 'd'
                Vec3 da = (angleGain * angleDelta / (1.0f + q2)) * Vec3( d[2], 0, -d[0] );
                p += da;
                hit.point -= da;
            }
        }
    }
}

inline void OrderParticles::debug(const DebugInfo &di)
{
    fprintf(stderr, "\t[order-particles] symmetry = %d\n", symmetry);
    fprintf(stderr, "\t[order-particles] vibration = %f\n", vibration);
    fprintf(stderr, "\t[order-particles] colorCycle = %f\n", colorCycle);
}

inline void OrderParticles::shader(Vec3& rgb, const PixelInfo& p) const
{
    ParticleEffect::shader(rgb, p);

    // Metaball-style shading: Use computed intensity as parameter for a
    // nonlinear color palette.

    rgb = palette.sample(0.5 + 0.5 * sinf(colorCycle), rgb[0]);
}