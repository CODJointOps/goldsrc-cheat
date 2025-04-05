#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/mman.h>

#include "include/globals.h"
#include "include/sdk.h"
#include "include/util.h"

game_id this_game_id = HL;
vec3_t g_punchAngles = { 0, 0, 0 };

float g_flNextAttack = 0.f, g_flNextPrimaryAttack = 0.f;
int g_iClip = 0;
int g_currentWeaponID = -1;

double g_flCurrentTime = 0.0;


void* hw;
void** h_client;
DECL_INTF(cl_enginefunc_t, engine);
DECL_INTF(cl_clientfunc_t, client);
DECL_INTF(playermove_t, pmove);
DECL_INTF(engine_studio_api_t, enginestudio);
DECL_INTF(StudioModelRenderer_t, studiomodelrenderer);

game_t* game_info;

void* player_extra_info;

cl_entity_t* localplayer = NULL;

float* scr_fov_value = NULL;

/*----------------------------------------------------------------------------*/

bool globals_init(void) {
    hw = dlopen("hw.so", RTLD_LAZY | RTLD_NOLOAD);
    if (!hw) {
        printf("goldsource-cheat: globals_init: can't open hw.so\n");
        return false;
    }

    h_client = (void**)dlsym(hw, "hClientDLL");
    if (!h_client) {
        printf("goldsource-cheat: globals_init: can't find hClientDLL\n");
        return false;
    }

    i_engine       = (cl_enginefunc_t*)dlsym(hw, "cl_enginefuncs");
    i_client       = (cl_clientfunc_t*)dlsym(hw, "cl_funcs");
    i_pmove        = *(playermove_t**)dlsym(hw, "pmove");
    i_enginestudio = (engine_studio_api_t*)dlsym(hw, "engine_studio_api");

    const char* SMR_STR   = "g_StudioRenderer"; /* For clang-format */
    i_studiomodelrenderer = *(StudioModelRenderer_t**)dlsym(*h_client, SMR_STR);

    player_extra_info = dlsym(*h_client, "g_PlayerExtraInfo");

    game_info = *(game_t**)dlsym(hw, "game");

    scr_fov_value = (float*)dlsym(hw, "scr_fov_value");
    
    if (!i_engine || !i_client || !i_pmove || !i_enginestudio ||
        !i_studiomodelrenderer || !game_info) {
        printf("goldsource-cheat: globals_init: couldn't load some symbols\n");
        return false;
    }

    if (!protect_addr(i_studiomodelrenderer, PROT_READ | PROT_WRITE)) {
        printf("goldsource-cheat: globals_init: couldn't unprotect address of SMR\n");
        return false;
    }

    globals_store();

    return true;
}

void globals_store(void) {
    memcpy(&o_engine, i_engine, sizeof(cl_enginefunc_t));
    memcpy(&o_client, i_client, sizeof(cl_clientfunc_t));
    memcpy(&o_enginestudio, i_enginestudio, sizeof(engine_studio_api_t));
}

void globals_restore(void) {
    memcpy(i_engine, &o_engine, sizeof(cl_enginefunc_t));
    memcpy(i_client, &o_client, sizeof(cl_clientfunc_t));
    memcpy(i_enginestudio, &o_enginestudio, sizeof(engine_studio_api_t));
}
