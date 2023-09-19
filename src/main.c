
#include <stdio.h>
#include <dlfcn.h>

#include "include/main.h"
#include "include/sdk.h"
#include "include/globals.h"
#include "include/cvars.h"
#include "include/hooks.h"
#include "include/util.h"
#include "include/game_detection.h"

static bool loaded = false;

__attribute__((constructor)) /* Entry point when injected */
void load(void) {
    printf("goldsource-cheat injected!\n");

    /* Initialize globals/interfaces */
    if (!globals_init()) {
        fprintf(stderr, "goldsource-cheat: load: error loading globals, aborting\n");
        self_unload();
        return;
    }

    /* Create cvars for settings */
    if (!cvars_init()) {
        fprintf(stderr, "goldsource-cheat: load: error creating cvars, aborting\n");
        self_unload();
        return;
    }

    /* Hook functions */
    if (!hooks_init()) {
        fprintf(stderr, "goldsource-cheat: load: error hooking functions, aborting\n");
        self_unload();
        return;
    }

    /* Get game version after injecting */
    //this_game_id = get_cur_game();

    if (IsCS16()) {
        i_engine->pfnClientCmd("play 'sound/radio/go.wav'");
    }
    else if (IsDayOfDefeat()) {
        i_engine->pfnClientCmd("play 'sound/player/gersniper.wav'");
    }
    else if (IsTFC()) {
        i_engine->pfnClientCmd("play 'sound/misc/party2.wav'");
    }
    else
    {
        i_engine->pfnClientCmd("play 'valve/sound/vox/suit.wav'");
    }
    
    i_engine->pfnClientCmd("echo \"goldsource-cheat loaded successfully!\"");
    i_engine->pfnClientCmd("echo \"Deadzone rulez!\"");
    i_engine->pfnClientCmd("echo \"https://git.deadzone.lol/Wizzard/goldsource-cheat\"");


    GameType game = get_current_game();
    switch(game) {
        case GAME_HALFLIFE:
            i_engine->pfnClientCmd("echo \"Detected Game: Half-Life 1\"");
            break;
        case GAME_CS16:
            i_engine->pfnClientCmd("echo \"Detected Game: Counter-Strike 1.6\"");
            break;
        case GAME_DAY_OF_DEFEAT:
            i_engine->pfnClientCmd("echo \"Detected Game: Day of Defeat\"");
            break;
        case GAME_TFC:
            i_engine->pfnClientCmd("echo \"Detected Game: Team Fortress Classic\"");
            break;
        default:
            i_engine->pfnClientCmd("echo \"Detected Game: Unknown Game\"");
            break;
    }

    return;

    loaded = true;
}

__attribute__((destructor)) /* Entry point when unloaded */
void unload(void) {
    if (loaded) {
        /* TODO: Remove our cvars */

        globals_restore();
        hooks_restore();

        GL_UNHOOK(glColor4f); /* Manually restore OpenGL hooks here */
    }

    printf("goldsource-cheat unloaded.\n\n");
}

void self_unload(void) {
    /*
     * RTLD_LAZY: If the symbol is never referenced, then it is never resolved.
     * RTLD_NOLOAD: Don't load the shared object.
     */
    void* self = dlopen("libhlcheat.so", RTLD_LAZY | RTLD_NOLOAD);

    /* Close the call we just made to dlopen() */
    dlclose(self);

    /* Close the call our injector made */
    dlclose(self);
}
