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
    bool aimbot_smoothing_enabled;
    bool aimbot_silent;
    bool aimbot_autoshoot;
    bool aimbot_require_key;
    bool aimbot_norecoil;
    bool aimbot_recoil_comp;
    bool aimbot_friendly_fire;
    bool aimbot_team_attack;
    int aimbot_hitbox;
    
    bool bhop;
    bool autostrafe;
    bool antiaim;
    bool antiaim_view;
    bool fakeduck;
    bool clmove;
    
    int aa_pitch_mode;
    int aa_yaw_mode;
    float aa_custom_pitch;
    float aa_custom_yaw;
    float aa_spin_speed;
    float aa_jitter_range;
    bool aa_lby_breaker;
    bool aa_desync;
    bool aa_on_attack;
    bool aa_first_person;
    bool fake_duck;
    
    bool antiaim_enabled;
    bool antiaim_pitch_enabled;
    bool antiaim_yaw_enabled;
    float antiaim_pitch;
    float antiaim_yaw;
    bool antiaim_fakeduck;
    int antiaim_fakeduck_key;
    bool antiaim_desync;
    bool antiaim_legit;
    
    int antiaim_pitch_mode;
    int antiaim_yaw_mode;
    float antiaim_custom_pitch;
    float antiaim_custom_yaw;
    float antiaim_spin_speed;
    float antiaim_jitter_range;
    bool antiaim_lby_breaker;
    bool antiaim_on_attack;
    
    bool namechanger;
    float namechanger_speed;
    bool menu_allow_movement;
    
    bool thirdperson;
    int thirdperson_key;
    float thirdperson_dist;

    bool blinker;
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
    g_settings.aimbot_smoothing_enabled = true;
    g_settings.aimbot_hitbox = 0;
    
    g_settings.esp_mode = ESP_OFF;
    g_settings.fov = 90.0f;
    
    g_settings.namechanger_speed = 5.0f;
    
    g_settings.thirdperson = false;
    g_settings.thirdperson_dist = 300.0f;
    g_settings.thirdperson_key = 'C';
    
    // Initialize anti-aim defaults
    g_settings.aa_pitch_mode = 0;   // None
    g_settings.aa_yaw_mode = 0;     // None
    g_settings.aa_custom_pitch = 0.0f;
    g_settings.aa_custom_yaw = 0.0f;
    g_settings.aa_spin_speed = 360.0f;  // One rotation per second
    g_settings.aa_jitter_range = 45.0f; // ±45 degrees jitter
    g_settings.aa_lby_breaker = false;
    g_settings.aa_desync = false;
    g_settings.aa_on_attack = false;
    g_settings.aa_first_person = false;
    g_settings.fake_duck = false;
    
    // Initialize new anti-aim settings
    g_settings.antiaim_enabled = false;
    g_settings.antiaim_pitch_enabled = false;
    g_settings.antiaim_yaw_enabled = false;
    g_settings.antiaim_pitch = 89.0f;       // Default to look down
    g_settings.antiaim_yaw = 180.0f;        // Default to backward
    g_settings.antiaim_fakeduck = false;
    g_settings.antiaim_fakeduck_key = 0;    // No key binding
    g_settings.antiaim_desync = false;
    g_settings.antiaim_legit = false;
    g_settings.antiaim_view = false;        // Don't show anti-aim in first person by default
    
    // Initialize advanced anti-aim settings
    g_settings.antiaim_pitch_mode = 1;      // Down (89°)
    g_settings.antiaim_yaw_mode = 1;        // Backward (180°)
    g_settings.antiaim_custom_pitch = 0.0f;
    g_settings.antiaim_custom_yaw = 0.0f;
    g_settings.antiaim_spin_speed = 360.0f; // One rotation per second
    g_settings.antiaim_jitter_range = 45.0f; // ±45 degrees jitter
    g_settings.antiaim_lby_breaker = false;
    g_settings.antiaim_on_attack = false;
    
    g_settings.menu_allow_movement = true;
    g_settings.blinker = false;
}

#ifdef __cplusplus
}
#endif 