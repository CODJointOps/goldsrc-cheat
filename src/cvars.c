
#include "include/cvars.h"
#include "include/sdk.h"
#include "include/globals.h"
#include "include/game_detection.h"

DECL_CVAR(movement_bhop);
DECL_CVAR(movement_autostrafe);
DECL_CVAR(aim_aimbot);
DECL_CVAR(aim_autoshoot);
DECL_CVAR(visuals_esp);
DECL_CVAR(visuals_chams);
DECL_CVAR(visuals_crosshair);
DECL_CVAR(visuals_tracers);
DECL_CVAR(movement_clmove);
DECL_CVAR(watermark);
DECL_CVAR(watermark_rainbow);
DECL_CVAR(aim_aimbot_silent);
DECL_CVAR(visuals_friendly);
DECL_CVAR(movement_antiaim);
DECL_CVAR(movement_antiaim_view);
DECL_CVAR(movement_fakeduck);
DECL_CVAR(misc_namechanger);
DECL_CVAR(misc_namechanger_speed);
DECL_CVAR(visuals_fov);


bool cvars_init(void) {
    REGISTER_CVAR(movement_bhop, 1);
    REGISTER_CVAR(movement_autostrafe, 1);
    REGISTER_CVAR(aim_aimbot, 0);
    REGISTER_CVAR(aim_autoshoot, 0); /* Only works with aimbot enabled */
    REGISTER_CVAR(visuals_esp, 3);
    REGISTER_CVAR(visuals_chams, 1);
    REGISTER_CVAR(visuals_crosshair, 0);
    REGISTER_CVAR(movement_clmove, 0);
    REGISTER_CVAR(watermark, 1);
    REGISTER_CVAR(watermark_rainbow, 1);
    REGISTER_CVAR(aim_aimbot_silent, 1);
    REGISTER_CVAR(visuals_friendly, 0);
    REGISTER_CVAR(movement_antiaim, 0);
    REGISTER_CVAR(movement_antiaim_view, 0);
    REGISTER_CVAR(movement_fakeduck, 0);
    REGISTER_CVAR(misc_namechanger, 0);
    REGISTER_CVAR(misc_namechanger_speed, 10);
    REGISTER_CVAR(visuals_fov, 90);
    if (IsCS16()) {
    REGISTER_CVAR(visuals_tracers, 0);
    } else {
    REGISTER_CVAR(visuals_tracers, 1);
    }
    return true;
}
