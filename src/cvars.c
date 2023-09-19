
#include "include/cvars.h"
#include "include/sdk.h"
#include "include/globals.h"

DECL_CVAR(bhop);
DECL_CVAR(autostrafe);
DECL_CVAR(aimbot);
DECL_CVAR(autoshoot);
DECL_CVAR(esp);
DECL_CVAR(chams);
DECL_CVAR(crosshair);
DECL_CVAR(tracers);
DECL_CVAR(clmove);
DECL_CVAR(watermark);
DECL_CVAR(watermark_rainbow);

bool cvars_init(void) {
    REGISTER_CVAR(bhop, 1);
    REGISTER_CVAR(autostrafe, 1);
    REGISTER_CVAR(aimbot, 0);
    REGISTER_CVAR(autoshoot, 0); /* Only works with aimbot enabled */
    REGISTER_CVAR(esp, 3);
    REGISTER_CVAR(chams, 1);
    REGISTER_CVAR(crosshair, 0);
    REGISTER_CVAR(tracers, 1);
    REGISTER_CVAR(clmove, 0);
    REGISTER_CVAR(watermark, 1);
    REGISTER_CVAR(watermark_rainbow, 1);

    return true;
}
