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
#include "vision_debug.h"
#include "glowpoi.h"


int Narrator::script(int st, PRNG &prng)
{
    static ChaosParticles chaosA(flow, runner.config["chaosParticles"]);
    static ChaosParticles chaosB(flow, runner.config["chaosParticles"]);
    static OrderParticles orderParticles(flow, runner.config["orderParticles"]);
    static Precursor precursor(flow, runner.config["precursor"]);
    static RingsEffect ringsA(flow, runner.config["ringsA"]);
    static RingsEffect ringsB(flow, runner.config["ringsB"]);
    static PartnerDance partnerDance(flow, runner.config["partnerDance"]);
    static VisionDebug visionDebug(flow, runner.config["visionDebug"]);
    static GlowPoi glowPoi(flow, runner.config["glowPoi"]);

    switch (st) {

        //////////////////////////////////////////////////////////////////////////////////////////////
        // Defaults

        default: {
            return 0;
        }

        case 0: {
            return 10;
        }

        //////////////////////////////////////////////////////////////////////////////////////////////
        // Debug states

        case 1: {
            // Special state; precursor only (sleep mode)            
            precursor.reseed(prng.uniform32());
            crossfade(&precursor, 1);
            while (true) {
                doFrame();
            }
            return 1;
        }

        case 2: {
            // Debugging the computer vision system
            crossfade(&visionDebug, 1);
            while (true) {
                doFrame();
            }
            return 2;
        }

        //////////////////////////////////////////////////////////////////////////////////////////////
        // Normal states

        case 10: {
            // Order trying to form out of the tiniest sparks; runs for an unpredictable time, fails.
            precursor.reseed(prng.uniform32());
            crossfade(&precursor, 20);

            // XXX: Temporary
            delay(30);

            // Make sure we get a little bit of quiet before the bang
            mixer.setFader(0, 0.0f);
            delay(1.0);
            return 20;
        }

        case 20: {
            // Bang. Explosive energy, hints of self-organization

            ChaosParticles *pChaosA = &chaosA;
            ChaosParticles *pChaosB = &chaosB;
            
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
            // Textures of light, exploring something formless. Slow crossfade in
            ringsA.reseed();
            crossfade(&ringsA, prng.uniform(15, 25));
            delay(prng.uniform(25, 90));
            return 40;
        }

        case 40: {
            // Add energy, explore another layer.
            ringsB.reseed();
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
            // Spiralling inwards. Depression. Beauty on the edge of destruction

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

            return 70;
        }

        case 70: {
            // Agency, creative energy.

            glowPoi.reseed(prng.uniform32());
            crossfade(&glowPoi, prng.uniform(10, 15));
            delay(prng.uniform(45, 90));
            return 80;
        }
    }
}
