/*
 * Experimental learning algorithm
 *
 * (c) 2014 Micah Elizabeth Scott
 * http://creativecommons.org/licenses/by/3.0/
 */

#pragma once

#include <vector>
#include "lib/camera_sampler.h"
#include "lib/effect_runner.h"
#include "lib/jpge.h"
#include "lib/lodepng.h"
#include "lib/prng.h"
#include "latencytimer.h"


class VisualMemory
{
public:
    // Starts a dedicated processing thread
    void start(const EffectRunner *runner, const EffectTap *tap);

    void process(const Camera::VideoChunk &chunk);

    void debug(const char *outputPngFilename) const;

private:
    typedef double memory_t;
    struct Cell {
        memory_t shortTerm;
        memory_t longTerm;
    };

    // Updated on the learning thread constantly
    typedef std::vector<Cell> memoryVector_t;
    memoryVector_t memory;

    PRNG random;
    const EffectTap *tap;
    std::vector<unsigned> denseToSparsePixelIndex;

    // Updated on the video thread constantly, via process()
    uint8_t samples[CameraSampler::kSamples];

    // Separate learning thread
    tthread::thread *learnThread;
    static void learnThreadFunc(void *context);

    static const memory_t kLearningThreshold = 0.25;
    static const memory_t kShortTermPermeability = 1e-1;
    static const memory_t kLongTermPermeability = 1e-4;

    // Main loop for learning thread
    void learnWorker();
    memory_t reinforcementFunction(int luminance, Vec3 led);
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


inline void VisualMemory::start(const EffectRunner *runner, const EffectTap *tap)
{
    this->tap = tap;
    const Effect::PixelInfoVec &pixelInfo = runner->getPixelInfo();

    // Make a densely packed pixel index, skipping any unmapped pixels

    denseToSparsePixelIndex.clear();
    for (unsigned i = 0; i < pixelInfo.size(); ++i) {
        const Effect::PixelInfo &pixel = pixelInfo[i];
        if (pixel.isMapped()) {
            denseToSparsePixelIndex.push_back(i);
        }
    }

    // Calculate size of full visual memory

    unsigned denseSize = denseToSparsePixelIndex.size();
    unsigned cells = CameraSampler::kSamples * denseSize;
    fprintf(stderr, "vismem: %d camera samples * %d LED pixels = %d cells\n",
        CameraSampler::kSamples, denseSize, cells);

    memory.resize(cells);
    random.seed(27);
    memset(samples, 0, sizeof samples);

    // Let the thread loose. This starts learning right away- no other thread should be
    // writing to memory[] from now on.

    learnThread = new tthread::thread(learnThreadFunc, this);
}

inline void VisualMemory::process(const Camera::VideoChunk &chunk)
{
    CameraSampler sampler(chunk);
    unsigned sampleIndex;
    uint8_t luminance;

    while (sampler.next(sampleIndex, luminance)) {
        samples[sampleIndex] = luminance;
    }
}

inline void VisualMemory::learnThreadFunc(void *context)
{
    VisualMemory *self = static_cast<VisualMemory*>(context);
    self->learnWorker();
}

inline VisualMemory::memory_t VisualMemory::reinforcementFunction(int luminance, Vec3 led)
{
    Real r = std::min(1.0f, led[0]);
    Real g = std::min(1.0f, led[1]);
    Real b = std::min(1.0f, led[2]);

    memory_t lumaSq = (luminance * luminance) / 65025.0;
    memory_t pixSq = (r*r + g*g + b*b) / 3.0f;

    return lumaSq * pixSq;
}

inline void VisualMemory::learnWorker()
{
    unsigned denseSize = denseToSparsePixelIndex.size();

    while (true) {
        for (unsigned sampleIndex = 0; sampleIndex != CameraSampler::kSamples; sampleIndex++) {

            uint8_t luminance = samples[sampleIndex];

            // Compute maximum possible reinforcement from this luminance; if it's below the
            // learning threshold, we can skip the entire sample.

            if (reinforcementFunction(luminance, Vec3(1,1,1)) < kLearningThreshold) {
                continue;
            }

            const EffectTap::Frame *effectFrame = tap->get(LatencyTimer::kExpectedDelay);
            if (!effectFrame) {
                // This frame isn't in our buffer yet
                usleep(10 * 1000);
                continue;
            }

            Cell* cell = &memory[sampleIndex * denseSize];

            for (unsigned denseIndex = 0; denseIndex != denseSize; denseIndex++, cell++) {
                unsigned sparseIndex = denseToSparsePixelIndex[denseIndex];
                Vec3 pixel = effectFrame->colors[sparseIndex];

                memory_t reinforcement = reinforcementFunction(luminance, pixel);

                if (reinforcement >= kLearningThreshold) {
                    Cell state = *cell;

                    state.shortTerm = (state.shortTerm - state.shortTerm * kShortTermPermeability) + reinforcement;
                    state.longTerm += (state.shortTerm - state.longTerm) * kLongTermPermeability;

                    *cell = state;
                }
            }
        }
    }
}

inline void VisualMemory::debug(const char *outputPngFilename) const
{
    unsigned denseSize = denseToSparsePixelIndex.size();

    // Tiled array of camera samples, one per LED. Artificial square grid of LEDs.
    const int ledsWide = 4;
    const int width = ledsWide * CameraSampler::kBlocksWide;
    const int ledsHigh = (denseToSparsePixelIndex.size() + ledsWide - 1) / ledsWide;
    const int height = ledsHigh * CameraSampler::kBlocksHigh;
    std::vector<uint8_t> image;
    image.resize(width * height * 3);

    // Extents, using long-term memory to set expected range
    memory_t cellMax = memory[0].longTerm;
    for (unsigned cell = 1; cell < memory.size(); cell++) {
        memory_t v = memory[cell].longTerm;
        cellMax = std::max(cellMax, v);
    }
    memory_t cellScale = 1.0 / cellMax;

    fprintf(stderr, "vismem: range %f\n", cellMax);

    for (unsigned sample = 0; sample < CameraSampler::kSamples; sample++) {
        for (unsigned led = 0; led < denseSize; led++) {

            int sx = CameraSampler::blockX(sample);
            int sy = CameraSampler::blockY(sample);

            int x = sx + (led % ledsWide) * CameraSampler::kBlocksWide;
            int y = sy + (led / ledsWide) * CameraSampler::kBlocksHigh;

            const Cell &cell = memory[ sample * denseSize + led ];
            uint8_t *pixel = &image[ 3 * (y * width + x) ];

            memory_t s = cell.shortTerm * cellScale;
            memory_t l = cell.longTerm * cellScale;

            int shortTermI = std::min<memory_t>(255.5f, s*s*s*s * 255.0f + 0.5f);
            int longTermI = std::min<memory_t>(255.5f, l*l*l*l * 255.0f + 0.5f);

            pixel[0] = shortTermI;
            pixel[1] = pixel[2] = longTermI;
        }
    }

    if (lodepng_encode_file(outputPngFilename, &image[0], width, height, LCT_RGB, 8)) {
        fprintf(stderr, "vismem: error saving %s\n", outputPngFilename);
    } else {
        fprintf(stderr, "vismem: saved %s\n", outputPngFilename);
    }
}
