#include "../include/sdk.h"
#include "../include/settings.h"
#include "../include/globals.h"
#include "../include/util.h"
#include "thirdperson.h"
#include <stdio.h>

#define TP_MIN_DIST 30.0f
#define TP_MAX_DIST 800.0f
#define TP_DEFAULT_DIST 150.0f

#define YAW   1
#define PITCH 0
#define ROLL  2

static bool s_initialized = false;
static bool s_thirdperson_enabled = false;
static vec3_t s_camera_origin = {0, 0, 0};

#ifdef __cplusplus
extern "C" {
#endif

void thirdperson_init(void) {
    s_initialized = true;
    s_thirdperson_enabled = false;
    i_engine->Con_Printf("Third-person system initialized\n");
}

void thirdperson_toggle(void) {
    g_settings.thirdperson = !g_settings.thirdperson;
    s_thirdperson_enabled = g_settings.thirdperson;
    
    if (s_thirdperson_enabled) {
        i_engine->Con_Printf("Third-person view enabled (distance: %.1f)\n", g_settings.thirdperson_dist);
    } else {
        i_engine->Con_Printf("Third-person view disabled\n");
    }
}

bool thirdperson_key_event(int keynum, int down) {
    i_engine->Con_Printf("Thirdperson key event: keynum=%d, down=%d, configured=%d\n", 
                      keynum, down, g_settings.thirdperson_key);
    
    if (!down) {
        return false;
    }
    
    bool should_toggle = false;
    
    if (keynum == 'C' || keynum == 'c' || keynum == 67 || keynum == 99) {
        i_engine->Con_Printf("C key detected (keynum=%d)\n", keynum);
        if (g_settings.thirdperson_key == 'C' || g_settings.thirdperson_key == 'c' || 
            g_settings.thirdperson_key == 67 || g_settings.thirdperson_key == 99) {
            should_toggle = true;
        }
    }
    
    else if (keynum == g_settings.thirdperson_key) {
        i_engine->Con_Printf("Configured key detected (keynum=%d)\n", keynum);
        should_toggle = true;
    }
    
    if (should_toggle) {
        thirdperson_toggle();
        return true;
    }
    
    return false;
}

void thirdperson_modify_view(ref_params_t* pparams) {
    if (!g_settings.thirdperson) {
        return;
    }
    
    float distance = g_settings.thirdperson_dist;
    
    vec3_t forward, right, up;
    
    i_engine->pfnAngleVectors(pparams->viewangles, forward, right, up);
    
    vec3_t camera_offset;
    for (int i = 0; i < 3; i++) {
        camera_offset[i] = -forward[i] * distance;
    }
    
    vec3_t newOrigin;
    for (int i = 0; i < 3; i++) {
        newOrigin[i] = pparams->vieworg[i] + camera_offset[i];
    }
    
    vec_copy(s_camera_origin, newOrigin);
    
    pmtrace_t trace;
    i_engine->pEventAPI->EV_SetTraceHull(2);
    i_engine->pEventAPI->EV_PlayerTrace(pparams->vieworg, newOrigin, PM_NORMAL, -1, &trace);
    
    if (trace.fraction < 1.0) {
        vec3_t dir;
        for (int i = 0; i < 3; i++) {
            dir[i] = newOrigin[i] - pparams->vieworg[i];
        }
        
        float len = sqrtf(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
        if (len > 0) {
            for (int i = 0; i < 3; i++) {
                dir[i] /= len;
            }
        }
        
        vec3_t collisionPoint;
        for (int i = 0; i < 3; i++) {
            collisionPoint[i] = pparams->vieworg[i] + (trace.fraction * distance - 5.0f) * forward[i];
        }
        vec_copy(newOrigin, collisionPoint);
    }
    
    vec_copy(pparams->vieworg, newOrigin);
    
    static int debug_count = 0;
    if (++debug_count % 300 == 0) {
        i_engine->Con_Printf("Camera position: [%.1f, %.1f, %.1f], distance=%.1f\n", 
                           newOrigin[0], newOrigin[1], newOrigin[2], distance);
    }
}

void thirdperson_update(void) {
    s_thirdperson_enabled = g_settings.thirdperson;
}

int thirdperson_is_active(void) {
    return g_settings.thirdperson ? 1 : 0;
}

#ifdef __cplusplus
}
#endif 