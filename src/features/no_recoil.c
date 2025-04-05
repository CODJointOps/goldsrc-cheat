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

#define WEAPON_GLOCK  17
#define WEAPON_DEAGLE 26
#define WEAPON_AK47   28
#define WEAPON_M4A1   22
#define WEAPON_AWP    33

#define AK47_RECOIL_VERT_MULT    2.8f
#define AK47_RECOIL_HORIZ_MULT   0.9f
#define DEFAULT_RECOIL_VERT_MULT 2.0f
#define DEFAULT_RECOIL_HORIZ_MULT 0.6f

static float ak47_pattern[] = {
    0.0f,
    2.2f,
    3.0f,
    3.5f,
    3.9f,
    4.0f,
    3.8f,
    3.5f,
    3.2f,
    2.9f,
    2.7f,
    2.5f,
    2.2f,
    2.0f,
    1.9f,
    1.7f,
    1.6f,
    1.5f,
    1.4f,
    1.3f,
    1.2f,
    1.1f,
    1.0f,
    0.9f,
    0.8f,
    0.8f,
    0.7f,
    0.7f,
    0.6f,
    0.6f
};

static int bullet_count = 0;
static float last_shot_time = 0.0f;
static int current_weapon_id = -1;

void no_recoil(usercmd_t* cmd) {
    if (!is_alive(localplayer) || (!g_settings.aimbot_norecoil && !g_settings.aimbot_recoil_comp)) {
        return;
    }

    if (!(cmd->buttons & IN_ATTACK)) {
        if (g_flCurrentTime - last_shot_time > 0.2f) {
            if (bullet_count > 0) {
                i_engine->Con_Printf("Recoil: Reset spray pattern from %d bullets\n", bullet_count);
                bullet_count = 0;
            }
        }
        return;
    }

    vec3_t current_viewangles;
    i_engine->GetViewAngles(current_viewangles);

    bool is_ak47 = false;
    bool is_high_recoil_weapon = false;
    
    if (g_currentWeaponID == WEAPON_AK47) {
        is_ak47 = true;
        is_high_recoil_weapon = true;
    } 
    else if (g_currentWeaponID == WEAPON_M4A1) {
        is_high_recoil_weapon = true;
    }
    else if (g_flNextPrimaryAttack - g_flNextAttack < 0.15f) {
        is_high_recoil_weapon = true;
        if (g_flNextPrimaryAttack - g_flNextAttack < 0.11f) {
            is_ak47 = true;
        }
    }

    last_shot_time = g_flCurrentTime;
    
    static float last_attack_time = 0.0f;
    bool is_new_shot = g_flCurrentTime - last_attack_time > 0.05f;
    
    if (is_new_shot) {
        bullet_count++;
        last_attack_time = g_flCurrentTime;
        
        if (bullet_count > 1 && (bullet_count % 3 == 0 || bullet_count <= 5)) {
            i_engine->Con_Printf("Recoil comp: Shot #%d, Weapon: %s\n", 
                bullet_count, is_ak47 ? "AK-47" : (is_high_recoil_weapon ? "High Recoil" : "Standard"));
        }
    }
    
    vec3_t punch_angles;
    vec_copy(punch_angles, g_punchAngles);
    
    float vert_mult = 0.0f;
    float horiz_mult = 0.0f;
    float pattern_compensation = 0.0f;
    
    if (g_settings.aimbot_norecoil) {
        if (is_ak47) {
            vert_mult = AK47_RECOIL_VERT_MULT;
            horiz_mult = AK47_RECOIL_HORIZ_MULT;
            
            if (bullet_count > 0 && bullet_count <= 30) {
                pattern_compensation = ak47_pattern[bullet_count-1] * 0.6f;
            }
        } 
        else if (is_high_recoil_weapon) {
            vert_mult = 2.2f;
            horiz_mult = 0.8f;
        } 
        else {
            vert_mult = 1.9f;
            horiz_mult = 0.7f;
        }
        
        cmd->viewangles[0] -= (punch_angles[0] * vert_mult);
        cmd->viewangles[1] -= (punch_angles[1] * horiz_mult);
        
        if (pattern_compensation > 0) {
            cmd->viewangles[0] -= pattern_compensation;
            
            if (is_ak47 && bullet_count <= 5) {
                i_engine->Con_Printf("AK-47 pattern comp: Shot #%d, Compensation: %.2f degrees\n",
                    bullet_count, pattern_compensation);
            }
        }
    }
    else if (g_settings.aimbot_recoil_comp) {
        if (is_ak47) {
            vert_mult = 2.0f;
            horiz_mult = 0.7f;
            
            if (bullet_count > 0 && bullet_count <= 30) {
                pattern_compensation = ak47_pattern[bullet_count-1] * 0.35f;
            }
        } 
        else if (is_high_recoil_weapon) {
            vert_mult = 1.7f;
            horiz_mult = 0.6f;
        } 
        else {
            vert_mult = 1.3f;
            horiz_mult = 0.5f;
        }
        
        cmd->viewangles[0] -= (punch_angles[0] * vert_mult);
        cmd->viewangles[1] -= (punch_angles[1] * horiz_mult);
        
        if (pattern_compensation > 0) {
            cmd->viewangles[0] -= pattern_compensation;
        }
    }
    
    time_t current_time = time(NULL);
    if (current_time - last_log_time >= 2) {
        if (bullet_count > 0) {
            i_engine->Con_Printf("Recoil control - Weapon: %s, Bullet: %d, VMult: %.1f, HMult: %.1f, Pattern: %.1f\n", 
                is_ak47 ? "AK-47" : (is_high_recoil_weapon ? "High Recoil" : "Standard"),
                bullet_count, vert_mult, horiz_mult, pattern_compensation);
        }
        
        last_log_time = current_time;
    }
    
    ang_clamp(&cmd->viewangles);
}
