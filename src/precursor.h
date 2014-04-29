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
#include "lib/effect.h"
#include "lib/prng.h"
#include "lib/texture.h"
#include "lib/noise.h"
#include "lib/camera_flow.h"
#include "lib/rapidjson/document.h"
#include "tree_growth.h"


class Precursor : public Effect
{
public:
    Precursor(const CameraFlowAnalyzer &flow, const rapidjson::Value &config);
    void reseed(unsigned seed);

    virtual void beginFrame(const FrameInfo &f);
    virtual void shader(Vec3& rgb, const PixelInfo &p) const;
    virtual void debug(const DebugInfo &di);

    bool isDone();

    TreeGrowth treeGrowth;

private:
    float flowScale;
    float flowFilterRate;
    float noiseRate;
    float noiseScale;
    float noiseDepth;
    float brightness;
    float darknessDurationMin;
    float darknessDurationMax;

    CameraFlowCapture flow;
    Texture palette;

    float noiseCycle;
    float colorSeed;
    float darknessDuration;
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


inline Precursor::Precursor(const CameraFlowAnalyzer& flow, const rapidjson::Value &config)
    : treeGrowth(flow, config["treeGrowth"]),
      flowScale(config["flowScale"].GetDouble()),
      flowFilterRate(config["flowFilterRate"].GetDouble()),
      noiseRate(config["noiseRate"].GetDouble()),
      noiseScale(config["noiseScale"].GetDouble()),
      noiseDepth(config["noiseDepth"].GetDouble()),
      brightness(config["brightness"].GetDouble()),
      darknessDurationMin(config["darknessDurationMin"].GetDouble()),
      darknessDurationMax(config["darknessDurationMax"].GetDouble()),
      flow(flow),
      palette(config["palette"].GetString())
{
    reseed(42);
}

inline void Precursor::reseed(unsigned seed)
{
    flow.capture(1.0);
    flow.origin();

    PRNG prng;
    prng.seed(seed);

    noiseCycle = prng.uniform(0, 10);
    colorSeed = prng.uniform(2, 5);
    darknessDuration = prng.uniform(darknessDurationMin, darknessDurationMax);
}

inline void Precursor::beginFrame(const FrameInfo &f)
{
    flow.capture(flowFilterRate);
    treeGrowth.beginFrame(f);
    noiseCycle += f.timeDelta * noiseRate;
}

inline void Precursor::debug(const DebugInfo &di)
{
    treeGrowth.debug(di);
    fprintf(stderr, "\t[precursor] noiseCycle = %f\n", noiseCycle);
    fprintf(stderr, "\t[precursor] colorSeed = %f\n", colorSeed);
    fprintf(stderr, "\t[precursor] darknessDuration = %f\n", darknessDuration);
}

inline bool Precursor::isDone()
{
    return treeGrowth.darknessDuration >= darknessDuration;
}

inline void Precursor::shader(Vec3& rgb, const PixelInfo &p) const
{
    float n = 1.5 + noiseDepth * fbm_noise3(
        XZ(p.getVec2("gridXY") * noiseScale)
        + flow.model * flowScale
        + Vec3(0, noiseCycle, 0), 4);

    // Lissajous sampling on palette
    rgb = treeGrowth.sampleIntensity(p.point) * brightness *
          palette.sample(0.5 + 0.5 * cos(n),
                         0.5 + 0.5 * sin(n * colorSeed));
}

