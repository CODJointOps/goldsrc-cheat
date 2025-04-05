#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "features.h"
#include "../include/sdk.h"
#include "../include/settings.h"
#include "../include/util.h"
#include "../include/globals.h"

#define AA_PITCH_NONE      0
#define AA_PITCH_DOWN      1
#define AA_PITCH_UP        2
#define AA_PITCH_ZERO      3
#define AA_PITCH_JITTER    4
#define AA_PITCH_CUSTOM    5

#define AA_YAW_NONE        0
#define AA_YAW_BACKWARD    1
#define AA_YAW_SPIN        2
#define AA_YAW_JITTER      3
#define AA_YAW_SIDEWAYS    4
#define AA_YAW_CUSTOM      5

static float spin_angle = 0.0f;
static float last_update_time = 0.0f;
static float jitter_next_update = 0.0f;

float random_float(float min, float max) {
    return (max - min) * ((float)rand() / (float)RAND_MAX) + min;
}

bool isSpacebarPressed() {
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        return false;
    }

    char keys_return[32];
    XQueryKeymap(display, keys_return);
    KeyCode kc = XKeysymToKeycode(display, XK_space);
    bool pressed = (keys_return[kc >> 3] & (1 << (kc & 7))) != 0;

    XCloseDisplay(display);
    return pressed;
}

bool is_key_pressed(int key_code) {
    if (key_code <= 0) {
        return false;
    }
    
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        return false;
    }

    char keys_return[32];
    XQueryKeymap(display, keys_return);
    
    KeyCode kc;
    if (key_code >= 'A' && key_code <= 'Z') {
        kc = XKeysymToKeycode(display, XK_a + (key_code - 'A'));
    } else if (key_code >= 'a' && key_code <= 'z') {
        kc = XKeysymToKeycode(display, XK_a + (key_code - 'a'));
    } else {
        switch (key_code) {
            case K_SPACE: kc = XKeysymToKeycode(display, XK_space); break;
            case K_CTRL: kc = XKeysymToKeycode(display, XK_Control_L); break;
            case K_SHIFT: kc = XKeysymToKeycode(display, XK_Shift_L); break;
            case K_ALT: kc = XKeysymToKeycode(display, XK_Alt_L); break;
            case K_TAB: kc = XKeysymToKeycode(display, XK_Tab); break;
            default: kc = XKeysymToKeycode(display, key_code);
        }
    }
    
    bool pressed = (keys_return[kc >> 3] & (1 << (kc & 7))) != 0;
    XCloseDisplay(display);
    return pressed;
}

void apply_pitch_anti_aim(vec3_t* view_angles, int pitch_mode, float custom_pitch) {
    switch (pitch_mode) {
        case AA_PITCH_DOWN:
            view_angles->x = 89.0f;
            break;
        case AA_PITCH_UP:
            view_angles->x = -89.0f;
            break;
        case AA_PITCH_ZERO:
            view_angles->x = 0.0f;
            break;
        case AA_PITCH_JITTER: {
            static bool flip_pitch = false;
            if (flip_pitch) {
                view_angles->x = 89.0f;
            } else {
                view_angles->x = -89.0f;
            }
            
            if (g_flCurrentTime > jitter_next_update) {
                flip_pitch = !flip_pitch;
                jitter_next_update = g_flCurrentTime + random_float(0.1f, 0.3f);
            }
            break;
        }
        case AA_PITCH_CUSTOM:
            view_angles->x = CLAMP(custom_pitch, -89.0f, 89.0f);
            break;
        default:
            break;
    }
}

void apply_yaw_anti_aim(vec3_t* view_angles, int yaw_mode, float custom_yaw, float speed, float jitter_range) {
    float time_now;
    float time_delta;
    static bool is_left = true;
    
    switch (yaw_mode) {
        case AA_YAW_BACKWARD:
            view_angles->y += 180.0f;
            break;
        case AA_YAW_SPIN:
            time_now = g_flCurrentTime;
            time_delta = time_now - last_update_time;
            
            spin_angle += time_delta * speed;
            
            if (spin_angle > 360.0f) {
                spin_angle -= 360.0f;
            }
            
            view_angles->y = spin_angle;
            
            last_update_time = time_now;
            break;
        case AA_YAW_JITTER:
            if (g_flCurrentTime > jitter_next_update) {
                view_angles->y += random_float(-jitter_range, jitter_range);
                jitter_next_update = g_flCurrentTime + random_float(0.1f, 0.3f);
            }
            break;
        case AA_YAW_SIDEWAYS:
            if (g_flCurrentTime > jitter_next_update) {
                is_left = !is_left;
                jitter_next_update = g_flCurrentTime + random_float(0.5f, 1.5f);
            }
            
            if (is_left) {
                view_angles->y += 90.0f;
            } else {
                view_angles->y -= 90.0f;
            }
            break;
        case AA_YAW_CUSTOM:
            view_angles->y = custom_yaw;
            break;
        default:
            break;
    }
}

void apply_lby_breaker(vec3_t* view_angles, bool enable_breaker) {
    if (!enable_breaker)
        return;
        
    static bool lby_update = false;
    static float next_lby_update = 0.0f;
    
    if (g_flCurrentTime > next_lby_update) {
        lby_update = !lby_update;
        next_lby_update = g_flCurrentTime + 1.1f;
        
        if (lby_update) {
            view_angles->y += 120.0f;
        }
    }
}

void apply_fake_duck(usercmd_t* cmd, bool enable_duck) {
    if (!enable_duck)
        return;
        
    static int duck_state = 0;
    static float next_duck_time = 0.0f;
    
    if (g_flCurrentTime > next_duck_time) {
        duck_state = (duck_state + 1) % 4;
        next_duck_time = g_flCurrentTime + 0.05f;
    }
    
    if (duck_state < 2) {
        cmd->buttons |= IN_DUCK;
    } else {
        cmd->buttons &= ~IN_DUCK;
    }
}

void anti_aim(usercmd_t* cmd) {
    if (!g_settings.antiaim_enabled) {
        return;
    }
    
    if (!is_alive(localplayer)) {
        return;
    }

    if ((cmd->buttons & IN_ATTACK) && !g_settings.antiaim_on_attack) {
        return;
    }
    
    if (cmd->buttons & IN_USE) {
        return;
    }

    vec3_t view_angles;
    i_engine->GetViewAngles(view_angles);

    if (g_settings.antiaim_pitch_enabled) {
        if (g_settings.antiaim_pitch_mode == 0) {
            view_angles.x = g_settings.antiaim_pitch;
            
            view_angles.x = CLAMP(view_angles.x, -89.0f, 89.0f);
        } else {
            apply_pitch_anti_aim(&view_angles, g_settings.antiaim_pitch_mode, g_settings.antiaim_custom_pitch);
        }
        
        static float last_debug_time = 0.0f;
        if (g_flCurrentTime > last_debug_time + 5.0f) {
            i_engine->Con_Printf("Anti-Aim: Applied pitch angle %.1f (mode %d)\n", 
                              view_angles.x, g_settings.antiaim_pitch_mode);
            last_debug_time = g_flCurrentTime;
        }
    }
    
    if (g_settings.antiaim_yaw_enabled) {
        if (g_settings.antiaim_yaw_mode == 0) {
            if (g_settings.antiaim_legit) {
                float legit_yaw_max = 35.0f;
                view_angles.y += CLAMP(g_settings.antiaim_yaw, -legit_yaw_max, legit_yaw_max);
            } else {
                view_angles.y += g_settings.antiaim_yaw;
            }
        } else {
            apply_yaw_anti_aim(&view_angles, g_settings.antiaim_yaw_mode, g_settings.antiaim_custom_yaw, 
                              g_settings.antiaim_spin_speed, g_settings.antiaim_jitter_range);
        }
        
        if (view_angles.y > 180.0f) view_angles.y -= 360.0f;
        if (view_angles.y < -180.0f) view_angles.y += 360.0f;
    }
    
    apply_lby_breaker(&view_angles, g_settings.antiaim_lby_breaker);
    
    if (g_settings.antiaim_desync) {
        static bool switch_side = false;
        static float next_switch = 0.0f;
        
        if (g_flCurrentTime > next_switch) {
            switch_side = !switch_side;
            next_switch = g_flCurrentTime + random_float(0.4f, 0.8f);
        }
        
        if (!g_settings.antiaim_view) {
            vec3_t real_angles;
            i_engine->GetViewAngles(real_angles);
            
            vec3_t server_angles;
            vec_copy(server_angles, real_angles);
            server_angles.y += (switch_side ? 58.0f : -58.0f);
            
            if (server_angles.y > 180.0f) server_angles.y -= 360.0f;
            if (server_angles.y < -180.0f) server_angles.y += 360.0f;
            
            vec_copy(cmd->viewangles, server_angles);
            i_engine->SetViewAngles(real_angles);
            
            static float last_desync_log = 0.0f;
            if (g_flCurrentTime > last_desync_log + 3.0f) {
                i_engine->Con_Printf("Desync active: Server %.1f, Client %.1f (invisible to user)\n", 
                                   server_angles.y, real_angles.y);
                last_desync_log = g_flCurrentTime;
            }
            
            return;
        }
        
        float desync_amount = switch_side ? 58.0f : -58.0f;
        view_angles.y += desync_amount;
        
        if (view_angles.y > 180.0f) view_angles.y -= 360.0f;
        if (view_angles.y < -180.0f) view_angles.y += 360.0f;
    }

    bool should_fake_duck = false;
    
    if (g_settings.antiaim_fakeduck_key > 0) {
        should_fake_duck = g_settings.antiaim_fakeduck && is_key_pressed(g_settings.antiaim_fakeduck_key);
    } else {
        should_fake_duck = g_settings.antiaim_fakeduck;
    }
    
    if (should_fake_duck) {
        apply_fake_duck(cmd, true);
    }

    if (g_settings.antiaim_view) {
        i_engine->SetViewAngles(view_angles);
    } else {
        vec_copy(cmd->viewangles, view_angles);
    }
    
    static float last_log_time = 0.0f;
    if (g_flCurrentTime > last_log_time + 5.0f) {
        i_engine->Con_Printf("Anti-Aim active: Pitch=%.1f, Yaw=%.1f, Mode=%d/%d\n", 
                           view_angles.x, view_angles.y, 
                           g_settings.antiaim_pitch_mode, g_settings.antiaim_yaw_mode);
        last_log_time = g_flCurrentTime;
    }
}