
#ifndef CVARS_H_
#define CVARS_H_

#include "sdk.h"
#include "globals.h"

#define CVAR_PREFIX  "dz_"
#define CVAR_HACK_ID 0x4000 /* (1<<14) One that is not in use by the game */

/*
 *  DECL_CVAR: Declares cvar variable in source file.
 *  DECL_CVAR_EXTERN: Same but for headers.
 *  REGISTER_CVAR: Create the cvar, return cvar_t*
 *  CVAR_ON: Returns true if the cvar is non-zero
 *
 * prefix | meaning
 * -------+-------------------------------
 * dz_*   | cvar variable
 */
#define DECL_CVAR(name) cvar_t* dz_##name = NULL;

#define DECL_CVAR_EXTERN(name) extern cvar_t* dz_##name;

#define REGISTER_CVAR(name, value) \
    dz_##name =                    \
      i_engine->pfnRegisterVariable(CVAR_PREFIX #name, #value, CVAR_HACK_ID);

#define CVAR_ON(name) (dz_##name->value != 0.0f)

/*----------------------------------------------------------------------------*/

DECL_CVAR_EXTERN(movement_bhop);
DECL_CVAR_EXTERN(movement_autostrafe);
DECL_CVAR_EXTERN(aim_aimbot);
DECL_CVAR_EXTERN(aim_autoshoot);
DECL_CVAR_EXTERN(visuals_esp);
DECL_CVAR_EXTERN(visuals_chams);
DECL_CVAR_EXTERN(visuals_crosshair);
DECL_CVAR_EXTERN(visuals_tracers);
DECL_CVAR_EXTERN(movement_clmove);
DECL_CVAR_EXTERN(watermark);
DECL_CVAR_EXTERN(watermark_rainbow);
DECL_CVAR_EXTERN(aim_aimbot_silent);
DECL_CVAR_EXTERN(visuals_friendly);
DECL_CVAR_EXTERN(movement_antiaim);
DECL_CVAR_EXTERN(movement_antiaim_view);
DECL_CVAR_EXTERN(movement_fakeduck);


/*----------------------------------------------------------------------------*/

bool cvars_init(void);

#endif /* CVARS_H_ */
