// XXX hacky test code

#include <time.h>
#include <SDL/SDL.h>

#include "lib/effect.h"
#include "lib/effect_runner.h"
#include "lib/effect_mixer.h"
#include "lib/particle.h"
#include "lib/camera.h"
#include "lib/camera_framegrab.h"
#include "lib/camera_sampler.h"
#include "lib/prng.h"

#include "visual_memory.h"
#include "rings.h"
#include "chaos_particles.h"
#include "order_particles.h"
#include "precursor.h"
#include "spokes.h"

static CameraFramegrab grab;
static VisualMemory vismem;
static EffectTap tap;
static EffectRunner runner;
static EffectMixer mixer;
static SpokesEffect spokes;
static ChaosParticles chaosParticles;
static OrderParticles orderParticles;
static Precursor precursor;
static RingsEffect rings("data/glass.png");
static RecallDebugEffect recallDebug(&vismem);


static void videoCallback(const Camera::VideoChunk &video, void *)
{
    if (grab.isGrabbing()) {
        grab.process(video);
    }
    vismem.process(video);
}


static void debugThread(void *)
{
    char buffer[1024];
    unsigned counter = 0;
    struct tm tm;
    time_t clk;

    while (true) {
        clk = time(NULL);
        localtime_r(&clk, &tm);

        strftime(buffer, sizeof buffer, "output/%Y%m%d-%H%M%S-camera.jpeg", &tm);
        grab.begin(buffer);

        strftime(buffer, sizeof buffer, "output/%Y%m%d-%H%M%S-memory.png", &tm);
        vismem.debug(buffer);

        counter++;
        sleep(10);
    }
}

static void effectThread(void *)
{
    PRNG prng;
    prng.seed(time(0));

    while (true) {

        // Order trying to form out of the tiniest sparks; runs for a while, fails.
        precursor.reseed(prng.uniform32());
        mixer.set(&precursor);
        while (precursor.totalSecondsOfDarkness() < 6.0) {
            runner.doFrame();
        }

        // Bang.
        chaosParticles.reseed(Vec3(0,0,0), prng.uniform32());
        mixer.set(&chaosParticles);
        while (chaosParticles.isRunning()) {
            runner.doFrame();
        }

        // Textures of light
        rings.reseed();
        rings.palette.load("data/glass.png");
        mixer.set(&rings);
        for (float t = 0; t < 1; t += runner.doFrame() / 100.0f) {
            mixer.setFader(0, sinf(t * M_PI));
        }

        // Textures of energy
        rings.reseed();
        rings.palette.load("data/darkmatter-palette.png");
        mixer.set(&rings);
        for (float t = 0; t < 1; t += runner.doFrame() / 100.0f) {
            mixer.setFader(0, sinf(t * M_PI));
        }

        // Biology happens, order emerges
        orderParticles.reseed(prng.uniform32());
        mixer.set(&orderParticles);
        for (float t = 0; t < 1; t += runner.doFrame() / 100.0f) {
            orderParticles.vibration = 0.02 / (1.0 + t * 20.0);
            orderParticles.symmetry = 2 + (1 - t) * 12;
            mixer.setFader(0, sinf(t * M_PI));
        }

        // Textures of biology
        rings.reseed();
        rings.palette.load("data/succulent-palette.png");
        mixer.set(&rings);
        for (float t = 0; t < 1; t += runner.doFrame() / 100.0f) {
            mixer.setFader(0, sinf(t * M_PI));
        }
    }
}

static void sdlThread()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        perror("SDL init failed");
        return;
    }

    SDL_Surface *screen = SDL_SetVideoMode(1024, 768, 32, 0);
    if (!screen) {
        perror("SDL failed to set video mode");
        return;
    }

    SDL_Event event;
    do {

        SDL_Flip(screen);
        SDL_LockSurface(screen);

        int pitch = screen->pitch / 4;

        // Draw camera image with motion detection
        for (unsigned i = 0; i < CameraSampler8Q::kSamples; i++) {
            int x = 1 + CameraSampler8Q::sampleX(i);
            int y = 1 + CameraSampler8Q::sampleY(i);

            uint8_t s = vismem.luminance.buffer[i];
            //uint8_t m = std::min(255, std::max<int>(0, 0.1 * vismem.sobel.motion[i]));
            uint8_t l = vismem.learnFlags[i] ? 0xFF : 0;
            uint8_t m = vismem.recallFlags[CameraSampler8Q::blockIndex(i)] ? 0xFF : 0;

            uint32_t *pixel = x + pitch*y + (uint32_t*)screen->pixels;

            // Pack lots of debug info into RGB channels...
            uint32_t bgra = (m << 8) | (l << 16) | (s << 24);

            // Splat
            pixel[-pitch] = pixel[1] = pixel[0] = pixel[-1] = pixel[pitch] = bgra;
        }

        // Draw recall buffer
        unsigned left = 750;
        unsigned top = 400;

        for (unsigned i = 0; i < runner.getPixelInfo().size(); i++) {

            // Convert to RGB color
            float r = vismem.recall(i);
            unsigned s = r * 255; 
            uint32_t bgra = (s << 24) | (s << 16) | (s << 8);

            // Bars
            if (i < screen->h) {
                uint32_t *pixel = pitch*i + (uint32_t*)screen->pixels;
                unsigned bar = std::min(float(screen->w), (screen->w - left) * r + left);
                for (unsigned x = left; x < screen->w; x++) {
                    pixel[x] = x < bar ? bgra : 0;
                }
            }

            // XZ plane
            Vec3 point = runner.getPixelInfo()[i].point;
            int scale = -80;
            int x = screen->w/2 + point[0] * scale;
            int y = (screen->h + top) / 2 + point[2] * scale;
            if (x > 1 && x < screen->w - 1 && y > 1 && y < screen->h - 1) {
                uint32_t *pixel = x + pitch*y + (uint32_t*)screen->pixels;
            
                // Splat
                pixel[-pitch] = pixel[1] = pixel[0] = pixel[-1] = pixel[pitch] = bgra;
            }
        }

        SDL_UnlockSurface(screen);
        usleep(10000);

        while (SDL_PollEvent(&event) && event.type != SDL_QUIT);
    } while (event.type != SDL_QUIT);
}

int main(int argc, char **argv)
{
    Camera::start(videoCallback);

    tap.setEffect(&mixer);
    runner.setEffect(&tap);
    runner.setLayout("layouts/window6x12.json");
    if (!runner.parseArguments(argc, argv)) {
        return 1;
    }

    // Init visual memory, now that the layout is known
    vismem.start("imprint.mem", &runner, &tap);

    new tthread::thread(debugThread, 0);
    new tthread::thread(effectThread, 0);
    // sdlThread();
    while (1) sleep(1);
    return 0;
}
