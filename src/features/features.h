#ifndef FEATURES_H_
#define FEATURES_H_

#include "../include/sdk.h"

enum visible_flags {
    NONE               = 0,
    ENEMY_VISIBLE      = 1,
    ENEMY_NOT_VISIBLE  = 2,
    FRIEND_VISIBLE     = 3,
    FRIEND_NOT_VISIBLE = 4,
    HANDS              = 5,
    SCOPE              = 6,
};

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------*/

/* src/features/movement.c */
void bhop(usercmd_t* cmd);

/* src/features/esp.c */
void esp(void);
void correct_movement(usercmd_t* cmd, vec3_t old_angles);

/* src/features/chams.c */
extern visible_flags visible_mode;
bool chams(void* this_ptr);

/* src/features/aim.c */
void aimbot(usercmd_t* cmd);

/* src/features/no_recoil.c */
void no_recoil(usercmd_t* cmd);

/* src/features/misc.c */
void custom_crosshair(void);
void bullet_tracers(usercmd_t* cmd);

/* src/features/namechanger.c */
void check_namechanger_mode_and_execute(usercmd_t* cmd);

/* src/features/anti_aim.c */
void anti_aim(usercmd_t* cmd);

/* src/features/fov.c */
void fov_adjust(usercmd_t* cmd);

/* src/features/thirdperson.c */
void thirdperson_init(void);
void thirdperson_update(void);
bool thirdperson_key_event(int keynum, int down);

#ifdef __cplusplus
}
#endif

#endif /* FEATURES_H_ */