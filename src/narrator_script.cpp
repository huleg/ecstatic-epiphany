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
#include "partner_dance.h"


int Narrator::script(int st, PRNG &prng)
{
    static ChaosParticles chaosParticles[2];
    static OrderParticles orderParticles;
    static Precursor precursor;
    static RingsEffect ringsA, ringsB;
    static PartnerDance partnerDance;


    switch (st) {

        default: {
            return 0;
        }

        case 0: {
            return 10;
        }

        case 1: {
            // Special state; precursor only (sleep mode)            
            precursor.reseed(prng.uniform32());
            crossfade(&precursor, 1);
            precursor.resetCycle();
            while (precursor.cycle < 1.0f) {
                doFrame();
            }
            delay(prng.uniform(1, 45));
            return 1;
        }

        case 10: {
            // Order trying to form out of the tiniest sparks; runs for an unpredictable time, fails.
            precursor.reseed(prng.uniform32());
            crossfade(&precursor, 20);

            // Some quiet
            precursor.resetCycle();
            delay(4);
            precursor.resetCycle();

            // Run for a randomly determined amount of time
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
            // Biology happens, order emerges. Cellular look, emergent order.
            // Smooth out overall brightness jitter with the "Brightness" object.

            orderParticles.reseed(prng.uniform32());
            orderParticles.symmetry = 10;
            crossfade(&orderParticles, 15);
            while (orderParticles.symmetry > 2) {
                delay(prng.uniform(3, 20));
                orderParticles.symmetry--;
            }
            delay(prng.uniform(10, 25));
            return 60;
        }

        case 60: {
            // Two partners, populations of particles.
            // Act one, spiralling inwards. Depression.
            // Sparks happen at the edge of the void.
            // Foam on the edge of the apocalypse.

            partnerDance.reseed(prng.uniform32());

            // Start out slow during the crossfade
            partnerDance.targetGain = 0.00002;
            partnerDance.targetSpin = 0.000032;
            partnerDance.damping = 0.0045;
            partnerDance.dampingRate = 0;
            partnerDance.interactionRate = 0;

            crossfade(&partnerDance, prng.uniform(5, 10));

            // Normal speed
            partnerDance.targetGain = 0.00005;
            partnerDance.dampingRate = 0.0001;
            delay(prng.uniform(50, 75));

            // Damp quickly, fall into the void
            partnerDance.dampingRate = 0.003;
            partnerDance.targetSpin = 0.0001;
            delay(prng.uniform(20, 30));

            // Start interacting; agency overpowers the void
            partnerDance.targetGain = 0.000015;
            partnerDance.targetSpin = 0;
            partnerDance.damping = 0.0075;
            partnerDance.dampingRate = 0;
            partnerDance.interactionRate = 0.00007;
            delay(prng.uniform(80, 120));

            return 80;
        }
    }
}
