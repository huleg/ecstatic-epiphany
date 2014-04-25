/*
 * Infrastructure to tell a story with some lights.
 *
 * (c) 2014 Micah Elizabeth Scott
 * http://creativecommons.org/licenses/by/3.0/
 */

#pragma once

#include "lib/effect.h"
#include "lib/effect_mixer.h"
#include "lib/effect_runner.h"
#include "lib/effect_tap.h"
#include "lib/prng.h"
#include "lib/camera_flow.h"


class Narrator
{
public:

    class NEffectRunner : public EffectRunner
    {   
    public:
        NEffectRunner();
        int initialState;
    protected:
        virtual bool parseArgument(int &i, int &argc, char **argv);
        virtual void argumentUsage();
    };

    Narrator();

    void run();

    CameraFlowAnalyzer flow;
    NEffectRunner runner;
    EffectMixer mixer;

private:
    int script(int st, PRNG &prng);
    float doFrame();

    void crossfade(Effect *to, float duration);
    void delay(float seconds);

    void formatTime(double s);

    unsigned totalLoops;
    double totalTime;
    std::map<int, double> singleStateTime;
    int currentState;
};
