#include <stdlib.h>
#include <stdio.h>

// This file needs to be compiled as C++
#define IMGUI_IMPLEMENTATION
#include "include/menu.h"
#include "include/sdk.h"
#include "include/globals.h"
#include "include/settings.h"
#include "include/hooks.h"  // For hook function declarations
#include <GL/gl.h>

// Include for mouse button constants
#include "include/sdk/public/keydefs.h"

// Access to hitbox enum from aim.c
extern const char* hitbox_names[];
extern int current_hitbox;

// Menu state
bool g_menu_open = false;

// ImGui context and state
ImGuiContext* g_imgui_context = NULL;
bool g_imgui_initialized = false;

// Key binding state
static bool g_waiting_for_key_bind = false;
static const char* g_current_key_binding_action = NULL;

// Fallback menu if ImGui fails
static void render_fallback_menu(void);

// Verify OpenGL state is valid
static bool check_gl_state(void) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        const char* err_str = "Unknown";
        switch (error) {
            case GL_INVALID_ENUM: err_str = "GL_INVALID_ENUM"; break;
            case GL_INVALID_VALUE: err_str = "GL_INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: err_str = "GL_INVALID_OPERATION"; break;
            case GL_STACK_OVERFLOW: err_str = "GL_STACK_OVERFLOW"; break;
            case GL_STACK_UNDERFLOW: err_str = "GL_STACK_UNDERFLOW"; break;
            case GL_OUT_OF_MEMORY: err_str = "GL_OUT_OF_MEMORY"; break;
        }
        printf("OpenGL error: %s (0x%x)\n", err_str, error);
        i_engine->Con_Printf("OpenGL error: %s (0x%x)\n", err_str, error);
        return false;
    }
    return true;
}

extern "C" bool menu_init(void) {
    // Print debug message
    printf("Initializing ImGui menu...\n");
    i_engine->Con_Printf("Initializing ImGui menu...\n");
    
    // Check for existing context and cleanup
    if (g_imgui_context) {
        printf("ImGui context already exists, shutting down first\n");
        menu_shutdown();
    }
    
    // Check OpenGL state
    if (!check_gl_state()) {
        printf("OpenGL not ready for ImGui initialization\n");
        i_engine->Con_Printf("Warning: OpenGL state not clean, continuing anyway\n");
        // Not fatal, just a warning
    }
    
    // Create ImGui context
    g_imgui_context = ImGui::CreateContext();
    if (!g_imgui_context) {
        printf("Failed to create ImGui context\n");
        i_engine->Con_Printf("Error: Failed to create ImGui context\n");
        return false;
    }
    
    printf("ImGui context created successfully\n");
    i_engine->Con_Printf("ImGui context created successfully\n");
    
    // Configure ImGui
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    
    // Enable mouse input
    io.MouseDrawCursor = false;  // Don't draw cursor until menu is open
    
    // Get initial screen dimensions
    SCREENINFO scr_inf;
    scr_inf.iSize = sizeof(SCREENINFO);
    i_engine->pfnGetScreenInfo(&scr_inf);
    
    // Verify and set default dimensions
    if (scr_inf.iWidth <= 0 || scr_inf.iHeight <= 0) {
        scr_inf.iWidth = 800;
        scr_inf.iHeight = 600;
        i_engine->Con_Printf("Using default screen dimensions: %dx%d\n", 
                           scr_inf.iWidth, scr_inf.iHeight);
    } else {
        i_engine->Con_Printf("Screen dimensions: %dx%d\n", 
                           scr_inf.iWidth, scr_inf.iHeight);
    }
    
    // Set initial display size - critical for NewFrame to succeed
    io.DisplaySize = ImVec2((float)scr_inf.iWidth, (float)scr_inf.iHeight);
    
    // Initialize OpenGL implementation for ImGui
    bool init_result = ImGui_ImplOpenGL2_Init();
    if (!init_result) {
        printf("Failed to initialize ImGui OpenGL2 backend\n");
        i_engine->Con_Printf("Error: Failed to initialize ImGui OpenGL2 backend\n");
        ImGui::DestroyContext(g_imgui_context);
        g_imgui_context = NULL;
        return false;
    }
    
    printf("ImGui OpenGL2 backend initialized successfully\n");
    i_engine->Con_Printf("ImGui OpenGL2 backend initialized successfully\n");
    
    // Set ImGui style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.FrameRounding = 3.0f;
    style.ItemSpacing = ImVec2(8, 4);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 0.9f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
    
    // Menu starts closed
    g_menu_open = false;
    g_imgui_initialized = true;
    
    // Ensure game has mouse control at start
    i_client->IN_ActivateMouse();
    
    i_engine->Con_Printf("ImGui menu initialized successfully\n");
    return true;
}

extern "C" void menu_shutdown(void) {
    if (g_imgui_initialized) {
        i_engine->Con_Printf("Shutting down ImGui...\n");
        
        if (g_imgui_context) {
            ImGui_ImplOpenGL2_Shutdown();
            ImGui::DestroyContext(g_imgui_context);
            g_imgui_context = NULL;
        }
        
        g_imgui_initialized = false;
        i_engine->Con_Printf("ImGui shutdown complete\n");
    }
    
    // Always reset menu state on shutdown
    g_menu_open = false;
    
    // Make sure the game mouse is active when we shut down
    i_client->IN_ActivateMouse();
    
    // Restore mouse bindings
    i_engine->pfnClientCmd("bind mouse1 +attack; bind mouse2 +attack2");
}

extern "C" void menu_render(void) {
    // Skip rendering if menu is not open
    if (!g_menu_open)
        return;
    
    if (g_imgui_initialized && g_imgui_context) {
        // Set the current context
        ImGui::SetCurrentContext(g_imgui_context);
        
        // Get screen dimensions
        SCREENINFO scr_inf;
        scr_inf.iSize = sizeof(SCREENINFO);
        i_engine->pfnGetScreenInfo(&scr_inf);
        
        // Validate screen dimensions
        if (scr_inf.iWidth <= 0 || scr_inf.iHeight <= 0) {
            i_engine->Con_Printf("Warning: Invalid screen dimensions, using defaults\n");
            scr_inf.iWidth = 800;
            scr_inf.iHeight = 600;
        }
        
        // Set display size for ImGui
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2((float)scr_inf.iWidth, (float)scr_inf.iHeight);
        
        // Start new ImGui frame
        ImGui_ImplOpenGL2_NewFrame();
        ImGui::NewFrame();
        
        // Set window position and size
        ImGui::SetNextWindowPos(ImVec2(scr_inf.iWidth * 0.5f, scr_inf.iHeight * 0.5f), 
                               ImGuiCond_Once, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_Once);
        
        // Begin main window
        if (ImGui::Begin("GoldSource Cheat", &g_menu_open, 
                        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
            
            // Create tabs
            if (ImGui::BeginTabBar("##Tabs", ImGuiTabBarFlags_None)) {
                
                // Aimbot tab
                if (ImGui::BeginTabItem("Aimbot")) {
                    if (ImGui::Checkbox("Enable Aimbot", &g_settings.aimbot_enabled)) {
                        // No need to set cvar value
                    }
                    
                    if (g_settings.aimbot_enabled) {
                        if (ImGui::SliderFloat("FOV", &g_settings.aimbot_fov, 0.1f, 180.0f, "%.1f")) {
                            // No need to set cvar value
                        }
                        
                        if (ImGui::SliderFloat("Smoothing", &g_settings.aimbot_smooth, 1.0f, 100.0f, "%.1f")) {
                            // No need to set cvar value
                        }
                        
                        const char* hitbox_items[] = { "Head", "Chest", "Stomach", "Pelvis", "Nearest" };
                        if (ImGui::Combo("Hitbox", &g_settings.aimbot_hitbox, hitbox_items, 5)) {
                            current_hitbox = g_settings.aimbot_hitbox;
                        }
                        
                        ImGui::Checkbox("Auto Shoot", &g_settings.aimbot_autoshoot);
                        ImGui::Checkbox("Silent Aim", &g_settings.aimbot_silent);
                        ImGui::Checkbox("No Recoil", &g_settings.aimbot_norecoil);
                        ImGui::Checkbox("Recoil Compensation", &g_settings.aimbot_recoil_comp);
                        ImGui::Checkbox("Shoot Teammates", &g_settings.aimbot_team_attack);
                        ImGui::Checkbox("Rage Mode", &g_settings.aimbot_rage_mode);
                    }
                    
                    ImGui::EndTabItem();
                }
                
                // Visuals tab
                if (ImGui::BeginTabItem("Visuals")) {
                    const char* esp_modes[] = {"Off", "2D Box", "Name", "All"};
                    int esp_mode = (int)g_settings.esp_mode;
                    if (ImGui::Combo("ESP", &esp_mode, esp_modes, 4)) {
                        g_settings.esp_mode = (esp_mode_t)esp_mode;
                    }
                    
                    ImGui::Checkbox("ESP Friendly", &g_settings.esp_friendly);
                    
                    if (ImGui::SliderFloat("FOV Changer", &g_settings.fov, 70.0f, 140.0f, "%.1f")) {
                        // No need to set cvar value
                    }
                    
                    ImGui::Checkbox("Chams", &g_settings.chams);
                    
                    ImGui::Checkbox("Custom Crosshair", &g_settings.custom_crosshair);
                    
                    ImGui::Checkbox("Bullet Tracers", &g_settings.tracers);
                    
                    ImGui::EndTabItem();
                }
                
                // Movement tab
                if (ImGui::BeginTabItem("Movement")) {
                    ImGui::Checkbox("Bunny Hop", &g_settings.bhop);
                    
                    ImGui::Checkbox("Auto Strafe", &g_settings.autostrafe);
                    
                    ImGui::Checkbox("Anti-Aim", &g_settings.antiaim);
                    
                    if (g_settings.antiaim) {
                        ImGui::Checkbox("Anti-Aim View", &g_settings.antiaim_view);
                    }
                    
                    ImGui::Checkbox("Fake Duck", &g_settings.fakeduck);
                    
                    ImGui::Checkbox("CL_Move", &g_settings.clmove);
                    
                    ImGui::EndTabItem();
                }
                
                // Misc tab with extra options
                if (ImGui::BeginTabItem("Misc")) {
                    ImGui::Checkbox("Name Changer", &g_settings.namechanger);
                    
                    if (g_settings.namechanger) {
                        ImGui::SliderFloat("Name Change Speed", &g_settings.namechanger_speed, 1.0f, 50.0f, "%.1f");
                    }
                    
                    ImGui::Checkbox("Watermark", &g_settings.watermark);
                    
                    if (g_settings.watermark) {
                        ImGui::Checkbox("Rainbow Watermark", &g_settings.watermark_rainbow);
                    }
                    
                    ImGui::Separator();
                    
                    // Menu options
                    ImGui::Text("Menu Settings:");
                    ImGui::Checkbox("Allow Movement (WASD) With Menu Open", &g_settings.menu_allow_movement);
                    
                    ImGui::Separator();
                    
                    if (ImGui::Button("Uninject Cheat", ImVec2(150, 30))) {
                        i_engine->pfnClientCmd("dz_uninject");
                        g_menu_open = false;
                    }
                    
                    ImGui::EndTabItem();
                }
                
                // View settings tab
                if (ImGui::BeginTabItem("View")) {
                    ImGui::Text("Camera Settings:");
                    
                    // Third-person settings
                    bool thirdperson_changed = ImGui::Checkbox("Third-Person View", &g_settings.thirdperson);
                    
                    if (g_settings.thirdperson) {
                        // Distance preset buttons with direct setting of values
                        ImGui::BeginGroup();
                        if (ImGui::Button("100")) {
                            g_settings.thirdperson_dist = 100.0f;
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("270")) {
                            g_settings.thirdperson_dist = 270.0f;
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("500")) {
                            g_settings.thirdperson_dist = 500.0f;
                        }
                        ImGui::EndGroup();
                        
                        // Distance slider - DIRECTLY changes the value without console commands
                        ImGui::SliderFloat("Camera Distance", &g_settings.thirdperson_dist, 30.0f, 800.0f, "%.1f");
                        
                        // Remove multiplier message since we don't multiply anymore
                        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Camera distance updates in real-time");
                        
                        // Key binding for third-person toggle
                        ImGui::Separator();
                        ImGui::Text("Keybind for Third-Person Toggle:");
                        
                        // Display current key binding
                        const char* key_name = "None";
                        if (g_settings.thirdperson_key > 0) {
                            // Get better key names
                            switch(g_settings.thirdperson_key) {
                                case 'F': key_name = "F"; break;
                                case 'V': key_name = "V"; break;
                                case 'T': key_name = "T"; break;
                                case 'P': key_name = "P"; break;
                                case 'C': key_name = "C"; break;
                                case K_F1: key_name = "F1"; break;
                                case K_F2: key_name = "F2"; break;
                                case K_F3: key_name = "F3"; break;
                                case K_F4: key_name = "F4"; break;
                                case K_F5: key_name = "F5"; break;
                                case K_TAB: key_name = "Tab"; break;
                                case K_SPACE: key_name = "Space"; break;
                                case K_CTRL: key_name = "Ctrl"; break;
                                case K_SHIFT: key_name = "Shift"; break;
                                case K_ALT: key_name = "Alt"; break;
                                case K_MOUSE1: key_name = "Mouse1"; break;
                                case K_MOUSE2: key_name = "Mouse2"; break;
                                case K_MOUSE3: key_name = "Mouse3"; break;
                                default: {
                                    // For printable characters
                                    if (g_settings.thirdperson_key >= 32 && g_settings.thirdperson_key <= 126) {
                                        static char chr[2] = {0};
                                        chr[0] = (char)g_settings.thirdperson_key;
                                        key_name = chr;
                                    } else {
                                        // For other keys, show keycode
                                        static char code[32] = {0};
                                        snprintf(code, sizeof(code), "Key %d", g_settings.thirdperson_key);
                                        key_name = code;
                                    }
                                }
                            }
                        }
                        
                        ImGui::Text("Current Key: %s", key_name);
                        
                        // TEMP: Disabled key binding until fixed
                        // Disable any active key bindings
                        if (g_waiting_for_key_bind) {
                            g_waiting_for_key_bind = false;
                            g_current_key_binding_action = NULL;
                        }
                        
                        // Show preset buttons instead
                        if (ImGui::Button("Bind to C", ImVec2(100, 25))) {
                            g_settings.thirdperson_key = 'C';
                            i_engine->Con_Printf("Third-person toggle bound to C key\n");
                        }
                        
                        ImGui::SameLine();
                        
                        if (ImGui::Button("Bind to V", ImVec2(100, 25))) {
                            g_settings.thirdperson_key = 'V';
                            i_engine->Con_Printf("Third-person toggle bound to V key\n");
                        }
                        
                        ImGui::SameLine();
                        
                        if (ImGui::Button("Bind to F", ImVec2(100, 25))) {
                            g_settings.thirdperson_key = 'F';
                            i_engine->Con_Printf("Third-person toggle bound to F key\n");
                        }
                        
                        ImGui::SameLine();
                        
                        if (ImGui::Button("Clear", ImVec2(80, 25))) {
                            g_settings.thirdperson_key = 0;
                            i_engine->Con_Printf("Third-person key binding cleared\n");
                        }
                        
                        ImGui::Text("Note: Default binding is C key");
                    }
                    
                    ImGui::EndTabItem();
                }
                
                ImGui::EndTabBar();
            }
            
            ImGui::End();
        }
        
        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
    } else {
        render_fallback_menu();
    }
}

// This is our fallback menu that we know works
static void render_fallback_menu(void) {
    // Get screen dimensions
    SCREENINFO scr_inf;
    scr_inf.iSize = sizeof(SCREENINFO);
    i_engine->pfnGetScreenInfo(&scr_inf);
    
    // Draw simple menu background
    int x1 = scr_inf.iWidth / 4;
    int y1 = scr_inf.iHeight / 4;
    int x2 = scr_inf.iWidth * 3 / 4;
    int y2 = scr_inf.iHeight * 3 / 4;
    
    // More visible background (darker black with higher alpha)
    i_engine->pfnFillRGBA(x1, y1, x2-x1, y2-y1, 0, 0, 0, 230);
    
    // Draw bright border for visibility
    i_engine->pfnFillRGBA(x1, y1, x2-x1, 2, 255, 0, 0, 255);  // Top
    i_engine->pfnFillRGBA(x1, y2-2, x2-x1, 2, 255, 0, 0, 255); // Bottom
    i_engine->pfnFillRGBA(x1, y1, 2, y2-y1, 255, 0, 0, 255);  // Left
    i_engine->pfnFillRGBA(x2-2, y1, 2, y2-y1, 255, 0, 0, 255); // Right
    
    // Title
    i_engine->pfnDrawSetTextColor(1.0f, 1.0f, 0.0f);
    i_engine->pfnDrawConsoleString(x1+10, y1+10, "===== GoldSource Cheat Menu (Fallback) =====");
    
    // Reset color for menu items
    i_engine->pfnDrawSetTextColor(0.0f, 1.0f, 1.0f);
    
    // Draw menu items
    int y = y1 + 40;
    i_engine->pfnDrawConsoleString(x1+20, y, "-- Aimbot Settings --");
    y += 20;
    
    char buffer[128];
    
    bool aimbot_enabled = g_settings.aimbot_enabled;
    snprintf(buffer, sizeof(buffer), "- Aimbot: %s", aimbot_enabled ? "ON" : "OFF");
    i_engine->pfnDrawConsoleString(x1+30, y, buffer);
    y += 15;
    
    if (aimbot_enabled) {
        snprintf(buffer, sizeof(buffer), "- FOV: %.1f", g_settings.aimbot_fov);
        i_engine->pfnDrawConsoleString(x1+30, y, buffer);
        y += 15;
        
        snprintf(buffer, sizeof(buffer), "- Smoothing: %.1f", g_settings.aimbot_smooth);
        i_engine->pfnDrawConsoleString(x1+30, y, buffer);
        y += 15;
    }
    
    y += 10;
    i_engine->pfnDrawSetTextColor(0.0f, 1.0f, 1.0f);
    i_engine->pfnDrawConsoleString(x1+20, y, "-- Visual Settings --");
    y += 20;
    
    int esp_mode = (int)g_settings.esp_mode;
    snprintf(buffer, sizeof(buffer), "- ESP: %s", esp_mode > 0 ? "ON" : "OFF");
    i_engine->pfnDrawConsoleString(x1+30, y, buffer);
    y += 15;
    
    bool chams_enabled = g_settings.chams;
    snprintf(buffer, sizeof(buffer), "- Chams: %s", chams_enabled ? "ON" : "OFF");
    i_engine->pfnDrawConsoleString(x1+30, y, buffer);
    y += 30;
    
    // Instructions
    i_engine->pfnDrawSetTextColor(1.0f, 1.0f, 0.0f);
    i_engine->pfnDrawConsoleString(x1+20, y, "Press INSERT to close menu");
    y += 20;
    i_engine->pfnDrawConsoleString(x1+20, y, "Use console to change settings");
    
    // Draw help in bottom corner
    i_engine->pfnDrawSetTextColor(0.7f, 0.7f, 0.7f);
    i_engine->pfnDrawConsoleString(5, scr_inf.iHeight - 20, "Type 'dz_menu' in console to toggle menu");
}

// Update the key event handler to support key binding
extern "C" void menu_key_event(int keynum, int down) {
    // Add debug output
    i_engine->Con_Printf("menu_key_event called: keynum=%d, down=%d\n", keynum, down);
    
    // If we're waiting for a key bind and a key is pressed down
    if (g_waiting_for_key_bind && down && keynum != K_ESCAPE) {
        // Don't bind mouse wheel or menu toggle key
        if (keynum != K_MWHEELDOWN && keynum != K_MWHEELUP && keynum != K_INS) {
            // Bind the key based on current action
            if (g_current_key_binding_action && strcmp(g_current_key_binding_action, "thirdperson") == 0) {
                g_settings.thirdperson_key = keynum;
                i_engine->Con_Printf("Third-person key bound to keycode: %d\n", keynum);
            }
            
            // Exit binding mode
            g_waiting_for_key_bind = false;
            g_current_key_binding_action = NULL;
            return;
        }
    }
    
    // Cancel key binding if escape is pressed
    if (g_waiting_for_key_bind && down && keynum == K_ESCAPE) {
        g_waiting_for_key_bind = false;
        g_current_key_binding_action = NULL;
        i_engine->Con_Printf("Key binding canceled\n");
        return;
    }
    
    if (keynum == K_INS && down) {
        // Toggle menu state
        g_menu_open = !g_menu_open;
        i_engine->Con_Printf("Menu %s\n", g_menu_open ? "opened" : "closed");
        
        // Update ImGui IO if initialized
        if (g_imgui_initialized && g_imgui_context) {
            ImGui::SetCurrentContext(g_imgui_context);
            ImGuiIO& io = ImGui::GetIO();
            io.MouseDrawCursor = g_menu_open;
        }
        
        // Activate or deactivate mouse based on menu state
        if (g_menu_open) {
            // When opening menu, deactivate mouse in game and use our own cursor
            i_client->IN_DeactivateMouse();
            i_client->IN_ClearStates();
            
            // Fixes potential issues with key state
            i_engine->pfnClientCmd("unbind mouse1; unbind mouse2");
            i_engine->Con_Printf("Mouse deactivated for game, using ImGui cursor\n");
        } else {
            // When closing menu, reactivate mouse in game
            i_client->IN_ActivateMouse();
            
            // Restore default bindings
            i_engine->pfnClientCmd("bind mouse1 +attack; bind mouse2 +attack2");
            i_engine->Con_Printf("Mouse reactivated for game\n");
        }
    }
} 