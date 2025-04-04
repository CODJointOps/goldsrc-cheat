#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "include/settings.h"
#include "include/globals.h"
#include "include/game_detection.h"


cheat_settings_t g_settings;


void settings_init(void) {
    init_default_settings();
    
    const char* config_dir = get_config_dir();
    if (config_dir) {
        char filepath[1024];
        snprintf(filepath, sizeof(filepath), "%s/default.cfg", config_dir);
        
        if (access(filepath, F_OK) == 0) {
            i_engine->Con_Printf("Loading default configuration...\n");
            settings_load_from_file("default");
        }
    }
    
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

const char* get_config_dir(void) {
    static char config_dir[512] = {0};
    
    if (config_dir[0] == '\0') {
        const char* home = getenv("HOME");
        if (!home) {
            i_engine->Con_Printf("Error: Could not get HOME directory\n");
            return NULL;
        }
        
        snprintf(config_dir, sizeof(config_dir), "%s/.config/dz-goldsrccheat", home);
        
        struct stat st = {0};
        if (stat(config_dir, &st) == -1) {
            if (mkdir(config_dir, 0755) == -1) {
                i_engine->Con_Printf("Error: Could not create config directory %s\n", config_dir);
                return NULL;
            }
            i_engine->Con_Printf("Created config directory: %s\n", config_dir);
        }
    }
    
    return config_dir;
}

bool settings_save_to_file(const char* filename) {
    const char* config_dir = get_config_dir();
    if (!config_dir) {
        return false;
    }
    
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s.cfg", config_dir, filename);
    
    FILE* file = fopen(filepath, "wb");
    if (!file) {
        i_engine->Con_Printf("Error: Could not open file for writing: %s\n", filepath);
        return false;
    }
    
    size_t written = fwrite(&g_settings, sizeof(g_settings), 1, file);
    fclose(file);
    
    if (written != 1) {
        i_engine->Con_Printf("Error: Failed to write settings to file: %s\n", filepath);
        return false;
    }
    
    i_engine->Con_Printf("Settings saved to %s\n", filepath);
    return true;
}

bool settings_load_from_file(const char* filename) {
    const char* config_dir = get_config_dir();
    if (!config_dir) {
        return false;
    }
    
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s.cfg", config_dir, filename);
    
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        i_engine->Con_Printf("Error: Could not open file for reading: %s\n", filepath);
        return false;
    }
    
    cheat_settings_t temp_settings;
    size_t read = fread(&temp_settings, sizeof(temp_settings), 1, file);
    fclose(file);
    
    if (read != 1) {
        i_engine->Con_Printf("Error: Failed to read settings from file: %s\n", filepath);
        return false;
    }
    
    memcpy(&g_settings, &temp_settings, sizeof(g_settings));
    
    i_engine->Con_Printf("Settings loaded from %s\n", filepath);
    return true;
} 