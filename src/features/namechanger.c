#include <stdint.h>
#include <cstring>
#include <algorithm>
#include <vector>
#include "features.h"
#include "../include/globals.h"
#include "../include/cvars.h"
#include "../include/util.h"
#include "../include/game_detection.h"

static int change_counter = 0;
#define NAME_CHANGE_INTERVAL 10

static int last_name_idx = -1;

void change_name(const char* new_name) {
    if (!new_name) return;

    char command[256];
    snprintf(command, sizeof(command), "name \"%s\u200B\"", new_name);
    i_engine->pfnClientCmd(command);
}

std::vector<char*> get_valid_names(bool (*filter)(cl_entity_t*)) {
    int max_players = 32;
    std::vector<char*> valid_names;

    for (int i = 0; i < max_players; i++) {
        cl_entity_t* ent = get_player(i);
        if (!ent) continue;

        if (!filter || filter(ent)) {
            valid_names.push_back(get_name(i));
        }
    }

    return valid_names;
}

void change_name_from_list(std::vector<char*>& names) {
    if (names.empty()) return;

    last_name_idx = (last_name_idx + 1) % names.size();

    char* name = names[last_name_idx];
    if (name) {
        change_name(name);
        printf("Changing name to: %s\n", name);
    }
}

void change_name_teammates() {
    auto names = get_valid_names(is_friend);
    std::random_shuffle(names.begin(), names.end());
    change_name_from_list(names);
}

void change_name_enemies() {
    auto names = get_valid_names([](cl_entity_t* ent) -> bool {
        return !is_friend(ent);
    });
    std::random_shuffle(names.begin(), names.end());
    change_name_from_list(names);
}

void change_name_all_players() {
    auto names = get_valid_names(nullptr);
    std::random_shuffle(names.begin(), names.end());
    change_name_from_list(names);
}

void change_name_based_on_mode(usercmd_t* cmd) {
    if (!CVAR_ON(misc_namechanger)) return;

    if (++change_counter < NAME_CHANGE_INTERVAL) {
        return;
    }
    change_counter = 0;

    switch ((int)dz_misc_namechanger->value) {
        case 1:
            change_name_teammates();
            break;
        case 2:
            change_name_enemies();
            break;
        case 3:
            change_name_all_players();
            break;
        default:
            break;
    }
}

void check_namechanger_mode_and_execute(usercmd_t* cmd) {
    if (!CVAR_ON(misc_namechanger)) return;

    change_name_based_on_mode(cmd);
}

