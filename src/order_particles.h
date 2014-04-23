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
#include "lib/noise.h"
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
    int symmetry;
    float colorCycle;
    float baseAngle;

private:
    static const unsigned numParticles = 70;
    static const float relativeSize = 0.42;
    static const float intensity = 0.16;
    static const float brightness = 1.35;
    static const float stepSize = 1.0 / 300;
    static const float seedRadius = 3.0;
    static const float interactionSize = 0.56;
    static const float colorRate = 0.02;
    static const float lightSpinRate = 60.0;
    static const float angleGainRate = 0.3;
    static const float angleGainCenter = 0.02;
    static const float angleGainVariation = angleGainCenter * 0.3;

    unsigned seed;
    float timeDeltaRemainder;

    // Calculated per-frame
    Vec3 lightVec;
    float angleGain;

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
    symmetry = 1000;

    appearance.resize(numParticles);

    PRNG prng;
    prng.seed(seed);
    this->seed = seed;

    colorCycle = prng.uniform(0, 20 * M_PI);

    for (unsigned i = 0; i < appearance.size(); i++) {
        Vec2 p = prng.ringVector(1e-4, seedRadius);
        appearance[i].point = Vec3(p[0], 0, p[1]);
    }
}

inline void OrderParticles::beginFrame(const FrameInfo &f)
{    
    // Rebuild index
    ParticleEffect::beginFrame(f);

    float t = f.timeDelta + timeDeltaRemainder;
    int steps = t / stepSize;
    timeDeltaRemainder = t - steps * stepSize;

    // Particle appearance
    for (unsigned i = 0; i < appearance.size(); i++) {
        appearance[i].intensity = intensity;
        appearance[i].radius = f.modelDiameter * relativeSize;
    }

    while (steps > 0) {
        runStep(f);
        steps--;

        // Build index again after each physics step
        ParticleEffect::beginFrame(f);
    }

    // Lighting
    colorCycle += f.timeDelta * colorRate;
    float lightAngle = fbm_noise2(colorCycle * 0.08f, seed * 1e-6, 2) * lightSpinRate;
    lightVec = Vec3(sin(lightAngle), 0, cos(lightAngle));

    // Angular speed and direction
    angleGain = angleGainCenter + angleGainVariation *
        fbm_noise2(colorCycle * angleGainRate, seed * 5e-7, 2);
}

inline void OrderParticles::runStep(const FrameInfo &f)
{
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
                Vec3 da = angleGain * angleDelta * kernel2(q2) * Vec3( d[2], 0, -d[0] );
                p += da;
                hit.point -= da;
            }
        }
    }
}

inline void OrderParticles::debug(const DebugInfo &di)
{
    fprintf(stderr, "\t[order-particles] symmetry = %d\n", symmetry);
    fprintf(stderr, "\t[order-particles] colorCycle = %f\n", colorCycle);
}

inline void OrderParticles::shader(Vec3& rgb, const PixelInfo& p) const
{
    // Metaball-style shading with lambertian diffuse lighting and an image-based color palette

    float intensity = sampleIntensity(p.point);
    Vec3 gradient = sampleIntensityGradient(p.point);
    float gradientMagnitude = len(gradient);
    Vec3 normal = gradientMagnitude ? (gradient / gradientMagnitude) : Vec3(0, 0, 0);
    float lambert = 0.4f * std::max(0.0f, dot(normal, lightVec));
    float ambient = 1.0f;

    rgb = (brightness * (ambient + lambert)) * 
        palette.sample(0.5 + 0.5 * sinf(colorCycle), intensity);
}