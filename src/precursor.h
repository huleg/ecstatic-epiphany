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
#include "lib/rapidjson/document.h"


class Precursor : public ParticleEffect
{
public:
    Precursor(const CameraFlowAnalyzer &flow, const rapidjson::Value &config);
    void reseed(unsigned seed);

    virtual void beginFrame(const FrameInfo &f);
    virtual void shader(Vec3& rgb, const PixelInfo &p) const;
    virtual void debug(const DebugInfo &di);

    float totalSecondsOfDarkness();

private:
    unsigned maxParticles;
    float launchProbabilityBaseline;
    float launchProbabilityGain;
    float launchProbabilityDecay;
    float flowScale;
    float stepRate;
    float noiseRate;
    float noiseScale;
    float brightness;
    float particleDuration;
    float particleRampup;
    float ledPull;
    float ledPullRadius;
    float blockPull;
    float blockPullRadius;
    float damping;
    float visibleRadius;
    float outsideMargin;

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
    float launchProbability;

    void runStep(GridStructure &grid, const FrameInfo &f);
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


inline Precursor::Precursor(const CameraFlowAnalyzer& flow, const rapidjson::Value &config)
    : maxParticles(config["maxParticles"].GetUint()),
      launchProbabilityBaseline(config["launchProbabilityBaseline"].GetDouble()),
      launchProbabilityGain(config["launchProbabilityGain"].GetDouble()),
      launchProbabilityDecay(config["launchProbabilityDecay"].GetDouble()),
      flowScale(config["flowScale"].GetDouble()),
      stepRate(config["stepRate"].GetDouble()),
      noiseRate(config["noiseRate"].GetDouble()),
      noiseScale(config["noiseScale"].GetDouble()),
      brightness(config["brightness"].GetDouble()),
      particleDuration(config["particleDuration"].GetDouble()),
      particleRampup(config["particleRampup"].GetDouble()),
      ledPull(config["ledPull"].GetDouble()),
      ledPullRadius(config["ledPullRadius"].GetDouble()),
      blockPull(config["blockPull"].GetDouble()),
      blockPullRadius(config["blockPullRadius"].GetDouble()),
      damping(config["damping"].GetDouble()),
      visibleRadius(config["visibleRadius"].GetDouble()),
      outsideMargin(config["outsideMargin"].GetDouble()),
      flow(flow),
      palette(config["palette"].GetString()),
      timeDeltaRemainder(0)
{
    reseed(42);
}

inline void Precursor::reseed(unsigned seed)
{
    flow.capture(1.0);
    flow.origin();

    prng.seed(seed);
    noiseCycle = 0;
    darkStepCount = 0;
    launchProbability = 0;
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
        dynamics[i].velocity += flow.model * flowScale;
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
    fprintf(stderr, "\t[precursor] launchProbability = %f\n", launchProbability);
    fprintf(stderr, "\t[precursor] darkness = %f sec\n", totalSecondsOfDarkness());
}

inline void Precursor::runStep(GridStructure &grid, const FrameInfo &f)
{
    // Randomly launch particles according to our flow motion total
    launchProbability += sq(flow.motionLength) * launchProbabilityGain;
    launchProbability += (launchProbabilityBaseline - launchProbability) * launchProbabilityDecay;
    unsigned launchCount = std::min<int>(maxParticles - appearance.size(), prng.uniform(0, 1.0f + launchProbability));

    // Launch new particles
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
            // Discard, too old
            continue;
        }

        if (f.distanceOutsideBoundingBox(pa.point) > outsideMargin * visibleRadius) {
            // Outside bounding box, discard
            continue;
        }

        // Ramp up, then fade
        pa.intensity = kernel(pd.time) * kernel(1.0f - std::min(1.0f, pd.time / particleRampup));

        pa.radius = visibleRadius;

        // Pull toward nearby LEDs, so the particles kinda-follow the grid.
        // We only pull if the direction matches where we're already headed.

        ResultSet_t hits;
        f.radiusSearch(hits, pa.point, blockPullRadius);
        for (unsigned h = 0; h < hits.size(); h++) {
            const PixelInfo &hit = f.pixels[hits[h].first];
            if (!hit.isMapped()) {
                continue;
            }

            Vec3 v = pd.velocity * (1.0f - damping);

            // Pull toward LED
            Vec3 d = hit.point - pa.point;
            float q2 = hits[h].second / sq(ledPullRadius);
            if (q2 < 1.0f && dot(d, v) > 0.0f) {
                v += ledPull * kernel2(q2) * d;
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
    float n = 1.5 + fbm_noise2(p.getVec2("gridXY") * noiseScale + Vec2(noiseCycle, 0), 2);

    // Lissajous sampling on palette
    rgb = sampleIntensity(p.point) * brightness *
          palette.sample(0.5 + 0.5 * cos(n),
                         0.5 + 0.5 * sin(n * colorSeed));
}

