/*
 * Tell a story with some lights.
 *
 * (c) 2014 Micah Elizabeth Scott
 * http://creativecommons.org/licenses/by/3.0/
 */

#include "narrator.h"
#include "chaos_particles.h"
#include "order_particles.h"
#include "precursor.h"
#include "rings.h"
#include "ants.h"
#include "temporal_convolution.h"
#include "color_field.h"


int Narrator::script(int st, PRNG &prng)
{
    static ChaosParticles chaosParticles;
    static OrderParticles orderParticles;
    static Precursor precursor;
    static RingsEffect ringsA, ringsB;
    static Ants ants;
    static TemporalConvolution tconv;
    static ColorField field;

    switch (st) {

        default:
            return 10;

        case 10:
            // Order trying to form out of the tiniest sparks; runs for an unpredictable time, fails.
            precursor.reseed(prng.uniform32());
            crossfade(&precursor, 15);
            while (precursor.totalSecondsOfDarkness() < 6.0) {
                doFrame();
            }
            return 20;

        case 20:
            // Bang. Explosive energy, hints of self-organization
            chaosParticles.reseed(Vec3(0,0,0), prng.uniform32());
            mixer.set(&chaosParticles);
            while (chaosParticles.isRunning()) {
                doFrame();
            }
            return 30;

        case 30:
            // Textures of light
            ringsA.reseed();
            ringsA.palette.load("data/glass.png");
            crossfade(&ringsA, 10);
            delay(30);
            return 40;

        case 40:
            // Textures of energy
            ringsB.reseed();
            ringsB.palette.load("data/darkmatter-palette.png");
            crossfade(&ringsB, 10);
            delay(30);
            return 50;

        case 50:
            // Biology happens, order emerges
            orderParticles.reseed(prng.uniform32());
            orderParticles.vibration = 0.01;
            orderParticles.symmetry = 12;
            crossfade(&orderParticles, 4);
            while (orderParticles.symmetry > 1) {
                delay(5);
                orderParticles.symmetry--;
                orderParticles.vibration *= 0.5;
            }
            return 60;

        case 60:
            // Textures of biology
            ringsA.reseed();
            ringsA.palette.load("data/succulent-palette.png");
            crossfade(&ringsA, 15);
            delay(30);
            return 70;

        case 70:
            // Langton's ant
            ants.reseed(prng.uniform32());
            ants.stepSize = 0.5;
            crossfade(&ants, 10);
            delay(1);
            ants.stepSize = 0.1;
            delay(5);
            ants.stepSize = 0.05;
            delay(5);
            ants.stepSize = 0.025;
            delay(5);
            ants.stepSize = 0.0125;
            delay(5);
            ants.stepSize = 0.00625;
            return 80;

        case 80:
            // Sample and hold, capture a still-life from the noisy ant.
            tconv.setTap(&tap);
            tconv.setGaussian(8, 0.3, 0.0);
            crossfade(&tconv, 8);
            field.set(runner, tap);
            crossfade(&field, 2);
            delay(1000);
            return 70;
    }
}
