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


class VisualMemory
{
public:
    // Starts a dedicated processing thread
    void start(EffectRunner *runner);

    void process(const Camera::VideoChunk &chunk);

    void debug(const char *outputPngFilename) const;

private:
    typedef double memory_t;
    typedef std::vector<memory_t> memoryVector_t;

    PRNG random;
    EffectRunner *runner;
    std::vector<unsigned> denseToSparsePixelIndex;

    // Updated on the learning thread constantly
    memoryVector_t memory;

    // Updated on the video thread constantly, via process()
    uint8_t samples[CameraSampler::kSamples];

    // Separate learning thread
    tthread::thread *learnThread;
    static void learnThreadFunc(void *context);

    static const memory_t kLearningThreshold = 0.5;
    static const memory_t kForgetfulness = 1e-5;

    // Main loop for learning thread
    void learnWorker();
    memory_t reinforcementFunction(int luminance, int r, int g, int b);
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


inline void VisualMemory::start(EffectRunner *runner)
{
    this->runner = runner;
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

inline VisualMemory::memory_t VisualMemory::reinforcementFunction(int luminance, int r, int g, int b)
{
    memory_t lumaSq = (luminance * luminance) / 65025.0;
    memory_t pixSq = (r*r + g*g + b*b) / 195075.0;
    return lumaSq * pixSq;
}

inline void VisualMemory::learnWorker()
{
    unsigned denseSize = denseToSparsePixelIndex.size();
    EffectRunner *runner = this->runner;
    while (true) {
        for (unsigned sampleIndex = 0; sampleIndex != CameraSampler::kSamples; sampleIndex++) {

            uint8_t luminance = samples[sampleIndex];

            // Compute maximum possible reinforcement from this luminance; if it's below the
            // learning threshold, we can skip the entire sample.

            if (reinforcementFunction(luminance, 255, 255, 255) < kLearningThreshold) {
                continue;
            }

            memory_t *cell = &memory[sampleIndex * denseSize];

            for (unsigned denseIndex = 0; denseIndex != denseSize; denseIndex++, cell++) {
                unsigned sparseIndex = denseToSparsePixelIndex[denseIndex];
                const uint8_t* pixel = runner->getPixel(sparseIndex);

                uint8_t r = pixel[0];
                uint8_t g = pixel[1];
                uint8_t b = pixel[2];

                memory_t reinforcement = reinforcementFunction(luminance, r, g, b);

                if (reinforcement >= kLearningThreshold) {
                    memory_t c = *cell;
                    *cell = (c - c * kForgetfulness) + reinforcement;
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
    image.resize(width * height);

    // Extents
    memory_t cellMin = memory[0];
    memory_t cellMax = memory[0];
    for (unsigned cell = 1; cell < memory.size(); cell++) {
        memory_t v = memory[cell];
        cellMin = std::min(cellMin, v);
        cellMax = std::max(cellMax, v);
    }

    memory_t cellScale = 1.0 / (cellMax - cellMin);
    const float gamma = 1.0 / 4.0f;
    fprintf(stderr, "vismem: range [%f, %f]\n", cellMin, cellMax);

    for (unsigned sample = 0; sample < CameraSampler::kSamples; sample++) {
        for (unsigned led = 0; led < denseSize; led++) {

            int sx = CameraSampler::blockX(sample);
            int sy = CameraSampler::blockY(sample);

            int x = sx + (led % ledsWide) * CameraSampler::kBlocksWide;
            int y = sy + (led / ledsWide) * CameraSampler::kBlocksHigh;

            const memory_t &cell = memory[ sample * denseSize + led ];
            uint8_t *pixel = &image[ y * width + x ];

            *pixel = powf((cell - cellMin) * cellScale, gamma) * 255.0f + 0.5f;
        }
    }

    if (lodepng_encode_file(outputPngFilename, &image[0], width, height, LCT_GREY, 8)) {
        fprintf(stderr, "vismem: error saving %s\n", outputPngFilename);
    } else {
        fprintf(stderr, "vismem: saved %s\n", outputPngFilename);
    }
}
