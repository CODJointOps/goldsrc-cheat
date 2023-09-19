#ifndef _GAME_DETECT_H_
#define _GAME_DETECT_H_

typedef enum {
    GAME_UNKNOWN = 0,
    GAME_HALFLIFE,
    GAME_CS16,
    GAME_TFC,
    GAME_DAY_OF_DEFEAT
} GameType;

GameType get_current_game(void);
int IsCS16(void);
int IsHalfLife(void);
int IsDayOfDefeat(void);
int IsTFC(void);

#endif
