/*
 * Hints of potential, before the bang.
 * Fleeting, square by square.
 * Tastes like automata.
 *
 * (c) 2014 Micah Elizabeth Scott
 * http://creativecommons.org/licenses/by/3.0/
 */

#pragma once

#include <vector>
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

private:
    static const float speed = 0.8;
    static const float potentialTarget = 1e-6;
    static const float potentialGain = 0.05;
    static const float maxBrightness = 2.0;

    struct PixelState {
        PixelState();

        float timeAxis;         // Progresses from 0 to 1
        float identityAxis;     // Random in [0, 1], or negative for uninitialized
        float potential;        // Probability to restart
    };

    std::vector<PixelState> pixelState;
    Texture texture;
    unsigned seed;
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


inline Precursor::Precursor()
    : texture("data/glass.png"), seed(0)
{
    reseed();

}

inline void Precursor::reseed()
{
    pixelState.clear();
    seed ^= 0xFFFF0000;
}

// Pixels start out dead and without identity
inline Precursor::PixelState::PixelState()
    : timeAxis(1), identityAxis(-1), potential(0)
{}

inline void Precursor::beginFrame(const FrameInfo &f)
{
    PRNG prng;
    prng.seed(seed);
    seed++;

    pixelState.resize(f.pixels.size());

    for (unsigned i = 0; i < pixelState.size(); i++) {
        PixelState &pix = pixelState[i];

        if (pix.identityAxis < 0) {
            pix.identityAxis = prng.uniform();
        }

        if (prng.uniform() < pix.potential) {
            // Restart
            pix.timeAxis = 0;
        }

        // Linear progression
        pix.timeAxis = std::min(1.0f, pix.timeAxis + f.timeDelta * speed);

        // Approach background potential
        pix.potential += (potentialTarget - pix.potential) * potentialGain;
    }
}

inline void Precursor::shader(Vec3& rgb, const PixelInfo &p) const
{
    const PixelState &pix = pixelState[p.index];

    // Fade in and out
    float br = maxBrightness * sq(std::max(0.0f, sinf(pix.timeAxis * M_PI)));

    // Lissajous sampling on texture
    float t = pix.timeAxis * 0.1;
    float u = pix.identityAxis * 10.0;
    rgb = texture.sample(0.5 + 0.5 * cos(t), 0.5 + 0.5 * sin(t*u)) * br;
}
