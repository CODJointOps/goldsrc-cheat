#include <stdio.h>
#include "../include/globals.h"
#include "../include/sdk.h"
#include "../include/util.h"
#include "../include/settings.h"
#include "features.h"

extern float* scr_fov_value;

void fov_adjust(usercmd_t* cmd) {
    if (!scr_fov_value) {
        printf("FOV ERROR: Check globals.c missing scr_fov_value.\n");
        return;
    }

    if (g_settings.fov > 0) {
        *scr_fov_value = g_settings.fov;
    }
}
