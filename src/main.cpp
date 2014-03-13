#include "lib/effect.h"
#include "lib/effect_runner.h"
#include "lib/effect_mixer.h"
#include "rings.h"


int main(int argc, char **argv)
{
    RingsEffect rings("data/glass.png");

    EffectMixer mixer;
    mixer.add(&rings);

    EffectRunner r;
    r.setEffect(&mixer);
    r.setLayout("layouts/window6x12.json");
    return r.main(argc, argv);
}