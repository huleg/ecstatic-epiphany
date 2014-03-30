/*
 * Tell a story with some lights.
 *
 * (c) 2014 Micah Elizabeth Scott
 * http://creativecommons.org/licenses/by/3.0/
 */

#include "narrator.h"


Narrator::Narrator()
{
    tap.setEffect(&mixer);
    runner.setEffect(&tap);
}

void Narrator::run()
{
    PRNG prng;
    prng.seed(time(0));

    while (true) {
        loop(prng);
    }
}

float Narrator::doFrame()
{
    return runner.doFrame();
}

void Narrator::loop(PRNG &prng)
{
    // Order trying to form out of the tiniest sparks; runs for a while, fails.
    precursor.reseed(prng.uniform32());
    mixer.set(&precursor);
    while (precursor.totalSecondsOfDarkness() < 6.0) {
        doFrame();
    }

    // Bang.
    chaosParticles.reseed(Vec3(0,0,0), prng.uniform32());
    mixer.set(&chaosParticles);
    while (chaosParticles.isRunning()) {
        doFrame();
    }

    // Textures of light
    rings.reseed();
    rings.palette.load("data/glass.png");
    mixer.set(&rings);
    for (float t = 0; t < 1; t += doFrame() / 100.0f) {
        mixer.setFader(0, sinf(t * M_PI));
    }

    // Textures of energy
    rings.reseed();
    rings.palette.load("data/darkmatter-palette.png");
    mixer.set(&rings);
    for (float t = 0; t < 1; t += doFrame() / 100.0f) {
        mixer.setFader(0, sinf(t * M_PI));
    }

    // Biology happens, order emerges
    orderParticles.reseed(prng.uniform32());
    mixer.set(&orderParticles);
    for (float t = 0; t < 1; t += doFrame() / 100.0f) {
        orderParticles.vibration = 0.02 / (1.0 + t * 20.0);
        orderParticles.symmetry = 2 + (1 - t) * 12;
        mixer.setFader(0, sinf(t * M_PI));
    }

    // Textures of biology
    rings.reseed();
    rings.palette.load("data/succulent-palette.png");
    mixer.set(&rings);
    for (float t = 0; t < 1; t += doFrame() / 100.0f) {
        mixer.setFader(0, sinf(t * M_PI));
    }
}
