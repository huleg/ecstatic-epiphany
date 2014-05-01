/*
 * Infrastructure to tell a story with some lights.
 *
 * (c) 2014 Micah Elizabeth Scott
 * http://creativecommons.org/licenses/by/3.0/
 */

#include "narrator.h"


Narrator::Narrator()
    : brightness(mixer)
{
    runner.setEffect(&brightness);
}

void Narrator::setup()
{
    flow.setConfig(runner.config["flow"]);
    brightness.set(0.0f, runner.config["brightnessLimit"].GetDouble());
    mixer.setConcurrency(runner.config["concurrency"].GetUint());
    runner.setMaxFrameRate(runner.config["fps"].GetDouble());
    currentState = runner.initialState;
}    

void Narrator::run()
{
    PRNG prng;

    totalTime = 0;
    totalLoops = 0;
    prng.seed(time(0));

    while (true) {
        if (runner.isVerbose()) {
            fprintf(stderr, "narrator: state %d\n", currentState);
        }

        int nextState = script(currentState, prng);

        if (nextState == runner.initialState) {
            totalLoops++;

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

        currentState = nextState;
    }
}

EffectRunner::FrameStatus Narrator::doFrame()
{
    EffectRunner::FrameStatus st = runner.doFrame();

    totalTime += st.timeDelta;
    singleStateTime[currentState] += st.timeDelta;

    if (st.debugOutput && runner.isVerbose()) {
        fprintf(stderr, "\t[narrator] state = %d\n", currentState);
    }

    return st;
}

void Narrator::crossfade(Effect *to, float duration)
{
    int n = mixer.numChannels();
    if (n > 0) {
        mixer.add(to);
        for (float t = 0; t < duration; t += doFrame().timeDelta) {
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
        seconds -= doFrame().timeDelta;
    }
}

void Narrator::delayForever()
{
    for (;;) {
        doFrame();
    }
}

void Narrator::formatTime(double s)
{
    fprintf(stderr, "%02d:%02d:%05.2f", (int)s / (60*60), ((int)s / 60) % 60, fmod(s, 60));
} 

Narrator::NEffectRunner::NEffectRunner()
    : initialState(0)
{
    if (!setConfig("data/config.json")) {
        fprintf(stderr, "Can't load default configuration file\n");
    }
}

bool Narrator::NEffectRunner::parseArgument(int &i, int &argc, char **argv)
{
    if (!strcmp(argv[i], "-state") && (i+1 < argc)) {
        initialState = atoi(argv[++i]);
        return true;
    }

    if (!strcmp(argv[i], "-config") && (i+1 < argc)) {
        if (!setConfig(argv[++i])) {
            fprintf(stderr, "Can't load config from %s\n", argv[i]);
            return false;
        }
        return true;
    }

    return EffectRunner::parseArgument(i, argc, argv);
}

void Narrator::NEffectRunner::argumentUsage()
{
    EffectRunner::argumentUsage();
    fprintf(stderr, " [-state ST] [-config FILE.json]");
}

bool Narrator::NEffectRunner::validateArguments()
{
    return config.IsObject() && EffectRunner::validateArguments();
}

bool Narrator::NEffectRunner::setConfig(const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (!f) {
        return false;
    }

    rapidjson::FileStream istr(f);
    config.ParseStream<0>(istr);
    fclose(f);

    if (config.HasParseError()) {
        return false;
    }
    if (!config.IsObject()) {
        return false;
    }

    initialState = config["initialState"].GetInt();

    return true;
}

void Narrator::attention(Sampler &s, const rapidjson::Value& config)
{
    // Keep running until we run out of attention for the current scene.
    // Attention amounts and retention parameters come from JSON, and we show
    // our state during debug debug output.

    float bootstrap = s.value(config["bootstrap"]);
    float attention = s.value(config["initial"]);
    float brightnessDeltaMin = s.value(config["brightnessDeltaMin"]);
    float brightnessAverageMin = s.value(config["brightnessAverageMin"]);
    float rateBaseline = s.value(config["rateBaseline"]);
    float rateDark = s.value(config["rateDark"]);
    float rateStill = s.value(config["rateStill"]);

    delay(bootstrap);

    while (attention > 0) {
        EffectRunner::FrameStatus st = doFrame();

        float brightnessDelta = brightness.getTotalBrightnessDelta();
        float brightnessAverage = brightness.getAverageBrightness();

        float rate = rateBaseline
            + (brightnessDelta < brightnessDeltaMin ? rateStill : 0)
            + (brightnessAverage < brightnessAverageMin ? rateDark : 0);

        if (st.debugOutput && runner.isVerbose()) {
            fprintf(stderr, "\t[narrator] attention = %f\n", attention);
            fprintf(stderr, "\t[narrator] rate = %f\n", rate);
            fprintf(stderr, "\t[narrator] brightnessDelta = %f, threshold = %f %s\n",
                brightnessDelta, brightnessDeltaMin, brightnessDelta < brightnessDeltaMin ? "<<<" : "");
            fprintf(stderr, "\t[narrator] brightnessAverage = %f, threshold = %f %s\n",
                brightnessAverage, brightnessAverageMin, brightnessAverage < brightnessAverageMin ? "<<<" : "");
        }

        attention -= st.timeDelta * rate;
    }
}
