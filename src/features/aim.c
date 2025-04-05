#include <math.h>
#include <string.h>

#include "../include/sdk.h"
#include "../include/util.h"
#include "../include/entityutil.h"
#include "../include/mathutil.h"
#include "../include/menu.h"
#include "../include/settings.h"
#include "features.h"

#define HITBOX_HEAD      0
#define HITBOX_CHEST     1
#define HITBOX_STOMACH   2
#define HITBOX_PELVIS    3
#define HITBOX_LEFTARM   4
#define HITBOX_RIGHTARM  5
#define HITBOX_LEFTLEG   6
#define HITBOX_RIGHTLEG  7
#define HITBOX_NEAREST   4
#define MAX_HITBOXES     7

#define AIM_PUNCH_MULT 2.0f

#define SMOOTHING_FACTOR 3.0f

#define PITCH 0
#define YAW   1
#define ROLL  2

#define PRIORITY_NONE   0
#define PRIORITY_LOW    1
#define PRIORITY_MEDIUM 2 
#define PRIORITY_HIGH   3

// Weapon IDs
#define WEAPON_GLOCK  17
#define WEAPON_DEAGLE 26
#define WEAPON_AK47   28
#define WEAPON_M4A1   22
#define WEAPON_AWP    33

extern const char* hitbox_options[];
extern int current_hitbox;

const char* hitbox_names[] = {
    "Head",
    "Chest", 
    "Stomach",
    "Pelvis",
    "Nearest Point"
};

int current_hitbox = HITBOX_HEAD;

typedef struct {
    vec3_t mins;
    vec3_t maxs;
    vec3_t origin;
    float radius;
} hitbox_t;

bool get_hitbox(cl_entity_t* ent, int hitbox_id, hitbox_t* out_hitbox) {
    if (!ent || !out_hitbox) {
        return false;
    }

    out_hitbox->radius = 5.0f;
    
    bool is_ak47 = (g_currentWeaponID == WEAPON_AK47);
    
    studiohdr_t* studio = NULL;
    if (ent->model) {
        studio = (studiohdr_t*)i_enginestudio->Mod_Extradata(ent->model);
    }
    
    switch (hitbox_id) {
        case 0: {
            vec3_t view_offset;
            view_offset.x = 0.0f;
            view_offset.y = 0.0f;
            
            if (is_ak47) {
                view_offset.z = 26.0f;
            } else {
                view_offset.z = 27.0f;
            }
            
            out_hitbox->origin = vec_add(ent->origin, view_offset);
            out_hitbox->radius = 7.0f;
            
            vec3_t head_offset;
            head_offset.x = 0.0f;
            head_offset.y = 0.0f;
            head_offset.z = 0.0f;
            
            if (is_ak47) {
                head_offset.z = -1.0f;
            }
            
            out_hitbox->origin = vec_add(out_hitbox->origin, head_offset);
            
            static int debug_counter = 0;
            if (debug_counter++ % 500 == 0) {
                i_engine->Con_Printf("Head hitbox: Origin(%.1f,%.1f,%.1f) Radius(%.1f) WeaponID: %d\n", 
                    out_hitbox->origin.x, out_hitbox->origin.y, out_hitbox->origin.z, 
                    out_hitbox->radius, g_currentWeaponID);
            }
            
            return true;
        }
        case 1: {
            vec3_t chest_offset;
            chest_offset.x = 0.0f;
            chest_offset.y = 0.0f;
            chest_offset.z = 18.0f;
            
            out_hitbox->origin = vec_add(ent->origin, chest_offset);
            out_hitbox->radius = 10.0f;
            return true;
        }
        case 2: {
            vec3_t stomach_offset;
            stomach_offset.x = 0.0f;
            stomach_offset.y = 0.0f;
            stomach_offset.z = 12.0f;
            
            out_hitbox->origin = vec_add(ent->origin, stomach_offset);
            out_hitbox->radius = 9.0f;
            return true;
        }
        case 3: {
            vec3_t pelvis_offset;
            pelvis_offset.x = 0.0f;
            pelvis_offset.y = 0.0f;
            pelvis_offset.z = 6.0f;
            
            out_hitbox->origin = vec_add(ent->origin, pelvis_offset);
            out_hitbox->radius = 8.0f;
            return true;
        }
        default:
            if (is_ak47) {
                vec3_t fallback_offset;
                fallback_offset.x = 0.0f;
                fallback_offset.y = 0.0f;
                fallback_offset.z = 20.0f;
                
                out_hitbox->origin = vec_add(ent->origin, fallback_offset);
            } else {
                vec3_t fallback_offset;
                fallback_offset.x = 0.0f;
                fallback_offset.y = 0.0f;
                fallback_offset.z = 25.0f;
                
                out_hitbox->origin = vec_add(ent->origin, fallback_offset);
            }
            out_hitbox->radius = 8.0f;
            return true;
    }
}

bool is_hitbox_visible(vec3_t eye_pos, hitbox_t* hitbox) {
    if (!hitbox)
        return false;
    
    pmtrace_t* trace = i_engine->PM_TraceLine(eye_pos, hitbox->origin, PM_TRACELINE_PHYSENTSONLY, 2, -1);
    
    static int trace_debug = 0;
    if (trace_debug++ % 500 == 0) {
        printf("Trace: fraction=%.3f, ent=%d\n", trace->fraction, trace->ent);
    }
    
    if (trace->fraction < 0.9f) {
        if (trace->ent <= 0) {
            return false;
        }
        
        const int ent_idx = i_pmove->physents[trace->ent].info;
        cl_entity_t* hit_entity = get_player(ent_idx);
        
        if (hit_entity) {
            return true;
        }
        
        return false;
    }
    
    return true;
}

typedef struct {
    cl_entity_t* entity;
    float fov;
    vec3_t aim_point;
    bool is_visible;
    int priority;
    float distance;
} target_t;

int get_target_priority(cl_entity_t* ent) {
    if (!ent)
        return PRIORITY_NONE;
    
    return PRIORITY_MEDIUM;
}

static target_t get_best_target(vec3_t viewangles, vec3_t eye_pos) {
    target_t best_target = {NULL, 0.0f, {0, 0, 0}, false, PRIORITY_NONE, 9999.0f};
    float best_score = 0.0f;
    float max_fov = g_settings.aimbot_fov;
    
    for (int i = 1; i <= i_engine->GetMaxClients(); i++) {
        cl_entity_t* ent = get_player(i);
        
        if (!ent || !is_alive(ent))
            continue;
            
        if (!g_settings.aimbot_team_attack && is_friend(ent))
            continue;
        
        hitbox_t target_hitbox;
        bool hitbox_found = false;
        
        if (current_hitbox == HITBOX_NEAREST) {
            const int hitbox_priority[] = {HITBOX_HEAD, HITBOX_CHEST, HITBOX_STOMACH, HITBOX_PELVIS};
            
            for (int h = 0; h < 4; h++) {
                if (get_hitbox(ent, hitbox_priority[h], &target_hitbox)) {
                    if (is_hitbox_visible(eye_pos, &target_hitbox)) {
                        hitbox_found = true;
                        break;
                    }
                }
            }
        } else {
            hitbox_found = get_hitbox(ent, current_hitbox, &target_hitbox);
            if (hitbox_found) {
                hitbox_found = is_hitbox_visible(eye_pos, &target_hitbox);
            }
        }
        
        if (!hitbox_found)
            continue;
        
        vec3_t to_target = vec_sub(target_hitbox.origin, eye_pos);
        vec3_t aim_angles = vec_to_ang(to_target);
        
        vec3_t angle_delta = vec_sub(aim_angles, viewangles);
        vec_norm(&angle_delta);
        ang_clamp(&angle_delta);
        
        float fov_distance = sqrtf(angle_delta.x * angle_delta.x + angle_delta.y * angle_delta.y);
        
        if (fov_distance > max_fov)
            continue;
        
        float distance = sqrtf(to_target.x * to_target.x + to_target.y * to_target.y + to_target.z * to_target.z);
            
        float fov_score = 1.0f - (fov_distance / max_fov);
        
        int priority = get_target_priority(ent);
        float priority_score = priority / (float)PRIORITY_HIGH;
        
        float distance_score = 1.0f - fmin(1.0f, distance / 3000.0f);
        
        float final_score = (fov_score * 0.6f) + (priority_score * 0.3f) + (distance_score * 0.1f);
        
        if (final_score > best_score) {
            best_score = final_score;
            best_target.entity = ent;
            best_target.fov = fov_distance;
            vec_copy(best_target.aim_point, target_hitbox.origin);
            best_target.is_visible = true;
            best_target.priority = priority;
            best_target.distance = distance;
        }
    }
    
    return best_target;
}

#define RECOIL_DETECTION_MULT 1.0f
#define RECOIL_DECAY_RATE 0.5f

static vec3_t s_last_viewangles = {0, 0, 0};
static vec3_t s_punch_angles = {0, 0, 0};
static int s_firing_frames = 0;

void aimbot(usercmd_t* cmd) {
    if (!g_settings.aimbot_enabled)
        return;

    if (!is_alive(localplayer))
        return;

    if (g_settings.aimbot_require_key && !(cmd->buttons & IN_ATTACK))
        return;
        
    bool can_fire = g_flCurrentTime >= g_flNextPrimaryAttack;
    
    vec3_t eye_pos;
    vec_copy(eye_pos, localplayer->origin);
    eye_pos.z += 28.0f;
    
    vec3_t viewangles;
    i_engine->GetViewAngles(viewangles);
    
    static int shot_count = 0;
    static float last_shot_time = 0.0f;
    
    if (cmd->buttons & IN_ATTACK) {
        if (g_flCurrentTime - last_shot_time > 0.08f) {
            shot_count++;
            last_shot_time = g_flCurrentTime;
            
            if (shot_count % 5 == 0) {
                i_engine->Con_Printf("Shot counter: %d shots in spray\n", shot_count);
            }
        }
    } else {
        if (g_flCurrentTime - last_shot_time > 0.25f) {
            if (shot_count > 3) {
                i_engine->Con_Printf("Reset shot counter from %d\n", shot_count);
            }
            shot_count = 0;
        }
    }
    
    if (cmd->buttons & IN_ATTACK) {
        if (g_currentWeaponID == WEAPON_AK47) {
            viewangles.x -= g_punchAngles[0] * 1.2f;
            viewangles.y -= g_punchAngles[1] * 0.8f;
            
            if (shot_count >= 3) {
                float extra_comp = CLAMP((shot_count - 2) * 0.3f, 0.0f, 3.0f);
                viewangles.x -= extra_comp;
                
                if (shot_count % 5 == 0) {
                    i_engine->Con_Printf("AK-47 Extra comp: %.1f degrees down\n", extra_comp);
                }
            }
        } else {
            viewangles.x -= g_punchAngles[0] * 0.6f;
            viewangles.y -= g_punchAngles[1] * 0.4f;
        }
    }
    
    target_t best_target = get_best_target(viewangles, eye_pos);
    
    if (!best_target.entity) {
        return;
    }
    
    vec3_t aim_direction = vec_sub(best_target.aim_point, eye_pos);
    vec3_t aim_angles = vec_to_ang(aim_direction);
    
    if (g_currentWeaponID == WEAPON_AK47) {
        if (shot_count > 0) {
            float ak_compensation = CLAMP(shot_count * 0.25f, 0.0f, 6.0f);
            
            aim_angles.x -= ak_compensation;
            
            if (shot_count % 5 == 0) {
                i_engine->Con_Printf("AK-47 aim adjust: Shot %d, Aiming %.1f lower\n", 
                    shot_count, ak_compensation);
            }
        }
    }
    
    vec3_t engine_viewangles;
    i_engine->GetViewAngles(engine_viewangles);
    
    if (g_settings.aimbot_smoothing_enabled && !g_settings.aimbot_silent) {
        vec3_t delta;
        delta.x = aim_angles.x - engine_viewangles.x;
        delta.y = aim_angles.y - engine_viewangles.y;
        delta.z = 0.0f;
        
        if (delta.y > 180.0f) delta.y -= 360.0f;
        if (delta.y < -180.0f) delta.y += 360.0f;
        
        float smooth_factor = g_settings.aimbot_smooth;
        
        if (g_currentWeaponID == WEAPON_AK47 && shot_count > 1) {
            smooth_factor *= 0.7f;
        }
        
        if (best_target.fov < g_settings.aimbot_fov * 0.5f) {
            smooth_factor *= 0.6f;
        }
        
        if (smooth_factor < 1.0f) smooth_factor = 1.0f;
        
        vec3_t smooth_angles;
        smooth_angles.x = engine_viewangles.x + delta.x / smooth_factor;
        smooth_angles.y = engine_viewangles.y + delta.y / smooth_factor;
        smooth_angles.z = 0.0f;
        
        ang_clamp(&smooth_angles);
        
        engine_viewangles = smooth_angles;
        
        i_engine->SetViewAngles(engine_viewangles);
    } 
    else if (g_settings.aimbot_silent) {
        vec_copy(cmd->viewangles, aim_angles);
    } 
    else {
        engine_viewangles = aim_angles;
        i_engine->SetViewAngles(engine_viewangles);
    }
    
    if (g_settings.aimbot_autoshoot && best_target.is_visible && can_fire) {
        cmd->buttons |= IN_ATTACK;
    }
}