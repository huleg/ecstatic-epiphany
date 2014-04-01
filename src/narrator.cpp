/*
 * Infrastructure to tell a story with some lights.
 *
 * (c) 2014 Micah Elizabeth Scott
 * http://creativecommons.org/licenses/by/3.0/
 */

#include "narrator.h"


Narrator::Narrator()
{
    tap.setEffect(&mixer);
    runner.setEffect(&tap);
}

void Narrator::run()
{
    PRNG prng;

    totalTime = 0;
    totalLoops = 0;
    prng.seed(time(0));

    currentState = runner.initialState;

    while (true) {
        if (runner.isVerbose()) {
            fprintf(stderr, "narrator: state %d\n", currentState);
        }

        int nextState = doState(currentState, prng);

        if (nextState == runner.initialState) {
            totalLoops++;
            if (runner.isVerbose()) {
                fprintf(stderr, "narrator: ------------------- Summary ------------------\n");
                fprintf(stderr, "narrator: Total loops: %d\n", totalLoops);
                fprintf(stderr, "narrator:       loop total "); formatTime(totalTime); 
                fprintf(stderr, "  average ");
                formatTime(totalTime / totalLoops);
                fprintf(stderr, "\n");

                for (std::map<int, double>::iterator it = singleStateTime.begin(); it != singleStateTime.end(); it++) {
                    fprintf(stderr, "narrator: state %-3d  total ", it->first);
                    formatTime(it->second);
                    fprintf(stderr, "  average ");
                    formatTime(it->second / totalLoops);
                    fprintf(stderr, "\n");
                } 

                fprintf(stderr, "narrator: ----------------------------------------------\n");
            }
        }

        currentState = nextState;
    }
}

float Narrator::doFrame()
{
    float t = runner.doFrame();

    totalTime += t;
    singleStateTime[currentState] += t;

    return t;
}

void Narrator::crossfade(Effect *to, float duration)
{
    int n = mixer.numChannels();
    if (n > 0) {
        mixer.add(to);
        for (float t = 0; t < duration; t += doFrame()) {
            float q = t / duration;
            for (int i = 0; i < n; i++) {
                mixer.setFader(i, 1 - q);
            }
            mixer.setFader(n, q);
        }
    }
    mixer.set(to);
}

void Narrator::delay(float seconds)
{
    while (seconds > 0) {
        seconds -= doFrame();
    }
}

void Narrator::formatTime(double s)
{
    fprintf(stderr, "%02d:%02d:%05.2f", (int)s / (60*60), (int)s / 60, fmod(s, 60));
} 

Narrator::NEffectRunner::NEffectRunner()
    : initialState(0)
{}

bool Narrator::NEffectRunner::parseArgument(int &i, int &argc, char **argv)
{
    if (!strcmp(argv[i], "-state") && (i+1 < argc)) {
        initialState = atoi(argv[++i]);
        return true;
    }
    return EffectRunner::parseArgument(i, argc, argv);
}

void Narrator::NEffectRunner::argumentUsage()
{
    EffectRunner::argumentUsage();
    fprintf(stderr, " [-state ST]");
}