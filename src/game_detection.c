#include "include/game_detection.h"
#include <string.h> 
#include <stdio.h>

static GameType current_game = GAME_UNKNOWN;

GameType get_current_game(void) {
    if (current_game != GAME_UNKNOWN) {
        return current_game;
    }

    FILE *fp = fopen("/proc/self/cmdline", "r");
    if (fp) {
        char buf[1024];
        size_t size = fread(buf, sizeof(char), sizeof(buf) - 1, fp);
        fclose(fp);

        if (size > 0) {
            buf[size] = '\0';

            char *gameTypeToken = NULL;
            char *steamToken = NULL;

            int tokensFound = 0;

            for (char *token = buf; token < buf + size; token += strlen(token) + 1) {
                tokensFound++;

                if (strcmp(token, "-game") == 0) {
                    gameTypeToken = token + strlen(token) + 1;
                } else if (strcmp(token, "-steam") == 0) {
                    steamToken = token;
                }
            }

            if (gameTypeToken) {
                if (strcmp(gameTypeToken, "cstrike") == 0) {
                    current_game = GAME_CS16;
                } else if (strcmp(gameTypeToken, "dod") == 0) {
                    current_game = GAME_DAY_OF_DEFEAT;
                } else if (strcmp(gameTypeToken, "dmc") == 0) {
                    current_game = GAME_DMC;    
                } else if (strcmp(gameTypeToken, "tfc") == 0) {
                    current_game = GAME_TFC;
                }
            } else if (steamToken && tokensFound == 2) {
                // If only `-steam` is found and no `-game`, with only two tokens, assume it's Half-Life 1
                current_game = GAME_HALFLIFE;
            }
        }
    }

    return current_game;
}

int IsCS16(void) {
    return get_current_game() == GAME_CS16;
}

int IsHalfLife(void) {
    return get_current_game() == GAME_HALFLIFE;
}

int IsDayOfDefeat(void) {
    return get_current_game() == GAME_DAY_OF_DEFEAT;
}

int IsTFC(void) {
    return get_current_game() == GAME_TFC;
}

int IsDeathmatchClassic(void) {
    return get_current_game() == GAME_DMC;
}
