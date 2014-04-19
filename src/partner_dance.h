/*
 * Two entities, exploring ways to interact.
 *
 * (c) 2014 Micah Elizabeth Scott
 * http://creativecommons.org/licenses/by/3.0/
 */

#pragma once

#include <math.h>
#include <vector>
#include "lib/effect.h"
#include "lib/particle.h"
#include "lib/texture.h"
#include "lib/noise.h"


class PartnerDance : public ParticleEffect
{
public:
    PartnerDance();
    void reseed(uint32_t seed);

    virtual void beginFrame(const FrameInfo &f);
    virtual void shader(Vec3& rgb, const PixelInfo& p) const;
    virtual void debug(const DebugInfo& d);

    Texture palette;

    float targetGain;
    float targetSpin;
    float damping;
    float dampingRate;

private:
    static const unsigned particlesPerDancer = 20;
    static const unsigned numDancers = 2;
    static const unsigned numParticles = numDancers * particlesPerDancer;
    static const float stepSize = 1.0 / 200;

    static const float colorRate = 0.8;
    static const float noiseRate = 0.1;
    static const float radius = 0.5;
    static const float intensityScale = 60.0;
    static const float maxIntensity = 0.6;
    static const float targetRadius = 0.3;
    static const float jitterRate = 2.0;
    static const float jitterStrength = 0.3;
    static const float jitterScale = 1.2;
    static const float initialVelocity = 0.02;

    struct ParticleDynamics {
        Vec2 position;
        Vec2 velocity;
        Vec2 target;
    };

    std::vector<ParticleDynamics> dynamics;
    float timeDeltaRemainder;
    float noiseCycle;

    void resetParticle(ParticleDynamics &pd, PRNG &prng, unsigned dancer) const;
    void runStep(const FrameInfo &f);
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


inline PartnerDance::PartnerDance()
    : targetGain(0), targetSpin(0), damping(0), dampingRate(0),
      timeDeltaRemainder(0)
{
    // Sky at (0,0), lightness along +X, darkness along +Y. Fire encircles the void at (1,1)
    palette.load("data/beach-palette.png"),

    reseed(42);
}

inline void PartnerDance::reseed(uint32_t seed)
{
    PRNG prng;
    prng.seed(seed);

    noiseCycle = prng.uniform(0, 1000);

    appearance.resize(numParticles);
    dynamics.resize(numParticles);

    ParticleAppearance *pa = &appearance[0];
    ParticleDynamics *pd = &dynamics[0]; 

    for (unsigned dancer = 0; dancer < numDancers; dancer++) {
        for (unsigned i = 0; i < particlesPerDancer; i++, pa++, pd++) {

            pd->target = Vec2(0,0);

            resetParticle(*pd, prng, dancer);

            pa->radius = radius;
            pa->color = dancer ? Vec3(1, 0, 0) : Vec3(0, 1, 0);
        }
    }
}

inline void PartnerDance::beginFrame(const FrameInfo &f)
{    
    noiseCycle += f.timeDelta * noiseRate;

    damping += f.timeDelta * dampingRate;

    float t = f.timeDelta + timeDeltaRemainder;
    int steps = t / stepSize;
    timeDeltaRemainder = t - steps * stepSize;

    while (steps > 0) {
        runStep(f);
        steps--;
    }

    ParticleEffect::beginFrame(f);
}

inline void PartnerDance::debug(const DebugInfo& d)
{
    fprintf(stderr, "\t[partner-dance] noiseCycle = %f\n", noiseCycle);
    fprintf(stderr, "\t[partner-dance] damping = %f\n", damping);
}

inline void PartnerDance::runStep(const FrameInfo &f)
{
    ParticleAppearance *pa = &appearance[0];
    ParticleDynamics *pd = &dynamics[0]; 

    PRNG prng;
    prng.seed(42);

    for (unsigned dancer = 0; dancer < numDancers; dancer++) {
        for (unsigned i = 0; i < particlesPerDancer; i++, pa++, pd++) {

            prng.remix(pd->position[0] * 1e8);
            prng.remix(pd->position[1] * 1e8);

            Vec2 disparity = pd->target - pd->position;
            Vec2 normal = Vec2(disparity[1], -disparity[0]);
            Vec2 v = pd->velocity;
            v *= 1.0 - damping;
            v += disparity * targetGain;
            v += normal * targetSpin;

            pd->target[0] = targetRadius * fbm_noise2(noiseCycle, 0, 4);
            pd->target[1] = targetRadius * fbm_noise2(noiseCycle, 1, 4);

            pd->velocity = v;
            pd->position += v;

            pa->point = Vec3(pd->position[0], 0, pd->position[1]);
            pa->intensity = std::min(float(maxIntensity), intensityScale * len(v));

            if (pa->intensity < 1.0 / prng.uniform(40, 100)) {
                resetParticle(*pd, prng, dancer);
            }
        }
    }
}

inline void PartnerDance::resetParticle(ParticleDynamics &pd, PRNG &prng, unsigned dancer) const
{
    pd.velocity = prng.circularVector() * initialVelocity;
    pd.position = prng.circularVector() * 2.0 + (dancer ? Vec2(3, 0) : Vec2(-3, 0));
}

inline void PartnerDance::shader(Vec3& rgb, const PixelInfo& p) const
{
    Vec3 jitter = Vec3(
        fbm_noise3(noiseCycle * jitterRate, p.point[0] * jitterScale, p.point[2] * jitterScale, 2) * jitterStrength,
        fbm_noise3(noiseCycle * jitterRate, p.point[0] * jitterScale, p.point[2] * jitterScale, 2) * jitterStrength,
        0);

    // Use 'color' to encode contributions from both partners
    Vec3 c = sampleColor(p.point) + jitter;

    // 2-dimensional palette lookup
    rgb = palette.sample(c[0], c[1]);
}
