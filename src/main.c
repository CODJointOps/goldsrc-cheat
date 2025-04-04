#include <ctype.h>
#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "include/main.h"
#include "include/sdk.h"
#include "include/globals.h"
#include "include/settings.h"
#include "include/hooks.h"
#include "include/util.h"
#include "include/game_detection.h"

static bool loaded = false;

/* Create a debug log file */
void debug_log(const char* message) {
    FILE* logfile = fopen("/tmp/cheat-unload-debug.log", "a");
    if (logfile) {
        time_t now = time(NULL);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(logfile, "[%s] %s\n", timestamp, message);
        fclose(logfile);
    }
}

/* Enhanced unloading with debug logging */
void safe_unload_with_debug(void) {
    debug_log("Starting safe unload with debug logging");
    
    /* Log important function addresses for debugging */
    char buf[256];
    snprintf(buf, sizeof(buf), "Function addresses - self_unload: %p, unload: %p, hooks_restore: %p", 
             (void*)self_unload, (void*)unload, (void*)hooks_restore);
    debug_log(buf);
    
    /* Reset all settings to safe values */
    debug_log("Resetting all settings to default values");
    i_engine->pfnClientCmd("echo \"Step 0: Resetting all settings...\"");
    settings_reset();
    
    /* Unhook everything first in an orderly manner */
    i_engine->pfnClientCmd("echo \"Step 1: Unhooking functions...\"");
    debug_log("Unhooking all functions");
    
    /* First OpenGL hooks since they're most likely to crash if not restored */
    debug_log("Restoring OpenGL hooks");
    // Don't manually unhook GL functions, it's already handled in hooks_restore
    
    /* Then restore other hooks */
    debug_log("Restoring VMT hooks");
    hooks_restore();
    
    /* Restore globals */
    debug_log("Restoring globals");
    globals_restore();
    
    /* Remove our command */
    debug_log("Removing custom commands");
    i_engine->pfnClientCmd("echo \"Step 2: Removing custom commands...\"");
    
    /* Wait a bit to ensure all hooks are properly removed */
    debug_log("Waiting for hooks to settle");
    i_engine->pfnClientCmd("echo \"Step 3: Waiting for hooks to settle...\"");
    usleep(250000); /* 250ms */
    
    /* Now try to unload the library */
    debug_log("Beginning library unload sequence");
    i_engine->pfnClientCmd("echo \"Step 4: Unloading library...\"");
    
    /*
     * RTLD_LAZY: If the symbol is never referenced, then it is never resolved.
     * RTLD_NOLOAD: Don't load the shared object.
     */
    void* self = dlopen("libhlcheat.so", RTLD_LAZY | RTLD_NOLOAD);
    
    snprintf(buf, sizeof(buf), "dlopen result: %p", self);
    debug_log(buf);
    
    if (self) {
        /* Close the call we just made to dlopen() */
        debug_log("Closing first dlopen reference");
        int result = dlclose(self);
        snprintf(buf, sizeof(buf), "First dlclose result: %d", result);
        debug_log(buf);
        
        /* Close the call our injector made */
        debug_log("Closing second dlopen reference");
        result = dlclose(self);
        snprintf(buf, sizeof(buf), "Second dlclose result: %d", result);
        debug_log(buf);
    } else {
        debug_log("ERROR: Failed to get handle to library");
        const char* error = dlerror();
        if (error) {
            debug_log(error);
            i_engine->pfnClientCmd("echo \"ERROR: Failed to get handle to library\"");
        }
    }
    
    debug_log("Safe unload completed");
    i_engine->pfnClientCmd("echo \"Unload procedure completed\"");
}

/* Handler for dz_uninject command */
void UNINJECT_CommandHandler(void) {
    i_engine->pfnClientCmd("echo \"Uninjecting goldsource-cheat with debug logging...\"");
    
    /* Use the safe unload with debug instead of scheduling uninjection */
    safe_unload_with_debug();
}

/* Handler for dz_menu command */
void MENU_CommandHandler(void) {
    extern bool g_menu_open;
    g_menu_open = !g_menu_open;
    i_engine->Con_Printf("Menu toggled to %s\n", g_menu_open ? "open" : "closed");
}

__attribute__((constructor)) /* Entry point when injected */
void load(void) {
    printf("goldsource-cheat injected!\n");

    /* Initialize globals/interfaces */
    if (!globals_init()) {
        fprintf(stderr, "goldsource-cheat: load: error loading globals, aborting\n");
        self_unload();
        return;
    }

    /* Initialize settings with defaults */
    settings_init();

    /* Hook functions */
    if (!hooks_init()) {
        fprintf(stderr, "goldsource-cheat: load: error hooking functions, aborting\n");
        self_unload();
        return;
    }

    /* Register custom commands */
    i_engine->pfnAddCommand("dz_uninject", UNINJECT_CommandHandler);
    i_engine->pfnAddCommand("dz_menu", MENU_CommandHandler);
    
    // Bind INSERT key to dz_menu command (for menu toggle)
    i_engine->pfnClientCmd("bind INS dz_menu");
    i_engine->Con_Printf("Bound INSERT key to menu toggle\n");

    /* Play sound to indicate successful injection */
    if (IsCS16()) {
        i_engine->pfnClientCmd("play weapons/knife_deploy1.wav");
        i_engine->pfnClientCmd("speak \"Cheat successfully loaded\"");
    }
    else if (IsDayOfDefeat()) {
        i_engine->pfnClientCmd("play weapons/kar_cock.wav");
        i_engine->pfnClientCmd("speak \"Cheat successfully loaded\"");
    }
    else if (IsTFC()) {
        i_engine->pfnClientCmd("play misc/party2.wav");
        i_engine->pfnClientCmd("speak \"Cheat successfully loaded\"");
    }
    else if (IsDeathmatchClassic()) {
        i_engine->pfnClientCmd("play items/r_item1.wav");
        i_engine->pfnClientCmd("speak \"Cheat successfully loaded\"");
    }
    else
    {
        i_engine->pfnClientCmd("play weapons/cbar_hit1.wav");
        i_engine->pfnClientCmd("speak \"Cheat successfully loaded\"");
    }
    
    i_engine->pfnClientCmd("echo \"goldsource-cheat loaded successfully!\"");
    i_engine->pfnClientCmd("echo \"Deadzone rulez!\"");
    i_engine->pfnClientCmd("echo \"https://git.deadzone.lol/Wizzard/goldsrc-cheat\"");

    /* Get game version after injecting */
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
        case GAME_DMC:
            i_engine->pfnClientCmd("echo \"Detected Game: Deathmatch Classic\"");
            break;
        case GAME_SL:
            i_engine->pfnClientCmd("echo \"Detected Game: Space Life: Finley's Revenge\"");
            break;
        default:
            i_engine->pfnClientCmd("echo \"Detected Game: Unknown Game\"");
            break;
    }

    loaded = true;
}

__attribute__((destructor)) /* Entry point when unloaded */
void unload(void) {
    if (loaded) {
        /* Reset all settings to default values */
        settings_reset();
        
        globals_restore();
        hooks_restore();

        // Don't manually unhook GL functions, it's already handled in hooks_restore
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
