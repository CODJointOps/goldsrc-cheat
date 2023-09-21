#include <math.h>
#include <cfloat>

#include "features.h"
#include "../include/sdk.h"
#include "../include/cvars.h"
#include "../include/util.h"
#include "../include/game_detection.h"

/* Game units to add to the entity origin to get the head */
#define HL1_HEAD_OFFSET 25.f
#define CS16_HEAD_OFFSET 22.f
#define CS16_HORIZONTAL_OFFSET 8.0f

/* Scale factor for aim punch */
#define AIM_PUNCH_MULT 2

static float vec_length(vec3_t v) {
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}


static bool is_visible(vec3_t start, vec3_t end) {
    pmtrace_t* tr = i_engine->PM_TraceLine(start, end, PM_TRACELINE_PHYSENTSONLY, 2, -1);
    if (tr->ent <= 0) return false;
    const int ent_idx = i_pmove->physents[tr->ent].info;
    return get_player(ent_idx) != NULL;
}

static vec3_t get_closest_delta(vec3_t viewangles) {
    // Compensate for aim punch
    viewangles.x += g_punchAngles.x * AIM_PUNCH_MULT;
    viewangles.y += g_punchAngles.y * AIM_PUNCH_MULT;
    viewangles.z += g_punchAngles.z * AIM_PUNCH_MULT;

    vec3_t view_height;
    i_engine->pEventAPI->EV_LocalPlayerViewheight(view_height);
    vec3_t local_eyes = vec_add(localplayer->origin, view_height);

    float min_distance = FLT_MAX;  // For tracking the closest player
    float best_fov = dz_aim_aimbot->value;
    vec3_t best_delta = { 0, 0, 0 };

    for (int i = 1; i <= i_engine->GetMaxClients(); i++) {
        cl_entity_t* ent = get_player(i);

        if (!is_alive(ent) || is_friend(ent)) {
            continue; // Skip if not alive or is a friend
        }

        vec3_t head_pos = ent->origin;

        if (ent->curstate.usehull != 1) {  // Not crouched
            if (IsCS16()) {
                head_pos.z += CS16_HEAD_OFFSET;
                head_pos.x += CS16_HORIZONTAL_OFFSET;
            } else {
                head_pos.z += HL1_HEAD_OFFSET;
            }
        }


        
        float distance = vec_length(vec_sub(ent->origin, local_eyes));
        if (distance > min_distance) {
            continue; // Skip players that are further than the current closest target
        }

        const vec3_t enemy_angle = vec_to_ang(vec_sub(head_pos, local_eyes));
        const vec3_t delta = vec_sub(enemy_angle, viewangles);
        vec_norm(delta);

        float fov = hypotf(delta.x, delta.y);
        if (fov > 360.0f) {
            fov = remainderf(fov, 360.0f);
        }
        if (fov > 180.0f) {
            fov = 360.0f - fov;
        }

        // Only check visibility for potential targets
        if (fov < best_fov && is_visible(local_eyes, head_pos)) {
            best_fov = fov;
            vec_copy(best_delta, delta);
            min_distance = distance;  // Update the closest target's distance
        }
    }

    return best_delta;
}

void aimbot(usercmd_t* cmd) {
    static bool shouldShootNextFrame = false;  // If you're not using this variable, you can remove it.
    static bool hasAdjustedView = false;

    if (!CVAR_ON(aim_aimbot) || !can_shoot())
        return;

    vec3_t engine_viewangles;
    i_engine->GetViewAngles(engine_viewangles);
    vec3_t best_delta = get_closest_delta(engine_viewangles);

    if (!vec_is_zero(best_delta)) {
        /*
        if (!lastShotHit) {
            // If the last shot missed, adjust the angle slightly
            // This is a simple approach and might need further tweaking
            best_delta.y += 0.5f;  // Adjust yaw slightly
        }
        */
        engine_viewangles.x += best_delta.x;
        engine_viewangles.y += best_delta.y;
        engine_viewangles.z += best_delta.z;

        if (CVAR_ON(aim_aimbot_silent)) {
            vec_copy(cmd->viewangles, engine_viewangles);
            if (cmd->buttons & IN_ATTACK) {
                cmd->buttons |= IN_ATTACK;
            }
        } else {
            if (cmd->buttons & IN_ATTACK) {  
                if (hasAdjustedView) {
                    hasAdjustedView = false; // Reset flag
                } else {
                    i_engine->SetViewAngles(engine_viewangles);
                    hasAdjustedView = true;
                    cmd->buttons &= ~IN_ATTACK; // Defer the shot to the next frame
                }
            }
        }
    } else if (CVAR_ON(aim_autoshoot) && (cmd->buttons & IN_ATTACK)) {
        cmd->buttons &= ~IN_ATTACK;
    }


}

