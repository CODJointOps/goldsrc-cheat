#pragma once

#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESP_OFF  = 0,
    ESP_BOX  = 1,
    ESP_NAME = 2,
    ESP_ALL  = 3
} esp_mode_t;

typedef struct {
    esp_mode_t esp_mode;
    bool esp_friendly;
    float fov;
    bool chams;
    bool tracers;
    bool custom_crosshair;
    bool watermark;
    bool watermark_rainbow;
    
    bool aimbot_enabled;
    float aimbot_fov;
    float aimbot_smooth;
    bool aimbot_silent;
    bool aimbot_autoshoot;
    bool aimbot_require_key;
    bool aimbot_norecoil;
    bool aimbot_recoil_comp;
    bool aimbot_friendly_fire;
    bool aimbot_rage_mode;
    bool aimbot_team_attack;
    int aimbot_hitbox;
    
    bool bhop;
    bool autostrafe;
    bool antiaim;
    bool antiaim_view;
    bool fakeduck;
    bool clmove;
    
    bool namechanger;
    float namechanger_speed;
    bool menu_allow_movement;
    
    bool thirdperson;
    int thirdperson_key;
    float thirdperson_dist;
} cheat_settings_t;

extern cheat_settings_t g_settings;

void settings_init(void);
void settings_reset(void);
bool settings_save_to_file(const char* filename);
bool settings_load_from_file(const char* filename);
bool settings_set_as_default(void);
bool settings_create_root_default(void);
const char* get_config_dir(void);

inline void init_default_settings(void) {
    memset(&g_settings, 0, sizeof(g_settings));
    
    g_settings.aimbot_fov = 5.0f;
    g_settings.aimbot_smooth = 10.0f;
    g_settings.aimbot_hitbox = 0;
    
    g_settings.esp_mode = ESP_OFF;
    g_settings.fov = 90.0f;
    
    g_settings.namechanger_speed = 5.0f;
    
    g_settings.thirdperson = false;
    g_settings.thirdperson_dist = 300.0f;
    g_settings.thirdperson_key = 'C';
    
    g_settings.menu_allow_movement = true;
}

#ifdef __cplusplus
}
#endif 