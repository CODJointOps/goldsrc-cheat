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

bool get_hitbox(cl_entity_t* ent, int hitbox_index, hitbox_t* out_hitbox) {
    if (!ent || !ent->model || !out_hitbox)
        return false;

    studiohdr_t* studio_model = (studiohdr_t*)i_enginestudio->Mod_Extradata(ent->model);
    if (!studio_model)
        return false;
    
    mstudiobbox_t* studio_box = NULL;
    if (studio_model->hitboxindex) {
        studio_box = (mstudiobbox_t*)((byte*)studio_model + studio_model->hitboxindex);
        if (hitbox_index >= studio_model->numhitboxes || hitbox_index < 0)
            return false;
            
        studio_box += hitbox_index;
    } else {
        return false;
    }
    
    vec_copy(out_hitbox->mins, studio_box->bbmin);
    vec_copy(out_hitbox->maxs, studio_box->bbmax);
    
    vec3_t center;
    center.x = (out_hitbox->mins.x + out_hitbox->maxs.x) * 0.5f;
    center.y = (out_hitbox->mins.y + out_hitbox->maxs.y) * 0.5f;
    center.z = (out_hitbox->mins.z + out_hitbox->maxs.z) * 0.5f;
    
    vec3_t angles;
    vec_copy(angles, ent->angles);
    
    int bone = studio_box->bone;
    if (bone >= 0 && bone < studio_model->numbones) {
        mstudiobone_t* pbone = (mstudiobone_t*)((byte*)studio_model + studio_model->boneindex);
        pbone += bone;
        
        angles[0] = pbone->value[0]; 
        angles[1] = pbone->value[1];
        angles[2] = pbone->value[2];
    }
    
    vec3_t forward, right, up;
    i_engine->pfnAngleVectors(angles, forward, right, up);
    
    vec3_t offset;
    offset.x = center.x * forward.x + center.y * right.x + center.z * up.x;
    offset.y = center.x * forward.y + center.y * right.y + center.z * up.y;
    offset.z = center.x * forward.z + center.y * right.z + center.z * up.z;
    
    out_hitbox->origin = vec_add(ent->origin, offset);
    
    if (hitbox_index == HITBOX_HEAD) {
        out_hitbox->origin.z += 8.0f;
    }
    
    vec3_t size;
    size.x = fabs(out_hitbox->maxs.x - out_hitbox->mins.x) * 0.5f;
    size.y = fabs(out_hitbox->maxs.y - out_hitbox->mins.y) * 0.5f;
    size.z = fabs(out_hitbox->maxs.z - out_hitbox->mins.z) * 0.5f;
    
    out_hitbox->radius = sqrtf(size.x * size.x + size.y * size.y + size.z * size.z);
    
    return true;
}

bool is_hitbox_visible(vec3_t eye_pos, hitbox_t* hitbox) {
    if (!hitbox)
        return false;
        
    pmtrace_t* trace = i_engine->PM_TraceLine(eye_pos, hitbox->origin, PM_TRACELINE_PHYSENTSONLY, 2, -1);
    
    if (g_settings.aimbot_rage_mode && trace->fraction > 0.5f) {
        return true;
    }
    
    if (trace->fraction < 1.0f && trace->ent <= 0)
        return false;
    
    if (trace->ent > 0) {
        const int ent_idx = i_pmove->physents[trace->ent].info;
        if (get_player(ent_idx))
            return true;
    }
    
    return false;
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
    
    if (g_settings.aimbot_rage_mode) {
        return PRIORITY_HIGH;
    }
    
    return PRIORITY_MEDIUM;
}

static bool get_best_hitbox(cl_entity_t* ent, vec3_t eye_pos, hitbox_t* out_hitbox) {
    if (!ent || !out_hitbox)
        return false;
    
    if (current_hitbox == HITBOX_NEAREST) {
        float best_distance = 9999.0f;
        bool found_hitbox = false;
        
        for (int i = 0; i < MAX_HITBOXES; i++) {
            hitbox_t temp_hitbox;
            if (get_hitbox(ent, i, &temp_hitbox)) {
                if (is_hitbox_visible(eye_pos, &temp_hitbox)) {
                    vec3_t to_hitbox = vec_sub(temp_hitbox.origin, eye_pos);
                    float distance = vec_len2d(to_hitbox);
                    
                    if (distance < best_distance) {
                        best_distance = distance;
                        *out_hitbox = temp_hitbox;
                        found_hitbox = true;
                    }
                }
            }
        }
        
        return found_hitbox;
    } 
    else {
        if (get_hitbox(ent, current_hitbox, out_hitbox)) {
            return is_hitbox_visible(eye_pos, out_hitbox);
        }
    }
    
    return false;
}

static target_t get_best_target(vec3_t viewangles, vec3_t eye_pos) {
    target_t best_target = {NULL, 0.0f, {0, 0, 0}, false, PRIORITY_NONE, 9999.0f};
    float best_score = 0.0f;
    float max_fov = g_settings.aimbot_fov;
    
    if (g_settings.aimbot_rage_mode && max_fov < 90.0f) {
        max_fov = 90.0f;
    }
    
    for (int i = 1; i <= i_engine->GetMaxClients(); i++) {
        cl_entity_t* ent = get_player(i);
        
        if (!ent || !is_alive(ent))
            continue;
            
        if (!g_settings.aimbot_friendly_fire && is_friend(ent))
            continue;
        
        hitbox_t target_hitbox;
        bool hitbox_found = false;
        
        hitbox_found = get_best_hitbox(ent, eye_pos, &target_hitbox);
        
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
        
        float priority_score = 0.0f;
        if (g_settings.aimbot_rage_mode) {
            int priority = get_target_priority(ent);
            priority_score = priority / (float)PRIORITY_HIGH;
        }
        
        float final_score = fov_score;
        if (g_settings.aimbot_rage_mode) {
            final_score = (fov_score * 0.5f) + (priority_score * 0.5f);
        }
        
        if (final_score > best_score) {
            best_score = final_score;
            best_target.entity = ent;
            best_target.fov = fov_distance;
            vec_copy(best_target.aim_point, target_hitbox.origin);
            best_target.is_visible = true;
            best_target.priority = get_target_priority(ent);
            best_target.distance = distance;
        }
    }
    
    return best_target;
}

void aimbot(usercmd_t* cmd) {
    if (!g_settings.aimbot_enabled)
        return;
    
    bool should_run_aimbot = true;
    bool should_autoshoot = g_settings.aimbot_autoshoot;
    bool fire_button_pressed = (cmd->buttons & IN_ATTACK) != 0;
    
    switch (0) {
        case 0:
            should_run_aimbot = true;
            break;
        case 1:
            should_run_aimbot = (cmd->buttons & IN_ATTACK) != 0;
            break;
        case 2:
            should_run_aimbot = (cmd->buttons & IN_ATTACK2) != 0;
            break;
        default:
            should_run_aimbot = true;
    }
    
    if (!should_run_aimbot && !g_settings.aimbot_rage_mode)
        return;
    
    if (g_settings.aimbot_rage_mode)
        should_run_aimbot = true;
    
    bool can_fire = can_shoot();
    
    vec3_t view_height;
    i_engine->pEventAPI->EV_LocalPlayerViewheight(view_height);
    vec3_t eye_pos = vec_add(localplayer->origin, view_height);
    
    vec3_t engine_viewangles;
    i_engine->GetViewAngles(engine_viewangles);
    
    vec3_t adjusted_viewangles = engine_viewangles;
    if (g_settings.aimbot_norecoil) {
        adjusted_viewangles.x += g_punchAngles.x * AIM_PUNCH_MULT;
        adjusted_viewangles.y += g_punchAngles.y * AIM_PUNCH_MULT;
        adjusted_viewangles.z += g_punchAngles.z * AIM_PUNCH_MULT;
    }
    
    target_t best_target = get_best_target(adjusted_viewangles, eye_pos);
    
    if (best_target.entity && best_target.is_visible) {
        vec3_t to_target = vec_sub(best_target.aim_point, eye_pos);
        vec3_t aim_angles = vec_to_ang(to_target);
        
        vec3_t delta = vec_sub(aim_angles, engine_viewangles);
        vec_norm(&delta);
        ang_clamp(&delta);
        
        if (g_settings.aimbot_silent) {
            cmd->viewangles.x = engine_viewangles.x + delta.x;
            cmd->viewangles.y = engine_viewangles.y + delta.y;
            cmd->viewangles.z = engine_viewangles.z + delta.z;
        } else {
            float smoothing = SMOOTHING_FACTOR;
            
            smoothing = g_settings.aimbot_smooth > 0 ? g_settings.aimbot_smooth : SMOOTHING_FACTOR;
            
            if (g_settings.aimbot_rage_mode) {
                smoothing = 1.2f;
            }
            
            engine_viewangles.x += delta.x / smoothing;
            engine_viewangles.y += delta.y / smoothing;
            engine_viewangles.z += delta.z / smoothing;
            
            ang_clamp(&engine_viewangles);
            
            i_engine->SetViewAngles(engine_viewangles);
            
            cmd->viewangles.x = engine_viewangles.x;
            cmd->viewangles.y = engine_viewangles.y;
            cmd->viewangles.z = engine_viewangles.z;
        }
        
        if (should_autoshoot && can_fire) {
            if (!g_settings.aimbot_require_key || fire_button_pressed) {
                if (g_settings.aimbot_rage_mode) {
                    cmd->buttons |= IN_ATTACK;
                } else {
                    float aim_error = sqrtf(delta.x * delta.x + delta.y * delta.y);
                    if (aim_error < 5.0f) {
                        cmd->buttons |= IN_ATTACK;
                    }
                }
            }
        }
    } else if (should_autoshoot) {
        cmd->buttons &= ~IN_ATTACK;
    }
}