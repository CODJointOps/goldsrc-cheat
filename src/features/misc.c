#include "features.h"
#include "../include/sdk.h"
#include "../include/globals.h"
#include "../include/util.h"
#include "../include/game_detection.h"
#include "../include/settings.h"

void custom_crosshair(void) {
    if (!g_settings.custom_crosshair)
        return;

    /* Get window size, and then the center. */
    int mx = game_info->m_width / 2;
    int my = game_info->m_height / 2;

    /* The real length is sqrt(2 * (len^2)) */
    const int len   = 5;
    const int gap   = 1;
    const float w   = 1;
    const rgb_t col = { 255, 255, 255 };

    /*
     *   1\ /2
     *     X
     *   3/ \4
     */
    gl_drawline(mx - gap, my - gap, mx - gap - len, my - gap - len, w, col);
    gl_drawline(mx + gap, my - gap, mx + gap + len, my - gap - len, w, col);
    gl_drawline(mx - gap, my + gap, mx - gap - len, my + gap + len, w, col);
    gl_drawline(mx + gap, my + gap, mx + gap + len, my + gap + len, w, col);
}

weapon_data_t g_currentWeapon;
static double lastTracerTime = 0;
    
static bool attackReleased = true;

void bullet_tracers(usercmd_t* cmd) {
    if (!g_settings.tracers || !is_alive(localplayer))
        return;

    if (IsCS16()) {
        if (cmd->buttons & IN_ATTACK) {
            if (!attackReleased) {
                return;
            }
            attackReleased = false;
        } else {
            attackReleased = true;
            return;
        }
        if (!can_shoot()) {
            return;
        }
    } 
    else {
        if (!(cmd->buttons & IN_ATTACK) || !can_shoot()) {
            return;
        }
    }

    /* Get player eye pos, start of tracer */
    vec3_t view_height;
    i_engine->pEventAPI->EV_LocalPlayerViewheight(view_height);
    vec3_t local_eyes = vec_add(localplayer->origin, view_height);
    
    /* Get forward vector from viewangles */
    vec3_t fwd;
    i_engine->pfnAngleVectors(cmd->viewangles, fwd, NULL, NULL);

    const int tracer_len = 3000;
    vec3_t end;
    end.x = local_eyes.x + fwd.x * tracer_len;
    end.y = local_eyes.y + fwd.y * tracer_len;
    end.z = local_eyes.z + fwd.z * tracer_len;

    /* NOTE: Change tracer settings here */
    const float w    = 0.8;
    const float time = 2;
    draw_tracer(local_eyes, end, (rgb_t){ 66, 165, 245 }, 1, w, time);
}
