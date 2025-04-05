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
    if (!ent || !out_hitbox)
        return false;

    vec_copy(out_hitbox->origin, ent->origin);
    out_hitbox->radius = 12.0f;

    if (ent->model) {
        studiohdr_t* studio = (studiohdr_t*)i_enginestudio->Mod_Extradata(ent->model);
        if (studio) {
            if (hitbox_index == HITBOX_HEAD) {
                vec3_t view_offset;
                view_offset.x = 0.0f;
                view_offset.y = 0.0f;
                view_offset.z = 28.0f;
                
                if (!ent->player) {
                    view_offset.z = 25.0f;
                }
                
                out_hitbox->origin = vec_add(ent->origin, view_offset);
                
                vec3_t head_offset;
                head_offset.x = 0.0f;
                head_offset.y = 0.0f;
                head_offset.z = 0.0f;
                
                float yaw = ent->angles.y * (M_PI / 180.0f);
                head_offset.x = cos(yaw) * 3.0f;
                head_offset.y = sin(yaw) * 3.0f;
                
                out_hitbox->origin = vec_add(out_hitbox->origin, head_offset);
                out_hitbox->radius = 5.0f;
                
                static int debug_count = 0;
                if (debug_count++ % 500 == 0) {
                    printf("Head hitbox: origin=(%.1f, %.1f, %.1f), radius=%.1f\n", 
                        out_hitbox->origin.x, out_hitbox->origin.y, out_hitbox->origin.z, out_hitbox->radius);
                }
                
                return true;
            }
            else if (hitbox_index == HITBOX_CHEST) {
                vec3_t chest_offset;
                chest_offset.x = 0.0f;
                chest_offset.y = 0.0f;
                chest_offset.z = 18.0f;
                
                out_hitbox->origin = vec_add(ent->origin, chest_offset);
                out_hitbox->radius = 10.0f;
                return true;
            }
            else if (hitbox_index == HITBOX_STOMACH) {
                vec3_t stomach_offset;
                stomach_offset.x = 0.0f;
                stomach_offset.y = 0.0f;
                stomach_offset.z = 12.0f;
                
                out_hitbox->origin = vec_add(ent->origin, stomach_offset);
                out_hitbox->radius = 10.0f;
                return true;
            }
            else if (hitbox_index == HITBOX_PELVIS) {
                vec3_t pelvis_offset;
                pelvis_offset.x = 0.0f;
                pelvis_offset.y = 0.0f;
                pelvis_offset.z = 8.0f;
                
                out_hitbox->origin = vec_add(ent->origin, pelvis_offset);
                out_hitbox->radius = 8.0f; 
                return true;
            }
        }
    }
    
    switch (hitbox_index) {
        case HITBOX_HEAD:
            out_hitbox->origin.z += 32.0f;
            out_hitbox->radius = 7.0f;
            break;
            
        case HITBOX_CHEST:
            out_hitbox->origin.z += 20.0f;
            out_hitbox->radius = 10.0f;
            break;
            
        case HITBOX_STOMACH:
            out_hitbox->origin.z += 12.0f;
            out_hitbox->radius = 10.0f;
            break;
            
        case HITBOX_PELVIS:
            out_hitbox->origin.z += 6.0f;
            out_hitbox->radius = 8.0f;
            break;
            
        default:
            out_hitbox->origin.z += 16.0f;
            out_hitbox->radius = 12.0f;
            break;
    }

    return true;
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
    
    bool original_attack_state = (cmd->buttons & IN_ATTACK) != 0;
    
    vec3_t view_height;
    i_engine->pEventAPI->EV_LocalPlayerViewheight(view_height);
    vec3_t eye_pos = vec_add(localplayer->origin, view_height);
    
    vec3_t engine_viewangles;
    i_engine->GetViewAngles(engine_viewangles);
    
    // Simplified recoil detection - we'll let no_recoil.c handle actual compensation
    bool is_firing = original_attack_state && can_shoot();
    
    // Just track firing for debugging
    if (is_firing) {
        s_firing_frames++;
    } else {
        if (s_firing_frames > 0) {
            s_firing_frames = 0;
        }
    }
    
    vec_copy(s_last_viewangles, engine_viewangles);
    
    target_t best_target = get_best_target(engine_viewangles, eye_pos);
    bool valid_target_found = (best_target.entity && best_target.is_visible);
    
    bool can_fire = can_shoot();
    
    static int frame_counter = 0;
    bool should_debug = (frame_counter++ % 100 == 0);
    
    if (should_debug) {
        printf("Aimbot: Target=%d, CanFire=%d, FOV=%.1f, Frames=%d\n", 
              valid_target_found, can_fire, 
              valid_target_found ? best_target.fov : 0.0f,
              s_firing_frames);
    }
    
    if (!valid_target_found) {
        if (g_settings.aimbot_autoshoot) {
            cmd->buttons &= ~IN_ATTACK;
        }
        return;
    }
    
    vec3_t to_target = vec_sub(best_target.aim_point, eye_pos);
    vec3_t aim_angles = vec_to_ang(to_target);
    
    ang_clamp(&aim_angles);
    
    vec3_t delta = vec_sub(aim_angles, engine_viewangles);
    vec_norm(&delta);
    
    float aim_error = sqrtf(delta.x * delta.x + delta.y * delta.y);
    bool aimed_accurately = (aim_error < 2.5f);
    
    static vec3_t smooth_angles = {0, 0, 0};
    static bool tracking_active = false;
    static float tracking_time = 0.0f;
    
    if (!tracking_active && valid_target_found) {
        vec_copy(smooth_angles, engine_viewangles);
        tracking_active = true;
        tracking_time = 0.0f;
    } else if (tracking_active && valid_target_found) {
        tracking_time += 0.016f;
        tracking_time = fminf(tracking_time, 1.0f);
    } else if (tracking_active && !valid_target_found) {
        tracking_active = false;
    }
    
    if (g_settings.aimbot_silent) {
        cmd->viewangles.x = aim_angles.x;
        cmd->viewangles.y = aim_angles.y;
        cmd->viewangles.z = 0;
    } else {
        if (g_settings.aimbot_smoothing_enabled && tracking_active) {
            float smoothing = g_settings.aimbot_smooth;
            if (smoothing <= 0.1f) smoothing = 0.1f;
            
            float base_factor = 1.0f / (smoothing * 1.5f);
            float adaptive_factor = base_factor * (1.0f - (tracking_time * 0.5f));
            
            float ease_factor = fminf(1.0f, adaptive_factor);
            float t = 1.0f - pow(1.0f - ease_factor, 3);
            
            float pitch_diff = aim_angles.x - smooth_angles.x;
            if (pitch_diff > 180.0f) pitch_diff -= 360.0f;
            if (pitch_diff < -180.0f) pitch_diff += 360.0f;
            smooth_angles.x += pitch_diff * t;
            
            float yaw_diff = aim_angles.y - smooth_angles.y;
            if (yaw_diff > 180.0f) yaw_diff -= 360.0f;
            if (yaw_diff < -180.0f) yaw_diff += 360.0f;
            smooth_angles.y += yaw_diff * t;
            
            smooth_angles.z = 0;
            
            if (smooth_angles.x > 180.0f) smooth_angles.x -= 360.0f;
            if (smooth_angles.x < -180.0f) smooth_angles.x += 360.0f;
            if (smooth_angles.y > 180.0f) smooth_angles.y -= 360.0f;
            if (smooth_angles.y < -180.0f) smooth_angles.y += 360.0f;
            
            engine_viewangles = smooth_angles;
        } else {
            engine_viewangles = aim_angles;
            vec_copy(smooth_angles, aim_angles);
        }
        
        i_engine->SetViewAngles(engine_viewangles);
        
        cmd->viewangles.x = engine_viewangles.x;
        cmd->viewangles.y = engine_viewangles.y;
        cmd->viewangles.z = 0;
    }
    
    if (can_fire) {
        if (g_settings.aimbot_autoshoot && aimed_accurately) {
            cmd->buttons |= IN_ATTACK;
            if (should_debug) printf("AUTO-FIRING (error: %.2f)\n", aim_error);
        }
        else if (g_settings.aimbot_require_key) {
            if (original_attack_state) {
                if (aimed_accurately) {
                    cmd->buttons |= IN_ATTACK;
                    if (should_debug) printf("MANUAL-FIRING with key (error: %.2f)\n", aim_error);
                } else {
                    cmd->buttons &= ~IN_ATTACK;
                }
            } else {
                cmd->buttons &= ~IN_ATTACK;
            }
        }
        else if (!g_settings.aimbot_require_key) {
            if (should_debug && original_attack_state) 
                printf("PRESERVING manual fire\n");
        }
    } else {
        if (g_settings.aimbot_autoshoot || g_settings.aimbot_require_key) {
            cmd->buttons &= ~IN_ATTACK;
        }
        
        if (should_debug && original_attack_state) {
            printf("CAN'T FIRE: NextAttack=%.3f, Primary=%.3f\n", 
                  g_flNextAttack, g_flNextPrimaryAttack);
        }
    }
}