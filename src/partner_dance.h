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
    PartnerDance(CameraFlowAnalyzer& flow);
    void reseed(uint32_t seed);

    virtual void beginFrame(const FrameInfo &f);
    virtual void shader(Vec3& rgb, const PixelInfo& p) const;
    virtual void debug(const DebugInfo& d);

    Texture palette;

    float targetGain;
    float targetSpin;
    float damping;
    float dampingRate;
    float interactionRate;

private:
    static constexpr unsigned particlesPerDancer = 30;
    static constexpr unsigned numDancers = 2;
    static constexpr unsigned numParticles = numDancers * particlesPerDancer;
    static constexpr float stepSize = 1.0 / 300;

    static constexpr float colorRate = 0.8;
    static constexpr float noiseRate = 0.4;
    static constexpr float radius = 0.9;
    static constexpr float radiusScale = 0.3;
    static constexpr float intensityScale = 20.0;
    static constexpr float maxIntensity = 0.3;
    static constexpr float minIntensity = 0.01;
    static constexpr float targetRadius = 0.5;
    static constexpr float interactionRadius = 0.4;
    static constexpr float jitterRate = 0.66;
    static constexpr float jitterStrength = 0.6;
    static constexpr float jitterScale = 0.7;

    struct ParticleDynamics {
        Vec2 position;
        Vec2 velocity;
    };

    CameraFlowCapture flow;

    std::vector<ParticleDynamics> dynamics;
    float timeDeltaRemainder;
    float noiseCycle;
    Vec2 target;

    void resetParticle(ParticleDynamics &pd, PRNG &prng, unsigned dancer) const;
    void runStep(const FrameInfo &f);
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


inline PartnerDance::PartnerDance(CameraFlowAnalyzer& flow)
    : targetGain(0),
      targetSpin(0),
      damping(0),
      dampingRate(0),
      interactionRate(0),
      flow(flow),
      timeDeltaRemainder(0)
{
    // Sky at (0,0), lightness along +X, darkness along +Y. Fire encircles the void at (1,1)
    palette.load("data/foggy-bay-palette.png"),

    reseed(42);
}

inline void PartnerDance::reseed(uint32_t seed)
{
    flow.capture();
    flow.origin();

    PRNG prng;
    prng.seed(seed);

    noiseCycle = prng.uniform(0, 1000);

    appearance.resize(numParticles);
    dynamics.resize(numParticles);

    ParticleAppearance *pa = &appearance[0];
    ParticleDynamics *pd = &dynamics[0]; 

    for (unsigned dancer = 0; dancer < numDancers; dancer++) {
        for (unsigned i = 0; i < particlesPerDancer; i++, pa++, pd++) {

            resetParticle(*pd, prng, dancer);

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

    // Update orbit angles
    float angle1 = noiseCycle;
    float angle2 = noiseCycle * 0.2f;
    float angle3 = noiseCycle * 0.7f;

    target[0] = targetRadius * sinf(angle1);
    target[1] = targetRadius * cosf(angle1 + sin(angle2));

    // Update all particle radii
    float r = radius + radiusScale * sinf(angle3);
    for (unsigned i = 0; i < appearance.size(); i++) {
        appearance[i].radius = r;
    }

    // Build first index
    ParticleEffect::beginFrame(f);

    while (steps > 0) {
        runStep(f);
        steps--;

        // Rebuild index
        ParticleEffect::beginFrame(f);
    }
}

inline void PartnerDance::debug(const DebugInfo& d)
{
    fprintf(stderr, "\t[partner-dance] numParticles = %d\n", numParticles);
    fprintf(stderr, "\t[partner-dance] radius = %f\n", appearance[0].radius);
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

            Vec2 disparity = target - pd->position;
            Vec2 normal = Vec2(disparity[1], -disparity[0]);
            Vec2 v = pd->velocity;

            // Single-particle velocity updates
            v *= 1.0 - damping;
            v += disparity * targetGain;
            v += normal * targetSpin;

            // Particle interactions
            std::vector<std::pair<size_t, Real> > hits;
            nanoflann::SearchParams params;
            params.sorted = false;
            float searchRadius2 = sq(interactionRadius);
            index.tree.radiusSearch(&pa->point[0], searchRadius2, hits, params);

            for (unsigned i = 0; i < hits.size(); i++) {
                unsigned hitDancer = hits[i].first / particlesPerDancer;
                if (hitDancer == dancer) {
                    // Only interact with other dancers
                    continue;
                }

                // Check distance
                float q2 = hits[i].second / searchRadius2;
                if (q2 >= 1.0f) {
                    continue;
                }
                float k = dancer ? kernel2(q2) : -kernel(q2);

                // Spin force, normal to the angle between us
                ParticleDynamics &other = dynamics[hits[i].first];
                Vec2 d = other.position - pd->position;
                Vec2 normal = Vec2(d[1], -d[0]);
                normal /= len(normal);

                v += interactionRate * k * normal;
            }

            pd->velocity = v;
            pd->position += v;

            pa->point = Vec3(pd->position[0], 0, pd->position[1]);
            pa->intensity = std::min(float(maxIntensity), intensityScale * len(v));

            if (pa->intensity < minIntensity) {
                resetParticle(*pd, prng, dancer);
            }
        }
    }
}

inline void PartnerDance::resetParticle(ParticleDynamics &pd, PRNG &prng, unsigned dancer) const
{
    pd.velocity = Vec2(0, 0);
    pd.position = prng.circularVector() * 7.0 + (dancer ? Vec2(5, 0) : Vec2(-5, 0));
}

inline void PartnerDance::shader(Vec3& rgb, const PixelInfo& p) const
{
    Vec3 jitter = Vec3(
        fbm_noise3(noiseCycle * jitterRate, p.point[0] * jitterScale, p.point[2] * jitterScale, 4) * jitterStrength,
        fbm_noise3(noiseCycle * jitterRate, p.point[0] * jitterScale, p.point[2] * jitterScale, 4) * jitterStrength,
        0);

    // Use 'color' to encode contributions from both partners
    Vec3 c = sampleColor(p.point) + jitter;

    // 2-dimensional palette lookup
    rgb = palette.sample(c[0], c[1]);
}
