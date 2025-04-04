#include "../include/sdk.h"
#include "../include/settings.h"
#include "../include/util.h"
#include "../include/globals.h"
#include "../features/features.h"
#include <stdio.h>
#include <time.h>

static time_t last_log_time = 0;

void no_recoil(usercmd_t* cmd) {
    if (!is_alive(localplayer) || (!g_settings.aimbot_norecoil && !g_settings.aimbot_recoil_comp)) {
        return;
    }

    if (!(cmd->buttons & IN_ATTACK)) {
        return;
    }

    time_t current_time = time(NULL);
    if (current_time - last_log_time >= 5) {
        printf("Recoil control active: Punch Angles (X: %f, Y: %f)\n", g_punchAngles[0], g_punchAngles[1]);
        last_log_time = current_time;
    }

    if (g_settings.aimbot_norecoil) {
        float multiplier = 200.0f;
        
        cmd->viewangles[0] -= (g_punchAngles[0] * multiplier);
        cmd->viewangles[1] -= (g_punchAngles[1] * multiplier);
        
        printf("Applied no_recoil: %f, %f\n", -g_punchAngles[0] * multiplier, -g_punchAngles[1] * multiplier);
    }
    else if (g_settings.aimbot_recoil_comp) {
        float multiplier = 5.0f;
        
        cmd->viewangles[0] -= (g_punchAngles[0] * multiplier);
        cmd->viewangles[1] -= (g_punchAngles[1] * multiplier);
        
        printf("Applied recoil_comp: %f, %f\n", -g_punchAngles[0] * multiplier, -g_punchAngles[1] * multiplier);
    }
}
