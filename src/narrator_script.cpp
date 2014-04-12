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
#include "planar_waves.h"
#include "partner_dance.h"


int Narrator::script(int st, PRNG &prng)
{
    static ChaosParticles chaosParticles[2];
    static OrderParticles orderParticles;
    static Precursor precursor;
    static RingsEffect ringsA, ringsB;
    static Ants ants;
    static PartnerDance partnerDance;


    switch (st) {

        default: {
            return 0;
        }

        case 0: {
            return 10;
        }

        case 10: {
            // Order trying to form out of the tiniest sparks; runs for an unpredictable time, fails.
            precursor.reseed(prng.uniform32());
            crossfade(&precursor, 20);
            precursor.resetCycle();
            do { doFrame(); } while (precursor.totalSecondsOfDarkness() < 6.1);
            // Make sure we get a little bit of quiet before the bang
            mixer.setFader(0, 0.0f);
            delay(1.0);
            return 20;
        }

        case 20: {
            // Bang. Explosive energy, hints of self-organization

            ChaosParticles *pChaosA = &chaosParticles[0];
            ChaosParticles *pChaosB = &chaosParticles[1];

            for (int i = 0; i < prng.uniform(1, 5); i++) {
                pChaosA->reseed(prng.circularVector() * 0.6, prng.uniform32());
                crossfade(pChaosA, 0.25);
                delay((1 << i) * 0.5f);
                std::swap(pChaosA, pChaosB);
            }

            // Run unconditionally to bootstrap particle intensity
            delay(30);

            // Keep going as long as it's fairly bright, with an upper limit
            float timeLimit = 4*60;
            do {
                timeLimit -= doFrame();
            } while (timeLimit > 0 && !(pChaosB->getTotalIntensity() < 500));

            return 30;
        }

        case 30: {
            // Textures of light, slow crossfade in
            ringsA.reseed();
            ringsA.palette.load("data/glass.png");
            crossfade(&ringsA, prng.uniform(15, 25));
            delay(prng.uniform(25, 90));
            return 40;
        }

        case 40: {
            // Textures of energy, slow crossfade in
            ringsB.reseed();
            ringsB.palette.load("data/darkmatter-palette.png");
            crossfade(&ringsB, prng.uniform(30, 90));
            delay(prng.uniform(25, 160));
            return 50;
        }

        case 50: {
            // Biology happens, order emerges
            orderParticles.reseed(prng.uniform32());
            orderParticles.vibration = 0.001;
            orderParticles.symmetry = 16;
            crossfade(&orderParticles, 15);

            // Run until we have square grid symmetry
            while (orderParticles.symmetry > 4) {
                delay(prng.uniform(2, 90));
                orderParticles.symmetry--;
            }
            delay(prng.uniform(10, 25));
            return 60;
        }

        case 60: {
            // Langton's ant
            ants.reseed(prng.uniform32());
            ants.stepSize = 0.5;
            crossfade(&ants, prng.uniform(5, 20));
            delay(prng.uniform(3, 6));
            ants.stepSize = 0.1;
            delay(prng.uniform(15, 60));
            ants.stepSize = 0.05;
            delay(prng.uniform(15, 90));
            ants.stepSize = 0.025;
            delay(2);
            ants.stepSize = 0.0125;
            delay(2);
            ants.stepSize = 0.00625;
            return 70;
        }

        case 70: {
            // Fast ant slowly crossfades into rings with same palette
            ringsA.reseed();
            ringsA.palette.load("data/succulent-palette.png");
            crossfade(&ringsA, prng.uniform(15, 30));
            delay(prng.uniform(60*1, 60*3));
            return 80;
        }

        case 80: {
            // Work in progress
            partnerDance.reseed(prng.uniform32());
            crossfade(&partnerDance, prng.uniform(5, 20));
            delay(prng.uniform(60*1, 60*3));
            return 90;
        }
    }
}
