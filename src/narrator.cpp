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

void Narrator::crossfade(Effect *to, float duration)
{
    int n = mixer.numChannels();
    if (n > 0) {
        mixer.add(to);
        for (float t = 0; t < duration; t += doFrame()) {
            float q = t / duration;
            for (int i = 0; i < n; i++) {
                mixer.setFader(i, 1 - q);
            }
            mixer.setFader(n, q);
        }
    }
    mixer.set(to);
}

void Narrator::delay(float seconds)
{
    while (seconds > 0) {
        seconds -= doFrame();
    }
}

void Narrator::loop(PRNG &prng)
{
    // Order trying to form out of the tiniest sparks; runs for a while, fails.
    precursor.reseed(prng.uniform32());
    crossfade(&precursor, 15);
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
    ringsA.reseed();
    ringsA.palette.load("data/glass.png");
    crossfade(&ringsA, 10);
    delay(30);

    // Textures of energy
    ringsB.reseed();
    ringsB.palette.load("data/darkmatter-palette.png");
    crossfade(&ringsB, 10);
    delay(30);

    // Biology happens, order emerges
    orderParticles.reseed(prng.uniform32());
    orderParticles.vibration = 0.01;
    orderParticles.symmetry = 12;
    crossfade(&orderParticles, 15);
    while (orderParticles.symmetry > 1) {
        delay(5);
        orderParticles.symmetry--;
        orderParticles.vibration *= 0.5;
    }

    // Textures of biology
    ringsA.reseed();
    ringsA.palette.load("data/succulent-palette.png");
    crossfade(&ringsA, 20);
    delay(30);
}
