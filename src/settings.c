#include "include/settings.h"
#include "include/sdk.h"
#include "include/globals.h"
#include "include/game_detection.h"


cheat_settings_t g_settings;


void settings_init(void) {
    
    init_default_settings();
    
    
    g_settings.thirdperson_key = 'C';
    
    i_engine->Con_Printf("Settings initialized with defaults. Third-person key bound to C.\n");
}


void settings_reset(void) {
    
    g_settings.esp_mode = ESP_OFF;
    g_settings.esp_friendly = false;
    g_settings.chams = false;
    g_settings.tracers = false;
    g_settings.custom_crosshair = false;
    g_settings.watermark = false;
    g_settings.watermark_rainbow = false;
    
    g_settings.aimbot_enabled = false;
    g_settings.aimbot_autoshoot = false;
    g_settings.aimbot_silent = false;
    g_settings.aimbot_norecoil = false;
    g_settings.aimbot_recoil_comp = false;
    g_settings.aimbot_friendly_fire = false;
    g_settings.aimbot_rage_mode = false;
    g_settings.aimbot_team_attack = false;
    
    g_settings.bhop = false;
    g_settings.autostrafe = false;
    g_settings.antiaim = false;
    g_settings.antiaim_view = false;
    g_settings.fakeduck = false;
    g_settings.clmove = false;
    
    g_settings.namechanger = false;
    
    g_settings.fov = 90.0f;
    
    g_settings.thirdperson = false;
    g_settings.thirdperson_key = 0;
    g_settings.thirdperson_dist = 150.0f;
    
    i_engine->pfnClientCmd("echo \"All cheat settings have been reset to default values\"");
} 