#include "../include/sdk.h"
#include "../include/settings.h"
#include "../include/util.h"
#include "../include/globals.h"
#include <stdio.h> // For printf
#include <time.h>  // For time

static time_t last_log_time = 0;

void no_recoil(usercmd_t* cmd) {
    if (!g_settings.aimbot_norecoil || !is_alive(localplayer)) {
        return;
    }

    time_t current_time = time(NULL);
    if (current_time - last_log_time >= 5) {
        printf("Applying anti-recoil: Punch Angles (X: %f, Y: %f)\n", g_punchAngles[0], g_punchAngles[1]);
        last_log_time = current_time;
    }

    float anti_recoil_value = 100.0f;
    cmd->viewangles[0] -= (g_punchAngles[0] * anti_recoil_value);
    cmd->viewangles[1] -= (g_punchAngles[1] * anti_recoil_value);
}
