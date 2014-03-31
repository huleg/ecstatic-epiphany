/*
 * Tell a story with some lights.
 *
 * (c) 2014 Micah Elizabeth Scott
 * http://creativecommons.org/licenses/by/3.0/
 */

#pragma once

#include "lib/effect.h"
#include "lib/effect_mixer.h"
#include "lib/effect_tap.h"
#include "lib/prng.h"

#include "visual_memory.h"
#include "rings.h"
#include "chaos_particles.h"
#include "order_particles.h"
#include "precursor.h"
#include "spokes.h"
#include "automata.h"


class Narrator
{
public:
    Narrator();

    void run();

    VisualMemory vismem;
    EffectTap tap;
    EffectRunner runner;
    EffectMixer mixer;

    ChaosParticles chaosParticles;
    OrderParticles orderParticles;
    Precursor precursor;
    RingsEffect ringsA, ringsB;
    Automata automata;

private:
    void loop(PRNG &prng);
    float doFrame();
    void crossfade(Effect *to, float duration);
    void delay(float seconds);
};
