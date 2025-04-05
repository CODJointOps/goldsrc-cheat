#include "../include/sdk.h"
#include "../include/settings.h"
#include "../include/util.h"
#include "../include/globals.h"
#include "../features/features.h"
#include <stdio.h>
#include <time.h>

static time_t last_log_time = 0;
static vec3_t last_punch = {0, 0, 0};

static vec3_t previous_viewangles = {0, 0, 0};

#define AK47_RECOIL_MULT 500.0f
#define DEFAULT_RECOIL_MULT 300.0f

void no_recoil(usercmd_t* cmd) {
    if (!is_alive(localplayer) || (!g_settings.aimbot_norecoil && !g_settings.aimbot_recoil_comp)) {
        return;
    }

    vec3_t current_viewangles;
    i_engine->GetViewAngles(current_viewangles);

    vec3_t angle_change;
    angle_change.x = current_viewangles.x - previous_viewangles.x;
    angle_change.y = current_viewangles.y - previous_viewangles.y;
    angle_change.z = current_viewangles.z - previous_viewangles.z;

    vec_copy(previous_viewangles, current_viewangles);

    if (cmd->buttons & IN_ATTACK) {
        bool applied = false;
        float multiplier = 0;

        vec3_t punch_angles;
        vec_copy(punch_angles, g_punchAngles);

        bool is_high_recoil_weapon = false;
        
        if (g_flNextPrimaryAttack - g_flNextAttack < 0.15f) {
            is_high_recoil_weapon = true;
        }

        if (g_settings.aimbot_norecoil) {
            if (is_high_recoil_weapon) {
                multiplier = AK47_RECOIL_MULT;
            } else {
                multiplier = DEFAULT_RECOIL_MULT;
            }
            
            static int shot_counter = 0;
            if (is_high_recoil_weapon) {
                if (shot_counter < 3) {
                    punch_angles[0] *= 1.2f;
                    shot_counter++;
                }
            } else {
                shot_counter = 0;
            }
            
            cmd->viewangles[0] -= (punch_angles[0] * multiplier);
            cmd->viewangles[1] -= (punch_angles[1] * multiplier * 0.7f);  // Less horizontal comp
            
            static int sustained_fire = 0;
            if (punch_angles[0] > 0.001f || punch_angles[1] > 0.001f) {
                sustained_fire++;
                
                if (sustained_fire > 4 && is_high_recoil_weapon) {
                    cmd->viewangles[0] -= 0.3f * (sustained_fire * 0.1f);
                }
            } else {
                sustained_fire = 0;
            }
            
            applied = true;
        }
        else if (g_settings.aimbot_recoil_comp) {
            if (is_high_recoil_weapon) {
                multiplier = 40.0f;
            } else {
                multiplier = 20.0f;
            }
            
            cmd->viewangles[0] -= (punch_angles[0] * multiplier);
            cmd->viewangles[1] -= (punch_angles[1] * multiplier * 0.6f);
            
            applied = true;
        }

        time_t current_time = time(NULL);
        if (applied && current_time - last_log_time >= 2) {
            printf("Applied recoil control - Weapon: %s, Multiplier: %.1f, PunchX: %.3f, PunchY: %.3f, Compensation: (%.2f, %.2f)\n", 
                   is_high_recoil_weapon ? "High Recoil" : "Standard",
                   multiplier, punch_angles[0], punch_angles[1], 
                   -punch_angles[0] * multiplier, -punch_angles[1] * multiplier);
            
            last_log_time = current_time;
        }
    }
    
    ang_clamp(&cmd->viewangles);
}
