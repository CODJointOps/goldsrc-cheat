#include <stdio.h>
#include "../include/globals.h"
#include "../include/sdk.h"
#include "../include/util.h"
#include "features.h"

extern cvar_t* dz_visuals_fov;
extern float* scr_fov_value;

void fov_adjust(usercmd_t* cmd) {
    if (!scr_fov_value) {
        printf("FOV ERROR: Check globals.c missing scr_fov_value.\n");
        return;
    }

    if (dz_visuals_fov->value) {
        *scr_fov_value = dz_visuals_fov->value;
    }
}
