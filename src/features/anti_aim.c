#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "features.h"
#include "../include/sdk.h"
#include "../include/cvars.h"
#include "../include/util.h"

float random_float(float min, float max) {
    return (max - min) * ((float)rand() / (float)RAND_MAX) + min;
}

void anti_aim(usercmd_t* cmd) {
    if (!CVAR_ON(movement_antiaim) || !is_alive(localplayer)) {
        return;
    }

    vec3_t random_angles;
    random_angles.x = random_float(-89.0f, 89.0f);
    random_angles.y = random_float(-180.0f, 180.0f);
    random_angles.z = 0.0f;

    if (CVAR_ON(movement_antiaim_view)) {
        i_engine->SetViewAngles(random_angles); 
    }

    vec_copy(random_angles, cmd->viewangles);

    static float last_log_time = 0.0f;
    if (cmd->msec - last_log_time >= 5000.0f) {
        i_engine->pfnClientCmd("echo \"Anti-Aim has adjusted view angles.\"");
        last_log_time = cmd->msec;
    }
}
