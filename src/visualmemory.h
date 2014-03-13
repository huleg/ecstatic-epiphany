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
    typedef float memory_t;
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

    static const memory_t kLearningThreshold = 0.1;
    static const memory_t kDamping = 0.9999;
    static const memory_t kSmoothingGain = 0.0001;
    static const unsigned kSmoothingSteps = 20;

    // Main loop for learning thread
    void learnWorker();

    // Reinforce memories. Called from a dedicated learning thread.
    void learn(memory_t &cell, const uint8_t *pixel, uint8_t luminance);

    // Smoothing over one axis. Pushes towards a version of the signal with DC
    // components removed along axis A. Remove DC signals from one axis (A) while iterating
    // over another axis (B).
    void smooth(unsigned stepA, unsigned countA, unsigned stepB, unsigned countB);
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

inline void VisualMemory::learnWorker()
{
    unsigned denseSize = denseToSparsePixelIndex.size();
    EffectRunner *runner = this->runner;

    unsigned sampleIndex = 0;
    unsigned denseIndex = 0;
    memory_t *cell = &memory[0];

    // Process in memory sequence, for cache performance
    while (true) {

        unsigned sparseIndex = denseToSparsePixelIndex[denseIndex];
        uint8_t luminance = samples[sampleIndex];
        const uint8_t* pixel = runner->getPixel(sparseIndex);

        learn(*cell, pixel, luminance);

        cell++;
        denseIndex++;
        if (denseIndex == denseSize) {
            denseIndex = 0;
            sampleIndex++;

            if (sampleIndex == CameraSampler::kSamples) {
                sampleIndex = 0;
                cell = &memory[0];

                // Multiple integration steps
                for (unsigned s = kSmoothingSteps; s; --s) {
                    smooth(1, denseSize, denseSize, CameraSampler::kSamples);
                    smooth(denseSize, CameraSampler::kSamples, 1, denseSize);
                }
            }
        }
    }
}

inline void VisualMemory::learn(memory_t &cell, const uint8_t *pixel, uint8_t luminance)
{
    uint8_t r = pixel[0];
    uint8_t g = pixel[1];
    uint8_t b = pixel[2];

    memory_t lumaSq = (int(luminance) * int(luminance)) / 65025.0;
    memory_t pixSq = int(r*r + g*g + b*b) / 195075.0;
    memory_t reinforcement = lumaSq * pixSq;

    if (reinforcement > kLearningThreshold) {
        cell += reinforcement;
    }
}

inline void VisualMemory::smooth(unsigned stepA, unsigned countA,
    unsigned stepB, unsigned countB)
{
    // Normalize, remove DC signals from one axis (A) while iterating
    // over another axis (B).

    unsigned indexB = 0, remainingB = countB;
    while (remainingB) {

        // First pass, sum over axis A

        memory_t total = 0;
        unsigned indexA = indexB, remainingA = countA;
        while (remainingA) {
            total += memory[indexA];
            indexA += stepA;
            remainingA--;
        }

        // Now subtract the average

        memory_t adjustment = -(kSmoothingGain / kSmoothingSteps) * total / countA;
        indexA = indexB;
        remainingA = countA;
        while (remainingA) {
            memory[indexA] = kDamping * (memory[indexA] - adjustment);
            indexA += stepA;
            remainingA--;
        }

        indexB += stepB;
        remainingB--;
    }
}

inline void VisualMemory::debug(const char *outputPngFilename) const
{
    unsigned denseSize = denseToSparsePixelIndex.size();

    // Tiled array of camera samples, one per LED. Artificial square grid of LEDs.
    const int ledsWide = 16;
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
    memory_t cellScale = 255 / (cellMax - cellMin);
    fprintf(stderr, "vismem: range [%f, %f]\n", cellMin, cellMax);

    for (unsigned sample = 0; sample < CameraSampler::kSamples; sample++) {
        for (unsigned led = 0; led < denseSize; led++) {

            int sx = CameraSampler::blockX(sample);
            int sy = CameraSampler::blockY(sample);

            int x = sx + (led % ledsWide) * CameraSampler::kBlocksWide;
            int y = sy + (led / ledsWide) * CameraSampler::kBlocksHigh;

            const memory_t &cell = memory[ sample * denseSize + led ];
            uint8_t *pixel = &image[ y * width + x ];

            *pixel = (cell - cellMin) * cellScale;
        }
    }

    if (lodepng_encode_file(outputPngFilename, &image[0], width, height, LCT_GREY, 8)) {
        fprintf(stderr, "vismem: error saving %s\n", outputPngFilename);
    } else {
        fprintf(stderr, "vismem: saved %s\n", outputPngFilename);
    }
}
