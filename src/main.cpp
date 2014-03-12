#include "lib/effect.h"
#include "rings.h"


int main(int argc, char **argv)
{
    EffectRunner r;

    RingsEffect e("data/glass.png");
    r.setEffect(&e);

    // Defaults, overridable with command line options
    r.setLayout("layouts/window6x12.json");

    return r.main(argc, argv);
}