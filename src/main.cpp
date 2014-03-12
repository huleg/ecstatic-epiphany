#include "lib/effect.h"
#include "rings_2d.h"


int main(int argc, char **argv)
{
    EffectRunner r;

    Rings2D e("data/glass.png", 0);
    r.setEffect(&e);

    // Defaults, overridable with command line options
    r.setLayout("layouts/window6x12.json");

    return r.main(argc, argv);
}