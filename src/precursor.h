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
#include "lib/prng.h"
#include "lib/texture.h"


class Precursor : public Effect
{
public:
    Precursor();
    void reseed();

    virtual void beginFrame(const FrameInfo &f);
    virtual void shader(Vec3& rgb, const PixelInfo &p) const;
    virtual void debug(const DebugInfo &di);

private:
    static const float stepSize = 0.001;
    static const float cycleRate = stepSize / (60 * 5);
    static const float speed = stepSize * 1.0;
    static const float potentialBackground = 5e-7;
    static const float potentialSettle = 0.15;
    static const float potentialTransfer = 0.1;
    static const float maxBrightness = 1.8;
    static const float maxPropagationDistance = 0.1;
    static const float maxPropagationRate = 0.05;

    struct PixelState {
        PixelState();

        // State
        float timeAxis;         // Progresses from 0 to 1
        float potential;        // Probability to restart
        float energy;           // Resource needed to restart
        unsigned generation;

        // Calculated
        float strength;         // From timeAxis
    };

    std::vector<PixelState> pixelState;
    Texture palette;
    unsigned seed;
    float cycle;
    float timeDeltaRemainder;

    // Calculated
    float propagationRate;
    float energyRate;

    void runStep(GridStructure &grid, const FrameInfo &f);
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


inline Precursor::Precursor()
    : palette("data/darkmatter-palette.png"), seed(0), timeDeltaRemainder(0)
{
    reseed();

}

inline void Precursor::reseed()
{
    pixelState.clear();
    seed ^= 0xFFFF0000;
    cycle = 0;
}

inline Precursor::PixelState::PixelState()
    : timeAxis(1), potential(potentialBackground), energy(1), generation(0)
{}

inline void Precursor::beginFrame(const FrameInfo &f)
{
    GridStructure grid;
    grid.init(f.pixels);

    pixelState.resize(f.pixels.size());

    // Fixed timestep
    float t = f.timeDelta + timeDeltaRemainder;
    int steps = t / stepSize;
    timeDeltaRemainder = t - steps * stepSize;

    while (steps > 0) {
        runStep(grid, f);
        steps--;
    }
}

inline void Precursor::runStep(GridStructure &grid, const FrameInfo &f)
{
    PRNG prng;
    prng.seed(seed);
    seed++;

    // Propagation rate varies over time, to give it an arc
    cycle = fmod(cycle + cycleRate, 1.0f);
    propagationRate = sinf(cycle * M_PI) * maxPropagationRate;
    energyRate = propagationRate * 2.0;

    for (unsigned i = 0; i < pixelState.size(); i++) {
        if (!f.pixels[i].isMapped()) {
            continue;
        }

        PixelState &pix = pixelState[i];

        if (pix.timeAxis == 1.0f && pix.energy == 1.0f && prng.uniform() < pix.potential) {
            // Restart
            pix.timeAxis = 0;
            pix.energy = 0;
            pix.generation++;

            // Approach background potential
            pix.potential += (potentialBackground - pix.potential) * potentialSettle;
        }

        // Linear progression
        pix.timeAxis = std::min(1.0f, pix.timeAxis + speed);

        // Energy refill
        pix.energy = std::min(1.0f, pix.energy + energyRate * speed);

        // Strength curve, fade in and out
        pix.strength = sq(std::max(0.0f, sinf(pix.timeAxis * M_PI)));

        // Propagation
        if (prng.uniform() < pix.strength * propagationRate) {
            const GridStructure::PixelIndexSet *set = NULL;

            // Direction, pick a target set. Low probability of jumping grid squares
            switch (prng.uniform32() & 63) {
                case 0:  set = &grid.coordIndex[0][f.pixels[i].point[0]]; break;
                case 1:  set = &grid.coordIndex[2][f.pixels[i].point[2]]; break;
                default: set = &grid.gridIndex[ GridStructure::intGridXY(f.pixels[i]) ]; break;
            };

            // Pick closest among target set
            GridStructure::PixelIndexSet::const_iterator iter;
            int best = -1;
            float bestDistSqr = sq(maxPropagationDistance);

            for (iter = set->begin(); iter != set->end(); iter++) {
                const PixelInfo &other = f.pixels[*iter];
                PixelState &oPix = pixelState[*iter];

                float distSqr = sqrlen(other.point - f.pixels[i].point);
                if (distSqr > sq(maxPropagationDistance)) {
                    continue;
                }

                // Might be closest
                if (distSqr < bestDistSqr && *iter != i && oPix.timeAxis == 1.0f) {
                    bestDistSqr = distSqr;
                    best = *iter;
                }
            }

            // Transfer potential
            if (best >= 0) {
                pixelState[best].potential += potentialTransfer;
            }
        }
    }
}

inline void Precursor::shader(Vec3& rgb, const PixelInfo &p) const
{
    const PixelState &pix = pixelState[p.index];

    // Lissajous sampling on palette
    float t = (pix.generation + pix.timeAxis) * 0.1;
    float u = 7.3f;
    rgb = palette.sample(0.5 + 0.5 * cos(t),
                         0.5 + 0.5 * sin(t*u))
        * (maxBrightness * pix.strength);
}

inline void Precursor::debug(const DebugInfo &di)
{
    fprintf(stderr, "\t[precursor] cycle = %f\n", cycle);
    fprintf(stderr, "\t[precursor] propagationRate = %f\n", propagationRate);
    fprintf(stderr, "\t[precursor] seed = 0x%08x\n", seed);
}
