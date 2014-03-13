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
#include "lib/mt19937.h"


class VisualMemory
{
public:
    void init(EffectRunner *runner);
    void process(const Camera::VideoChunk &chunk);

private:
    typedef float memory_t;
    typedef std::vector<memory_t> memoryVector_t;

    MT19937 random;
    EffectRunner *runner;
    std::vector<unsigned> denseToSparsePixelIndex;
    memoryVector_t memory;
    uint8_t samples[CameraSampler::kSamples];

    static const unsigned kLearnIterationsPerSample = 10;
    void learn(memory_t &cell, const uint8_t *pixel, uint8_t luminance);
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


inline void VisualMemory::init(EffectRunner *runner)
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
}

inline void VisualMemory::process(const Camera::VideoChunk &chunk)
{
    EffectRunner *runner = this->runner;
    CameraSampler sampler(chunk);
    unsigned sampleIndex;
    uint8_t luminance;
    unsigned denseSize = denseToSparsePixelIndex.size();

    // First, iterate over camera samples
    while (sampler.next(sampleIndex, luminance)) {
        memory_t *row = &memory[sampleIndex * denseSize];

        // Save the sample's luma value for later
        samples[sampleIndex] = luminance;

        // Randomly select kLearnIterationsPerSample LEDs to learn with
        for (unsigned iter = kLearnIterationsPerSample; iter; --iter) {
            unsigned denseIndex = (uint64_t(random.genrand_int32()) * denseSize) >> 32;
            unsigned sparseIndex = denseToSparsePixelIndex[denseIndex];

            // Single memory cell and pixel
            const uint8_t* pixel = runner->getPixel(sparseIndex);
            memory_t &cell = row[denseIndex];

            // Iterative learning algorithm
            learn(cell, pixel, luminance);
        }
    }
}

inline void VisualMemory::learn(memory_t &cell, const uint8_t *pixel, uint8_t luminance)
{
    uint8_t r = pixel[0];
    uint8_t g = pixel[1];
    uint8_t b = pixel[2];

    int lumaSq = int(luminance) * int(luminance);
    int pixSq = r*r + g*g + b*b;

    cell += lumaSq * pixSq;
}
