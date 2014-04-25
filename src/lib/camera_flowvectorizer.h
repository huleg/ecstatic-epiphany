/*
 * Camera Flow Vectorizer - an input device using optical flow
 * vector clustering for low-latency non-location-specific motion
 * capture.
 *
 * Copyright (c) 2014 Micah Elizabeth Scott <micah@scanlime.org>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <opencv2/opencv.hpp>
#include <stdio.h>
#include "camera.h"


class CameraFlowVectorizer {
public:
    CameraFlowVectorizer();

    // Process another chunk of video
    void process(const Camera::VideoChunk &chunk);

private:
    const unsigned kLinesPerStripe = Camera::kLinesPerField / 2;

    cv::Mat currentFrame;
    cv::Mat prevFrame;
    cv::Mat flow;

    void calculateFlow(unsigned field, unsigned stripe);
};


/*****************************************************************************************
 *                                   Implementation
 *****************************************************************************************/


inline CameraFlowVectorizer::CameraFlowVectorizer()
{
    unsigned width = Camera::kPixelsPerLine;
    unsigned height = Camera::kLinesPerField * Camera::kFields;

    currentFrame = cv::Mat::zeros(height, width, CV_8UC1);
    prevFrame = cv::Mat::zeros(height, width, CV_8UC1);
    flow = cv::Mat::zeros(height, width, CV_32FC2);
}

static void drawOptFlowMap(const cv::Mat& flow, cv::Mat& cflowmap, int step, const cv::Scalar& color)
{
    for(int y = 0; y < cflowmap.rows; y += step)
        for(int x = 0; x < cflowmap.cols; x += step)
        {
            const cv::Point2f& fxy = flow.at<cv::Point2f>(y, x);
            cv::line(cflowmap, cv::Point(x,y), cv::Point(cvRound(x+fxy.x), cvRound(y+fxy.y)),
                 color);
        }
}

inline void CameraFlowVectorizer::process(const Camera::VideoChunk &chunk)
{
    // Store luminance only (odd bytes)

    Camera::VideoChunk iter = chunk;
    uint8_t *dest = currentFrame.data
        + (iter.field * Camera::kLinesPerField + iter.line) * Camera::kPixelsPerLine
        + iter.byteOffset / 2;

    while (iter.byteCount) {
        if (iter.byteOffset & 1) {
            *(dest++) = *iter.data;
        }
        iter.data++;
        iter.byteOffset++;
        iter.byteCount--;
    }

    // End of line?
    if (chunk.byteCount + chunk.byteOffset == Camera::kBytesPerLine) {

        // End of stripe?
        if ((chunk.line % kLinesPerStripe) == (kLinesPerStripe - 1)) {
            calculateFlow(chunk.field, chunk.line / kLinesPerStripe);
        }

        // End of frame?
        if (chunk.line == Camera::kLinesPerField - 1 && chunk.field == Camera::kFields - 1) {
            std::swap(currentFrame, prevFrame);

            static int i = 0;
            char s[256];
            ++i;

            cv::Mat cflow;
            cv::cvtColor(prevFrame, cflow, cv::COLOR_GRAY2BGR);
            drawOptFlowMap(flow, cflow, 12, cv::Scalar(0, 255, 0));
            snprintf(s, sizeof s, "frame%d.png", i);
            cv::imwrite(s, cflow);

        }
    }
}

inline void CameraFlowVectorizer::calculateFlow(unsigned field, unsigned stripe)
{            
    unsigned firstRow = Camera::kLinesPerField * field + stripe * kLinesPerStripe;

    cv::Mat stripePrev = prevFrame.rowRange(firstRow, firstRow + kLinesPerStripe);
    cv::Mat stripeNext = currentFrame.rowRange(firstRow, firstRow + kLinesPerStripe);
    cv::Mat stripeFlow = flow.rowRange(firstRow, firstRow + kLinesPerStripe);

    printf("SF+\n");
    cv::calcOpticalFlowSF(stripePrev, stripeNext, stripeFlow,
        3,      // layers
        2,      // averaging_block_size
        4,      // max_flow
        4.1,    // sigma_dist
        25.5,   // sigma_color
        4,      // postprocess_window
        55,     // sigma_dist_fix
        25.5,   // sigma_color_fix
        0.35,   // occ_thr
        4,      // upscale_averaging_radius
        55.0,   // upscale_sigma_dist
        25.5,   // upscale_sigma_color
        10);    // speed_up_thr
    printf("SF-\n");
}
