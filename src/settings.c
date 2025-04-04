#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "include/settings.h"
#include "include/globals.h"
#include "include/game_detection.h"


cheat_settings_t g_settings;

static bool copy_file(const char* src_path, const char* dst_path) {
    FILE* src = fopen(src_path, "rb");
    if (!src) {
        return false;
    }
    
    FILE* dst = fopen(dst_path, "wb");
    if (!dst) {
        fclose(src);
        return false;
    }
    
    char buffer[4096];
    size_t bytes;
    
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, bytes, dst) != bytes) {
            fclose(src);
            fclose(dst);
            return false;
        }
    }
    
    fclose(src);
    fclose(dst);
    return true;
}

bool create_root_default_config(void) {
    cheat_settings_t preset;
    memset(&preset, 0, sizeof(preset));
    
    preset.esp_mode = ESP_BOX;
    preset.chams = true;
    preset.aimbot_enabled = true;
    preset.aimbot_fov = 5.0f;
    preset.bhop = true;
    preset.autostrafe = true;
    preset.thirdperson = false;
    preset.thirdperson_key = 'C';
    preset.thirdperson_dist = 150.0f;
    preset.watermark = true;
    preset.watermark_rainbow = true;
    preset.fov = 90.0f;
    preset.menu_allow_movement = true;
    
    FILE* file = fopen("../default.cfg", "wb");
    if (!file) {
        i_engine->Con_Printf("Error: Could not create root default.cfg\n");
        return false;
    }
    
    size_t written = fwrite(&preset, sizeof(preset), 1, file);
    fclose(file);
    
    if (written != 1) {
        i_engine->Con_Printf("Error: Failed to write to root default.cfg\n");
        return false;
    }
    
    i_engine->Con_Printf("Successfully created ../default.cfg with preset settings\n");
    return true;
}

void settings_init(void) {
    init_default_settings();
    
    char cwd[1024] = {0};
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        i_engine->Con_Printf("Current working directory: %s\n", cwd);
    }
    
    const char* config_dir = get_config_dir();
    if (!config_dir) {
        i_engine->Con_Printf("Error: Could not get config directory, using defaults\n");
        return;
    }
    
    char config_path[1024];
    snprintf(config_path, sizeof(config_path), "%s/default.cfg", config_dir);
    i_engine->Con_Printf("Config path: %s\n", config_path);
    
    if (access(config_path, F_OK) == 0) {
        i_engine->Con_Printf("Found config, loading from: %s\n", config_path);
        
        if (settings_load_from_file("default")) {
            i_engine->Con_Printf("Successfully loaded config\n");
        } else {
            i_engine->Con_Printf("Error loading config, using defaults\n");
        }
    } 
    else if (access("../default.cfg", F_OK) == 0) {
        i_engine->Con_Printf("Found config in project dir, copying to user config...\n");
        
        if (copy_file("../default.cfg", config_path)) {
            i_engine->Con_Printf("Successfully copied project config to user directory\n");
            
            if (settings_load_from_file("default")) {
                i_engine->Con_Printf("Successfully loaded copied config\n");
            } else {
                i_engine->Con_Printf("Error loading copied config, using defaults\n");
            }
        } else {
            i_engine->Con_Printf("Error copying project config, using defaults\n");
        }
    }
    else {
        i_engine->Con_Printf("No config found anywhere, creating default config with current settings\n");
        
        if (settings_save_to_file("default")) {
            i_engine->Con_Printf("Created new default config\n");
        } else {
            i_engine->Con_Printf("Error creating default config\n");
        }
    }
    
    if (g_settings.thirdperson_key == 0) {
        g_settings.thirdperson_key = 'C';
    }
    
    i_engine->Con_Printf("Settings initialized. Third-person key: %d\n", g_settings.thirdperson_key);
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
        i_engine->Con_Printf("Error: Could not get config directory\n");
        return false;
    }
    
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s.cfg", config_dir, filename);
    
    FILE* file = fopen(filepath, "wb");
    if (!file) {
        i_engine->Con_Printf("Error: Could not open file for writing: %s\n", filepath);
        return false;
    }
    
    // Simple direct binary write of the settings
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
        i_engine->Con_Printf("Error: Could not get config directory\n");
        return false;
    }
    
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s.cfg", config_dir, filename);
    
    if (access(filepath, F_OK) != 0) {
        i_engine->Con_Printf("Error: Config file does not exist: %s\n", filepath);
        return false;
    }
    
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        i_engine->Con_Printf("Error: Could not open file for reading: %s\n", filepath);
        return false;
    }
    
    // Get file size for debugging
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    i_engine->Con_Printf("Loading config from %s, file size: %ld bytes, struct size: %zu bytes\n", 
                       filepath, file_size, sizeof(cheat_settings_t));
    
    // Simple direct binary read of the settings
    size_t read = fread(&g_settings, sizeof(g_settings), 1, file);
    fclose(file);
    
    if (read != 1) {
        i_engine->Con_Printf("Error: Failed to read settings from file: %s\n", filepath);
        return false;
    }
    
    i_engine->Con_Printf("Settings loaded. Aimbot=%d, ESP=%d, Bhop=%d, ThirdPerson=%d, TPDist=%.1f\n",
                       g_settings.aimbot_enabled, g_settings.esp_mode, g_settings.bhop,
                       g_settings.thirdperson, g_settings.thirdperson_dist);
    
    return true;
}

bool settings_set_as_default(void) {
    return settings_save_to_file("default");
}

bool settings_create_root_default(void) {
    return create_root_default_config();
}

// Add a simple function to copy config from ~/gitprojects/goldsource-cheat/default.cfg to ~/.config/dz-goldsrccheat/default.cfg
bool copy_project_config_to_user(void) {
    const char* config_dir = get_config_dir();
    if (!config_dir) {
        i_engine->Con_Printf("Error: Could not get config directory\n");
        return false;
    }
    
    char dst_path[1024];
    snprintf(dst_path, sizeof(dst_path), "%s/default.cfg", config_dir);
    
    // Check if source exists
    if (access("../default.cfg", F_OK) != 0) {
        i_engine->Con_Printf("Error: Source config ../default.cfg does not exist\n");
        return false;
    }
    
    if (!copy_file("../default.cfg", dst_path)) {
        i_engine->Con_Printf("Error: Failed to copy ../default.cfg to %s\n", dst_path);
        return false;
    }
    
    i_engine->Con_Printf("Successfully copied project config to user directory\n");
    return true;
}

// Add a simple function to copy config from ~/.config/dz-goldsrccheat/default.cfg to ~/gitprojects/goldsource-cheat/default.cfg
bool copy_user_config_to_project(void) {
    const char* config_dir = get_config_dir();
    if (!config_dir) {
        i_engine->Con_Printf("Error: Could not get config directory\n");
        return false;
    }
    
    char src_path[1024];
    snprintf(src_path, sizeof(src_path), "%s/default.cfg", config_dir);
    
    // Check if source exists
    if (access(src_path, F_OK) != 0) {
        i_engine->Con_Printf("Error: Source config %s does not exist\n", src_path);
        return false;
    }
    
    if (!copy_file(src_path, "../default.cfg")) {
        i_engine->Con_Printf("Error: Failed to copy %s to ../default.cfg\n", src_path);
        return false;
    }
    
    i_engine->Con_Printf("Successfully copied user config to project directory\n");
    return true;
} 