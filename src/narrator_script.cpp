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
#include "planar_waves.h"


int Narrator::script(int st, PRNG &prng)
{
    static ChaosParticles chaosParticles;
    static OrderParticles orderParticles;
    static Precursor precursor;
    static RingsEffect ringsA, ringsB;
    static Ants ants;
    static TemporalConvolution temporalConvolution;
    static PlanarWaves planarWaves;

    switch (st) {

        default:
            return 10;

        case 10:
            // Order trying to form out of the tiniest sparks; runs for an unpredictable time, fails.
            precursor.reseed(prng.uniform32());
            crossfade(&precursor, 15);
            do { doFrame(); } while (precursor.totalSecondsOfDarkness() < 6.1);

            // Make sure we get a little bit of quiet before the bang
            mixer.setFader(0, 0.0f);
            delay(1.0);
            return 20;

        case 20:
            // Bang. Explosive energy, hints of self-organization

            mixer.set(&chaosParticles);
            for (int i = 0; i < prng.uniform(1, 4); i++) {
                chaosParticles.reseed(prng.circularVector() * 0.5, prng.uniform32());
                delay((1 << i) * 0.25f);
            }

            // Run for at least 20 secs, to bootstrap
            delay(20);

            // Keep going as long as it's bright, then crossfade
            do { doFrame(); } while (!(chaosParticles.getTotalIntensity() < 200));
            return 30;

        case 30:
            // Textures of light, slow crossfade in
            ringsA.reseed();
            ringsA.palette.load("data/glass.png");
            crossfade(&ringsA, 20);
            delay(prng.uniform(60*1, 60*2));
            return 40;

        case 40:
            // Textures of energy, slow crossfade in
            ringsB.reseed();
            ringsB.palette.load("data/darkmatter-palette.png");
            crossfade(&ringsB, prng.uniform(30, 90));
            delay(prng.uniform(60*1, 60*3));
            return 50;

        case 50:
            // Biology happens, order emerges
            orderParticles.reseed(prng.uniform32());
            orderParticles.vibration = 0.001;
            orderParticles.symmetry = 16;
            crossfade(&orderParticles, 15);

            // Run until we have square grid symmetry
            while (orderParticles.symmetry > 4) {
                delay(prng.uniform(2, 15));
                orderParticles.symmetry--;
            }
            delay(prng.uniform(2, 15));
            return 60;

        case 60:
            // Langton's ant
            ants.reseed(prng.uniform32());
            ants.stepSize = 0.5;
            crossfade(&ants, prng.uniform(5, 20));
            delay(prng.uniform(3, 6));
            ants.stepSize = 0.1;
            delay(prng.uniform(15, 45));
            ants.stepSize = 0.05;
            delay(prng.uniform(15, 45));
            ants.stepSize = 0.025;
            delay(2);
            ants.stepSize = 0.0125;
            delay(2);
            ants.stepSize = 0.00625;
            return 70;

        case 70:
            // Fast ant slowly crossfades into rings with same palette
            ringsA.reseed();
            ringsA.palette.load("data/succulent-palette.png");
            crossfade(&ringsA, prng.uniform(15, 30));
            delay(prng.uniform(60*1, 60*3));
            return 80;

#if 0
        case 80:
            // Blank canvas for wave patterns
            planarWaves.reset();
            planarWaves.waves.resize(4);
            crossfade(&planarWaves, 5);
            delay(2);

            // Simple start; one wave pulses
            planarWaves.waves[0].frequency = 2.0;
            planarWaves.waves[0].set();
            planarWaves.waves[0].amplitude = 0.05;
            delay(2);

            // Perpendicular wave pulses
            planarWaves.waves[1].frequency = 2.0;
            planarWaves.waves[1].angle = M_PI/2;
            planarWaves.waves[1].set();
            planarWaves.waves[1].amplitude = 0.05;
            delay(2);

            // In proximity
            planarWaves.waves[0].amplitude = 0.05;
            delay(0.1);
            planarWaves.waves[1].amplitude = 0.05;
            delay(2);

            // Both stick on
            planarWaves.waves[0].targetAmplitude = 0.1;
            planarWaves.waves[1].targetAmplitude = 0.1;
            delay(2);

            // Zoom out, adjust colors
            planarWaves.waves[0].targetFrequency = 3.0;
            planarWaves.waves[1].targetFrequency = 3.0;
            planarWaves.targetColorParam = 0.5;
            delay(2);

            // Zoom out more, start moving
            planarWaves.waves[0].targetFrequency = 6.0;
            planarWaves.waves[1].targetFrequency = 6.0;
            planarWaves.targetColorParam = 1.0;
            planarWaves.speedTarget = Vec2(0, 0.0002);
            delay(2);

            // Third wave
            planarWaves.waves[0].targetFrequency = 7.0;
            planarWaves.waves[1].targetFrequency = 7.0;
            planarWaves.waves[2].targetAmplitude = 0.4;
            planarWaves.waves[0].targetAmplitude = 0.4;
            planarWaves.waves[1].targetAmplitude = 0.4;
            planarWaves.waves[2].targetAmplitude = 0.4;
            planarWaves.waves[2].targetAngle = M_PI/3;
            planarWaves.waves[2].targetFrequency = 8.0;
            delay(1);

            // Fourth wave
            planarWaves.waves[0].targetAmplitude = 0.3;
            planarWaves.waves[1].targetAmplitude = 0.3;
            planarWaves.waves[2].targetAmplitude = 0.3;
            planarWaves.waves[3].targetAmplitude = 0.3;
            planarWaves.waves[3].targetAngle = M_PI * 3.15;
            planarWaves.waves[3].targetFrequency = 7.3;
            delay(1);

            // Keep changing angle
            for (unsigned i = 0; i < 60; i++) {
                planarWaves.waves[i % 4].targetAngle += prng.uniform(-M_PI/8, M_PI/8);
                delay(1);
            }
#endif

            return 0;
    }
}
