#include <stdio.h>
#include <math.h>

#include "include/settings.h"
#include "include/hooks.h"
#include "include/util.h"
#include "include/sdk.h"
#include "include/detour.h"    /* 8dcc/detour-lib */
#include "include/main.h"      /* For self_unload() */
#include "include/menu.h"      /* ImGui menu */
#include "features/features.h" /* bhop(), esp(), etc. */
#include "include/entityutil.h"
#include "include/game_detection.h"
#include "features/thirdperson.h"

// For ImGui context access
#define IMGUI_IMPLEMENTATION
#include "include/imgui/imgui.h"
#include "include/imgui/backends/imgui_impl_opengl2.h"

// Include for mouse button constants
#include "include/sdk/public/keydefs.h"

#include <dlfcn.h>
#include <GL/gl.h>
#include <string.h>
#include <unistd.h> // usleep()

// For DetourFunction and DetourRemove
extern void* DetourFunction(void* orig, void* hook);
extern bool DetourRemove(void* orig, void* hook);

// Define BYTE as unsigned char for GL hooks
typedef unsigned char BYTE;

/* Forward declarations of hook functions */
void h_glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void h_CL_Move(void);
int h_CL_IsThirdPerson(void);
void h_CL_CameraOffset(float* ofs);

/* Normal VMT hooks */
DECL_HOOK(CL_CreateMove);
DECL_HOOK(HUD_Redraw);
DECL_HOOK(StudioRenderModel);
DECL_HOOK(CalcRefdef);
DECL_HOOK(HUD_PostRunCmd);
key_event_func_t ho_HUD_Key_Event = NULL;  // Manually declare the hook

// Manual declarations for third-person hooks
int (*ho_CL_IsThirdPerson)(void) = NULL;
void (*ho_CL_CameraOffset)(float* ofs) = NULL;

/* OpenGL hooks - use function pointer as we get actual addresses from game */
void (*real_glColor4f)(GLfloat, GLfloat, GLfloat, GLfloat) = NULL;

/* Detour hooks */
static detour_data_t detour_data_clmove;

/* Flag to delay uninject until next frame */
static bool g_should_uninject = false;

/* Mouse position tracking independent from the game engine */
static int g_menu_mouse_x = 0;
static int g_menu_mouse_y = 0;
static bool g_mouse_down[5] = {false, false, false, false, false}; // Tracking mouse buttons

/* Store last known camera angles for locking */
static vec3_t s_locked_view_angles = {0, 0, 0};
static bool s_lock_initialized = false;

/* Forward declarations */
static void force_view_angles(void);

/*----------------------------------------------------------------------------*/

bool hooks_init(void) {
    /* Initialize ImGui menu before hooking any rendering functions */
    if (!menu_init()) {
        i_engine->Con_Printf("Failed to initialize ImGui menu\n");
        return false;
    }

    /* Initialize third-person camera system */
    thirdperson_init();

    /* VMT hooking */
    HOOK(i_client, CL_CreateMove);
    HOOK(i_client, HUD_Redraw);
    HOOK(i_studiomodelrenderer, StudioRenderModel);
    HOOK(i_client, CalcRefdef);
    HOOK(i_client, HUD_PostRunCmd);
    
    /* Third-person hooks - use direct replacement approach */
    ho_CL_IsThirdPerson = i_client->CL_IsThirdPerson;
    i_client->CL_IsThirdPerson = h_CL_IsThirdPerson;
    i_engine->Con_Printf("CL_IsThirdPerson hook installed at %p -> %p\n", 
                       ho_CL_IsThirdPerson, h_CL_IsThirdPerson);
    
    /* Camera offset hook - critical for third-person view */
    ho_CL_CameraOffset = i_client->CL_CameraOffset;
    i_client->CL_CameraOffset = h_CL_CameraOffset;
    i_engine->Con_Printf("CL_CameraOffset hook installed at %p -> %p\n", 
                       ho_CL_CameraOffset, h_CL_CameraOffset);
    
    /* Manual hook for HUD_Key_Event - avoiding type cast issues */
    ho_HUD_Key_Event = (key_event_func_t)i_client->HUD_Key_Event;
    i_client->HUD_Key_Event = (key_event_func_t)h_HUD_Key_Event;

    /* Manual OpenGL hooks - get functions from the game engine's GL context */
    real_glColor4f = (void (*)(GLfloat, GLfloat, GLfloat, GLfloat))glColor4f;
    if (real_glColor4f) {
        void* result = DetourFunction((void*)real_glColor4f, (void*)h_glColor4f);
        if (!result) {
            i_engine->Con_Printf("Failed to hook glColor4f\n");
        }
    }
    
    /* We don't hook swap buffers directly anymore - we use HUD_Redraw instead 
       which is more reliable in GoldSrc engine */
    
    /* Detour hooks */
    void* clmove_ptr = dlsym(hw, "CL_Move");
    if (!clmove_ptr) {
        i_engine->Con_Printf("Failed to find CL_Move\n");
        return false;
    }

    /* Initialize detour_data_clmove struct for detour, and add the hook */
    detour_init(&detour_data_clmove, clmove_ptr, (void*)h_CL_Move);
    if (!detour_add(&detour_data_clmove)) {
        i_engine->Con_Printf("Failed to hook CL_Move\n");
        return false;
    }

    /* Initialize debug info */
    if (g_settings.thirdperson) {
        i_engine->Con_Printf("Third-person mode initialized with distance %.1f\n", 
                           g_settings.thirdperson_dist);
    }

    i_engine->Con_Printf("All hooks initialized successfully\n");
    return true;
}

void hooks_restore(void) {
    /* First shut down ImGui to prevent any rendering after hooks are removed */
    menu_shutdown();
    
    /* Remove CL_Move detour */
    if (detour_data_clmove.detoured) {
        detour_del(&detour_data_clmove);
    }
    
    /* Remove OpenGL hooks */
    if (real_glColor4f) {
        DetourRemove((void*)real_glColor4f, (void*)h_glColor4f);
        real_glColor4f = NULL;
    }
    
    /* Restore VMT hooks */
    if (i_client) {
        i_client->CL_CreateMove = ho_CL_CreateMove;
        i_client->HUD_Redraw = ho_HUD_Redraw;
        i_client->CalcRefdef = ho_CalcRefdef;
        i_client->HUD_PostRunCmd = ho_HUD_PostRunCmd;
        i_client->CL_IsThirdPerson = ho_CL_IsThirdPerson;
        i_client->CL_CameraOffset = ho_CL_CameraOffset;
        
        if (ho_HUD_Key_Event) {
            i_client->HUD_Key_Event = ho_HUD_Key_Event;
            ho_HUD_Key_Event = NULL;
        }
    }
    
    if (i_studiomodelrenderer) {
        i_studiomodelrenderer->StudioRenderModel = ho_StudioRenderModel;
    }
    
    i_engine->Con_Printf("All hooks restored\n");
}

void hooks_schedule_uninject(void) {
    g_should_uninject = true;
}

/*----------------------------------------------------------------------------*/

void h_CL_CreateMove(float frametime, usercmd_t* cmd, int active) {
    /* Check if we should uninject before doing anything else */
    if (g_should_uninject) {
        g_should_uninject = false;
        self_unload();
        return;
    }

    /* Declared in globals.c */
    localplayer = i_engine->GetLocalPlayer();

    // First call original function to let the game set up movement commands
    ORIGINAL(CL_CreateMove, frametime, cmd, active);

    vec3_t old_angles = cmd->viewangles;
    
    // Store original movement commands
    float origForward = cmd->forwardmove;
    float origSide = cmd->sidemove;
    float origUp = cmd->upmove;
    int origButtons = cmd->buttons;

    // If menu is open and movement is not allowed, zero out movement commands
    if (g_menu_open && !g_settings.menu_allow_movement) {
        cmd->forwardmove = 0.0f;
        cmd->sidemove = 0.0f;
        cmd->upmove = 0.0f;
        cmd->buttons = 0;
        
        // If menu is open and movement is not allowed, only force angles
        if (s_lock_initialized) {
            vec_copy(cmd->viewangles, s_locked_view_angles);
        }
    } 
    else {
        // Menu is closed OR movement is allowed, process all features
        
        // Restore original movement values if menu is open with movement allowed
        if (g_menu_open && g_settings.menu_allow_movement) {
            cmd->forwardmove = origForward;
            cmd->sidemove = origSide;
            cmd->upmove = origUp;
            cmd->buttons = origButtons;
        }
        
        // Always process features if movement is allowed or menu is closed
        bhop(cmd);
        aimbot(cmd);
        bullet_tracers(cmd);
        anti_aim(cmd);
        check_namechanger_mode_and_execute(cmd);
        fov_adjust(cmd);
        
        // If menu is open with movement allowed, still need to lock view angles
        if (g_menu_open && s_lock_initialized) {
            vec_copy(cmd->viewangles, s_locked_view_angles);
        }
    }

    // Always maintain view angle control and clamp angles
    correct_movement(cmd, old_angles);
    ang_clamp(&cmd->viewangles);
}

/*----------------------------------------------------------------------------*/

rgb_t rainbow_color(float time) {
    const float frequency = 0.1f;

    unsigned char r = (sin(frequency * time + 0) * 127.5f + 127.5f);
    unsigned char g = (sin(frequency * time + 2.0f) * 127.5f + 127.5f);
    unsigned char b = (sin(frequency * time + 4.0f) * 127.5f + 127.5f);

    return (rgb_t){ r, g, b };
}

int h_HUD_Redraw(float time, int intermission) {
    // Force set view angles every frame when menu is open
    force_view_angles();
    
    // Call original function to let the game draw
    int ret = ORIGINAL(HUD_Redraw, time, intermission);

    // Draw watermark if enabled
    if (g_settings.watermark) {
        rgb_t color = g_settings.watermark_rainbow ? rainbow_color(time) : (rgb_t){ 0, 255, 255 };
        engine_draw_text(5, 5, "https://git.deadzone.lol/Wizzard/goldsrc-cheat", color);
    }

    // Draw ESP
    esp();
    
    // Draw custom crosshair
    custom_crosshair();
    
    // Handle cursor for ImGui when menu is open
    if (g_menu_open && g_imgui_context) {
        // Get screen dimensions
        SCREENINFO scr_inf;
        scr_inf.iSize = sizeof(SCREENINFO);
        i_engine->pfnGetScreenInfo(&scr_inf);
        
        // Get mouse position from engine
        int mouse_x, mouse_y;
        i_engine->GetMousePosition(&mouse_x, &mouse_y);
        
        // Update ImGui mouse position
        ImGui::SetCurrentContext(g_imgui_context);
        ImGuiIO& io = ImGui::GetIO();
        
        // Update mouse buttons - using our tracked state from key events
        io.MouseDown[0] = g_mouse_down[0];
        io.MouseDown[1] = g_mouse_down[1];
        
        // Update mouse position (clamped to screen)
        if (mouse_x >= 0 && mouse_x < scr_inf.iWidth && 
            mouse_y >= 0 && mouse_y < scr_inf.iHeight) {
            io.MousePos.x = (float)mouse_x;
            io.MousePos.y = (float)mouse_y;
        }
    }
    
    // Render ImGui menu (if open)
    menu_render();

    return ret;
}

/*----------------------------------------------------------------------------*/

void h_StudioRenderModel(void* this_ptr) {
    if (!chams(this_ptr))
        ORIGINAL(StudioRenderModel, this_ptr);
}

/*----------------------------------------------------------------------------*/

void h_CalcRefdef(ref_params_t* params) {
    /* Store punch angles for CreateMove */
    vec_copy(g_punchAngles, params->punchangle);
    
    /* Call original CalcRefdef */
    ORIGINAL(CalcRefdef, params);
    
    /* Apply third-person camera with direct view modification */
    if (g_settings.thirdperson) {
        thirdperson_modify_view(params);
    }
}

/*----------------------------------------------------------------------------*/

void h_HUD_PostRunCmd(struct local_state_s* from, struct local_state_s* to,
                      struct usercmd_s* cmd, int runfuncs, double time,
                      unsigned int random_seed) {
    ORIGINAL(HUD_PostRunCmd, from, to, cmd, runfuncs, time, random_seed);

    g_flCurrentTime = time;

    /* Store attack information to check if we can shoot */
    if (runfuncs) {
        g_flNextAttack = to->client.m_flNextAttack;
        g_flNextPrimaryAttack =
          to->weapondata[to->client.m_iId].m_flNextPrimaryAttack;
        g_iClip = to->weapondata[to->client.m_iId].m_iClip;
    }
}

/*----------------------------------------------------------------------------*/

void h_glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    /* This visible_mode variable is changed inside the chams() function, which
     * is called from the StudioRenderModel hook.
     *
     * Depending on the type of entity we are trying to render from there, and
     * depending on its visibility, we change this visible_mode variable. */
    switch (visible_mode) {
        case ENEMY_VISIBLE:
            r = 0.40f;
            g = 0.73f;
            b = 0.41f;
            break;
        case ENEMY_NOT_VISIBLE:
            r = 0.90f;
            g = 0.07f;
            b = 0.27f;
            break;
        case FRIEND_VISIBLE:
            r = 0.16f;
            g = 0.71f;
            b = 0.96f;
            break;
        case FRIEND_NOT_VISIBLE:
            r = 0.10f;
            g = 0.20f;
            b = 0.70f;
            break;
        case HANDS:
            /* Multiply by original func parameters for non-flat chams.
             * Credits: @oxiKKK */
            r *= 0.94f;
            g *= 0.66f;
            b *= 0.94f;
            break;
        default:
        case NONE:
            break;
    }

    /* This part is executed regardless of the visible_mode.
     * NOTE: Not calling original breaks chams. */
    if (real_glColor4f)
        real_glColor4f(r, g, b, a);
}

/*----------------------------------------------------------------------------*/

/*
 * Simple passthrough to thirdperson module
 */
int h_CL_IsThirdPerson(void) {
    // We still need this to tell the engine we're in third person view
    return thirdperson_is_active();
}

/*
 * This isn't used anymore - our direct camera control in CalcRefdef handles everything
 */
void h_CL_CameraOffset(float* ofs) {
    // Not used by our new implementation
    ofs[0] = ofs[1] = ofs[2] = 0.0f;
}

// Function to handle CL_Move
void h_CL_Move(void)
{
    // Get original function address safely
    void (*original_func)(void) = NULL;
    if (detour_data_clmove.detoured) {
        detour_del(&detour_data_clmove);
        original_func = (void (*)(void))detour_data_clmove.orig;
    }
    
    // Call original function if we have it
    if (original_func) {
        original_func();
    }
    
    // Update third-person camera every frame
    thirdperson_update();
    
    // Restore the detour
    if (detour_data_clmove.orig) {
        detour_add(&detour_data_clmove);
    }
}

/*----------------------------------------------------------------------------*/

int h_HUD_Key_Event(int down, int keynum, const char* pszCurrentBinding) {
    // Debug output but only for specific keys to avoid spam
    if (keynum == 'C' || keynum == g_settings.thirdperson_key || keynum == K_INS) {
        i_engine->Con_Printf("Key event: down=%d, keynum=%d, binding=%s\n", 
                           down, keynum, pszCurrentBinding ? pszCurrentBinding : "none");
    }
    
    // Check if thirdperson feature wants to handle this key
    if (thirdperson_key_event(down, keynum)) {
        return 0; // The key was handled by thirdperson
    }
    
    // Toggle menu with Insert key or specific binding
    if (down && (keynum == K_INS || (pszCurrentBinding && strcmp(pszCurrentBinding, "dz_menu") == 0))) {
        i_engine->Con_Printf("Menu toggle key detected!\n");
        menu_key_event(keynum, down);
        return 0; // Block this key from reaching the game
    }

    // ImGui gets priority on all input when menu is open
    if (g_menu_open) {
        // For mouse buttons, update our own tracking to help ImGui
        if (keynum == K_MOUSE1 || keynum == K_MOUSE2) {
            if (keynum == K_MOUSE1)
                g_mouse_down[0] = down ? true : false;
            else if (keynum == K_MOUSE2)
                g_mouse_down[1] = down ? true : false;

            return 0; // Block mouse buttons from game
        }

        // Let ESC pass through to the game to close console/etc.
        if (keynum == K_ESCAPE && down) {
            if (ho_HUD_Key_Event)
                return ho_HUD_Key_Event(down, keynum, pszCurrentBinding);
        }
        
        // Allow WASD movement keys if the setting is enabled
        if (g_settings.menu_allow_movement) {
            // Check for movement key bindings instead of specific keys
            if (pszCurrentBinding && (
                strstr(pszCurrentBinding, "+forward") ||
                strstr(pszCurrentBinding, "+back") ||
                strstr(pszCurrentBinding, "+moveleft") ||
                strstr(pszCurrentBinding, "+moveright") ||
                strstr(pszCurrentBinding, "+jump") ||
                strstr(pszCurrentBinding, "+duck") ||
                strstr(pszCurrentBinding, "+speed"))) {
                
                i_engine->Con_Printf("Passing movement key to game: %s\n", pszCurrentBinding);
                if (ho_HUD_Key_Event)
                    return ho_HUD_Key_Event(down, keynum, pszCurrentBinding);
            }
            
            // Also allow WASD keys directly
            if (keynum == 'W' || keynum == 'A' || keynum == 'S' || keynum == 'D' ||
                keynum == ' ' || // Space for jump
                keynum == K_CTRL || // Crouch
                keynum == K_SHIFT) { // Walk
                
                i_engine->Con_Printf("Passing direct movement key to game: %d\n", keynum);
                if (ho_HUD_Key_Event)
                    return ho_HUD_Key_Event(down, keynum, pszCurrentBinding);
            }
        }
        
        // Block all other keys from reaching the game
        return 0;
    }
    
    // When menu is closed, let all keys pass to the game
    if (ho_HUD_Key_Event) {
        return ho_HUD_Key_Event(down, keynum, pszCurrentBinding);
    }
    
    return 0;
}

/*----------------------------------------------------------------------------*/

// Force set view angles through the engine when menu is open
static void force_view_angles(void) {
    // Only do anything when menu is open/closed state changes
    static bool s_was_menu_open = false;
    
    // Get the current time for smoother transitions
    float current_time = i_engine->GetClientTime();
    static float s_menu_close_time = 0.0f;
    static bool s_is_restoring = false;
    
    // Handle menu state changes
    if (g_menu_open != s_was_menu_open) {
        // Menu state changed
        if (g_menu_open) {
            // Menu just opened - store current view angles
            i_engine->GetViewAngles(s_locked_view_angles);
            s_lock_initialized = true;
            i_engine->Con_Printf("Stored view angles: %.1f %.1f %.1f\n", 
                               s_locked_view_angles[0], s_locked_view_angles[1], s_locked_view_angles[2]);
            s_is_restoring = false;
        } else {
            // Menu just closed - set up restoration timing
            s_menu_close_time = current_time;
            s_is_restoring = true;
            
            // Immediately restore angles
            if (s_lock_initialized) {
                i_engine->SetViewAngles(s_locked_view_angles);
                i_engine->Con_Printf("Restored view angles: %.1f %.1f %.1f\n", 
                                   s_locked_view_angles[0], s_locked_view_angles[1], s_locked_view_angles[2]);
            }
        }
        
        s_was_menu_open = g_menu_open;
    }
    
    // When menu is open, continuously force the locked view angles
    if (g_menu_open && s_lock_initialized) {
        // Force the engine angles to match our locked angles
        i_engine->SetViewAngles(s_locked_view_angles);
    }
    
    // Continue restoring angles for a short time after menu close to prevent flicker
    if (!g_menu_open && s_is_restoring) {
        if (current_time - s_menu_close_time < 0.2f) {
            // Still in restoration period, keep forcing angles
            i_engine->SetViewAngles(s_locked_view_angles);
        } else {
            // Done restoring
            s_is_restoring = false;
        }
    }
}
