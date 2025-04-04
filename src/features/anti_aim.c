#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "features.h"
#include "../include/sdk.h"
#include "../include/settings.h"
#include "../include/util.h"

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

void anti_aim(usercmd_t* cmd) {
    if (cmd->buttons & IN_ATTACK || cmd->buttons & IN_USE) {
        if (cmd->buttons & IN_ATTACK) {
            i_engine->pfnClientCmd("echo \"Attack detected. Spinbot stopped.\"");
        } else if (cmd->buttons & IN_USE) {
            i_engine->pfnClientCmd("echo \"Use key detected. Spinbot stopped.\"");
        }
        return;
    }

    if (!g_settings.antiaim) {
        return;
    }
    
    if (!is_alive(localplayer)) {
        return;
    }

    vec3_t view_angles;
    i_engine->GetViewAngles(view_angles);

    static bool lbyBreak = false;
    if (lbyBreak) {
        view_angles.y += 120.0f;
    }
    lbyBreak = !lbyBreak;

    static bool flipPitch = false;
    if (flipPitch) {
        view_angles.x = 89.0f;
    } else {
        view_angles.x = -89.0f;
    }
    flipPitch = !flipPitch;

    view_angles.y += 30.0f;
    
    bool isBunnyHopping = cmd->buttons & IN_JUMP;
    bool isStationary = (cmd->forwardmove == 0.0f && cmd->sidemove == 0.0f);
    
    if (g_settings.fakeduck && (isStationary || isBunnyHopping || isSpacebarPressed())) {
        static int duckCounter = 0;
        if (duckCounter < 2) {
            cmd->buttons |= IN_DUCK;
        } else if (duckCounter < 4) {
            cmd->buttons &= ~IN_DUCK;
        } else {
            duckCounter = 0;
        }
        duckCounter++;
    }

    if (view_angles.y > 180.0f) view_angles.y -= 360.0f;
    if (view_angles.y < -180.0f) view_angles.y += 360.0f;

    if (g_settings.antiaim_view) {
        i_engine->SetViewAngles(view_angles);
        i_engine->pfnClientCmd("echo \"Set view angles directly using movement_antiaim_view.\"");
    } else {
        vec_copy(cmd->viewangles, view_angles);
        i_engine->pfnClientCmd("echo \"Set view angles silently.\"");
    }

    static float last_log_time = 0.0f;
    if (cmd->msec - last_log_time >= 5000.0f) {
        i_engine->pfnClientCmd("echo \"Advanced Anti-Aim has adjusted view angles.\"");
        last_log_time = cmd->msec;
    }
}