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

#define IMGUI_IMPLEMENTATION
#include "include/imgui/imgui.h"
#include "include/imgui/backends/imgui_impl_opengl2.h"

#include "include/sdk/public/keydefs.h"

#include <dlfcn.h>
#include <GL/gl.h>
#include <string.h>
#include <unistd.h>

extern void* DetourFunction(void* orig, void* hook);
extern bool DetourRemove(void* orig, void* hook);

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
    
    ho_CL_IsThirdPerson = i_client->CL_IsThirdPerson;
    i_client->CL_IsThirdPerson = h_CL_IsThirdPerson;
    i_engine->Con_Printf("CL_IsThirdPerson hook installed at %p -> %p\n", 
                       ho_CL_IsThirdPerson, h_CL_IsThirdPerson);
    
    ho_CL_CameraOffset = i_client->CL_CameraOffset;
    i_client->CL_CameraOffset = h_CL_CameraOffset;
    i_engine->Con_Printf("CL_CameraOffset hook installed at %p -> %p\n", 
                       ho_CL_CameraOffset, h_CL_CameraOffset);
    
    ho_HUD_Key_Event = (key_event_func_t)i_client->HUD_Key_Event;
    i_client->HUD_Key_Event = (key_event_func_t)h_HUD_Key_Event;

    real_glColor4f = (void (*)(GLfloat, GLfloat, GLfloat, GLfloat))glColor4f;
    if (real_glColor4f) {
        void* result = DetourFunction((void*)real_glColor4f, (void*)h_glColor4f);
        if (!result) {
            i_engine->Con_Printf("Failed to hook glColor4f\n");
        }
    }
    
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

    localplayer = i_engine->GetLocalPlayer();

    ORIGINAL(CL_CreateMove, frametime, cmd, active);

    vec3_t old_angles = cmd->viewangles;
    
    float origForward = cmd->forwardmove;
    float origSide = cmd->sidemove;
    float origUp = cmd->upmove;
    int origButtons = cmd->buttons;

    fov_adjust(cmd);

    if (g_menu_open && !g_settings.menu_allow_movement) {
        cmd->forwardmove = 0.0f;
        cmd->sidemove = 0.0f;
        cmd->upmove = 0.0f;
        cmd->buttons = 0;
        
        if (s_lock_initialized) {
            vec_copy(cmd->viewangles, s_locked_view_angles);
        }
    } 
    else {
        
        if (g_menu_open && g_settings.menu_allow_movement) {
            cmd->forwardmove = origForward;
            cmd->sidemove = origSide;
            cmd->upmove = origUp;
            cmd->buttons = origButtons;
        }
        
        bhop(cmd);
        no_recoil(cmd);  // Apply recoil control before aimbot
        aimbot(cmd);
        bullet_tracers(cmd);
        anti_aim(cmd);
        check_namechanger_mode_and_execute(cmd);
        
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
    force_view_angles();
    
    int ret = ORIGINAL(HUD_Redraw, time, intermission);

    if (g_settings.watermark) {
        rgb_t color = g_settings.watermark_rainbow ? rainbow_color(time) : (rgb_t){ 0, 255, 255 };
        engine_draw_text(5, 5, "https://git.deadzone.lol/Wizzard/goldsrc-cheat", color);
    }

    esp();
    
    custom_crosshair();
    
    if (g_menu_open && g_imgui_context) {
        SCREENINFO scr_inf;
        scr_inf.iSize = sizeof(SCREENINFO);
        i_engine->pfnGetScreenInfo(&scr_inf);
        
        int mouse_x, mouse_y;
        i_engine->GetMousePosition(&mouse_x, &mouse_y);
        
        ImGui::SetCurrentContext(g_imgui_context);
        ImGuiIO& io = ImGui::GetIO();
        
        io.MouseDown[0] = g_mouse_down[0];
        io.MouseDown[1] = g_mouse_down[1];
        
        if (mouse_x >= 0 && mouse_x < scr_inf.iWidth && 
            mouse_y >= 0 && mouse_y < scr_inf.iHeight) {
            io.MousePos.x = (float)mouse_x;
            io.MousePos.y = (float)mouse_y;
        }
    }
    
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
        
        // Track current weapon ID
        int weaponId = to->client.m_iId;
        
        // Update global weapon ID if it changed
        if (g_currentWeaponID != weaponId) {
            g_currentWeaponID = weaponId;
            i_engine->Con_Printf("Weapon changed: ID=%d\n", g_currentWeaponID);
        }
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
    if (keynum == 'C' || keynum == 'c' || keynum == 67 || keynum == 99 || 
        keynum == g_settings.thirdperson_key || keynum == K_INS) {
        i_engine->Con_Printf("Key event: down=%d, keynum=%d, binding=%s\n", 
                           down, keynum, pszCurrentBinding ? pszCurrentBinding : "none");
    }
    
    if (g_menu_open) {
        if (keynum == K_BACKSPACE || keynum == K_DEL || 
            keynum == K_LEFTARROW || keynum == K_RIGHTARROW || 
            keynum == K_UPARROW || keynum == K_DOWNARROW ||
            keynum == K_HOME || keynum == K_END ||
            keynum == K_ENTER || keynum == K_TAB ||
            keynum == K_CTRL || keynum == K_SHIFT || keynum == K_ALT) {
            
            menu_key_event(keynum, down);
            return 0;
        } else if (down && keynum >= 32 && keynum <= 126) {
            menu_char_event(keynum);
        }
    }
    
    extern bool g_waiting_for_key_bind;
    if (g_waiting_for_key_bind && down) {
        menu_key_event(keynum, down);
        return 0;
    }
    
    if (thirdperson_key_event(keynum, down)) {
        i_engine->Con_Printf("Thirdperson key event handled successfully\n");
        return 0;
    }
    
    if (down && (keynum == K_INS || (pszCurrentBinding && strcmp(pszCurrentBinding, "dz_menu") == 0))) {
        i_engine->Con_Printf("Menu toggle key detected!\n");
        menu_key_event(keynum, down);
        return 0;
    }

    if (g_menu_open) {
        if (keynum == K_MOUSE1 || keynum == K_MOUSE2) {
            if (keynum == K_MOUSE1)
                g_mouse_down[0] = down ? true : false;
            else if (keynum == K_MOUSE2)
                g_mouse_down[1] = down ? true : false;

            return 0;
        }

        if (keynum == K_ESCAPE && down) {
            if (ho_HUD_Key_Event)
                return ho_HUD_Key_Event(down, keynum, pszCurrentBinding);
        }
        
        if (g_settings.menu_allow_movement) {
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
            
            if (keynum == 'W' || keynum == 'A' || keynum == 'S' || keynum == 'D' ||
                keynum == ' ' ||
                keynum == K_CTRL ||
                keynum == K_SHIFT) {
                
                i_engine->Con_Printf("Passing direct movement key to game: %d\n", keynum);
                if (ho_HUD_Key_Event)
                    return ho_HUD_Key_Event(down, keynum, pszCurrentBinding);
            }
        }
        
        return 0;
    }
    
    if (ho_HUD_Key_Event) {
        return ho_HUD_Key_Event(down, keynum, pszCurrentBinding);
    }
    
    return 0;
}

/*----------------------------------------------------------------------------*/

static void force_view_angles(void) {
    static bool s_was_menu_open = false;
    
    float current_time = i_engine->GetClientTime();
    static float s_menu_close_time = 0.0f;
    static bool s_is_restoring = false;
    
    if (g_menu_open != s_was_menu_open) {
        if (g_menu_open) {
            i_engine->GetViewAngles(s_locked_view_angles);
            s_lock_initialized = true;
            i_engine->Con_Printf("Stored view angles: %.1f %.1f %.1f\n", 
                               s_locked_view_angles[0], s_locked_view_angles[1], s_locked_view_angles[2]);
            s_is_restoring = false;
        } else {
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
    
    if (g_menu_open && s_lock_initialized) {
        i_engine->SetViewAngles(s_locked_view_angles);
    }
    
    if (!g_menu_open && s_is_restoring) {
        if (current_time - s_menu_close_time < 0.2f) {
            i_engine->SetViewAngles(s_locked_view_angles);
        } else {
            s_is_restoring = false;
        }
    }
}
