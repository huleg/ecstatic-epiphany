/*
 * Hints of potential, before the bang.
 * Fleeting, square by square.
 * Reaction-diffusion. Precursors to life.
 *
 * (c) 2014 Micah Elizabeth Scott
 * http://creativecommons.org/licenses/by/3.0/
 */

#pragma once

#include <vector>
#include "grid_structure.h" 
#include "lib/effect.h"
#include "lib/particle.h"
#include "lib/prng.h"
#include "lib/texture.h"
#include "lib/noise.h"
#include "lib/camera_flow.h"



class Precursor : public ParticleEffect
{
public:
    Precursor(const CameraFlowAnalyzer &flow);
    void reseed(unsigned seed);

    virtual void beginFrame(const FrameInfo &f);
    virtual void shader(Vec3& rgb, const PixelInfo &p) const;
    virtual void debug(const DebugInfo &di);

    float totalSecondsOfDarkness();

private:
    static constexpr float launchProbabilityBaseline = 0.004;
    static constexpr float launchProbabilityScale = 0.002;
    static constexpr float flowScale = 0.04;
    static constexpr float stepRate = 200.0;
    static constexpr float noiseRate = 0.03;
    static constexpr float brightness = 2.5;
    static constexpr float particleDuration = 4.0;
    static constexpr float ledPull = 0.007;
    static constexpr float ledPullRadius = 0.04;
    static constexpr float blockPull = 0.000001;
    static constexpr float blockPullRadius = 0.2;
    static constexpr float damping = 0.00005;
    static constexpr float visibleRadius = 0.08;

    struct ParticleDynamics {
        Vec3 velocity;
        float time;
    };

    CameraFlowCapture flow;

    std::vector<ParticleDynamics> dynamics;
    Texture palette;
    PRNG prng;
    unsigned darkStepCount;
    float noiseCycle;
    float timeDeltaRemainder;
    float colorSeed;

    void runStep(GridStructure &grid, const FrameInfo &f);
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


inline Precursor::Precursor(const CameraFlowAnalyzer& flow)
    : flow(flow),
      palette("data/darkmatter-palette.png"),
      timeDeltaRemainder(0)
{
    reseed(42);
}

inline void Precursor::reseed(unsigned seed)
{
    flow.capture();
    flow.origin();
    prng.seed(seed);
    noiseCycle = 0;
    darkStepCount = 0;
    colorSeed = prng.uniform(2, 5);
}

inline float Precursor::totalSecondsOfDarkness()
{
    return darkStepCount / stepRate;
}

inline void Precursor::beginFrame(const FrameInfo &f)
{
    // Capture impulse, transfer to all particles
    flow.capture(1.0);
    flow.origin();
    for (unsigned i = 0; i < appearance.size(); i++) {
        appearance[i].point += flow.model * flowScale;
    }

    GridStructure grid;
    grid.init(f.pixels);

    // Simple time-varying parameters
    noiseCycle += f.timeDelta * noiseRate;

    // Fixed timestep
    float t = f.timeDelta + timeDeltaRemainder;
    int steps = t * stepRate;
    timeDeltaRemainder = t - steps / stepRate;

    while (steps > 0) {
        runStep(grid, f);
        steps--;
    }

    ParticleEffect::beginFrame(f);
}

inline void Precursor::debug(const DebugInfo &di)
{
    fprintf(stderr, "\t[precursor] particles = %d\n", (int)appearance.size());
    fprintf(stderr, "\t[precursor] motionLength = %f\n", flow.motionLength);
    fprintf(stderr, "\t[precursor] noiseCycle = %f\n", noiseCycle);
    fprintf(stderr, "\t[precursor] darkness = %f sec\n", totalSecondsOfDarkness());
}

inline void Precursor::runStep(GridStructure &grid, const FrameInfo &f)
{
    // Launch new particles
    float launchProbability = launchProbabilityBaseline + flow.motionLength * launchProbabilityScale;
    unsigned launchCount = prng.uniform(0, 1.0f + launchProbability);
    for (unsigned i = 0; i < launchCount; i++) {
        ParticleAppearance pa;
        ParticleDynamics pd;

        pa.point = Vec3( prng.uniform(f.modelMin[0], f.modelMax[0]),
                         prng.uniform(f.modelMin[1], f.modelMax[1]),
                         prng.uniform(f.modelMin[2], f.modelMax[2]) );

        pd.velocity = Vec3(0, 0, 0);
        pd.time = 0;

        appearance.push_back(pa);
        dynamics.push_back(pd);
    }

    // Iterate through and update all particles, discarding any that have expired
    unsigned j = 0;
    for (unsigned i = 0; i < appearance.size(); i++) {
        ParticleAppearance pa = appearance[i];
        ParticleDynamics pd = dynamics[i];

        // Update simulation

        pa.point += pd.velocity;
        pd.time += 1.0f / stepRate / particleDuration;

        if (pd.time >= 1.0f) {
            // Discard
            continue;
        }

        pa.intensity = sq(std::max(0.0f, sinf(pd.time * M_PI)));
        pa.radius = visibleRadius;

        // Pull toward nearby LEDs, so the particles kinda-follow the grid

        ResultSet_t hits;
        f.radiusSearch(hits, pa.point, blockPullRadius);
        for (unsigned h = 0; h < hits.size(); h++) {
            const PixelInfo &hit = f.pixels[hits[h].first];
            if (!hit.isMapped()) {
                continue;
            }

            Vec3 v = pd.velocity * (1.0f - damping);

            // Pull toward LED
            float q2 = hits[h].second / sq(ledPullRadius);
            if (q2 < 1.0f) {
                v += ledPull * kernel2(q2) * (hit.point - pa.point);
            }

            // Pull toward grid square center
            q2 = hits[h].second / sq(blockPullRadius);
            if (q2 < 1.0f) {
                Vec2 blockXY = hit.getVec2("blockXY");
                Vec2 b = blockPull * kernel2(q2) * blockXY;
                v += Vec3(-b[0], 0, b[1]);
            }

            pd.velocity = v;
        }

        // Write out
        appearance[j] = pa;
        dynamics[j] = pd;
        j++;
    }

    appearance.resize(j);
    dynamics.resize(j);

    if (appearance.empty()) {
        darkStepCount++;
    }
}

inline void Precursor::shader(Vec3& rgb, const PixelInfo &p) const
{
    // Sample noise with grid square granularity
    float n = 1.5 + fbm_noise2(p.getVec2("gridXY") * 0.3 + Vec2(noiseCycle, 0), 2);

    // Lissajous sampling on palette
    rgb = sampleIntensity(p.point) * brightness *
          palette.sample(0.5 + 0.5 * cos(n),
                         0.5 + 0.5 * sin(n * colorSeed));
}

