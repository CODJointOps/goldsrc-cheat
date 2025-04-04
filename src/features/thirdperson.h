#ifndef THIRDPERSON_H_
#define THIRDPERSON_H_

#include "../include/sdk.h"

#ifdef __cplusplus
extern "C" {
#endif

void thirdperson_init(void);

int thirdperson_is_active(void);

void thirdperson_get_offset(float* offset);

void thirdperson_update(void);

void thirdperson_toggle(void);

bool thirdperson_key_event(int keynum, int down);

void thirdperson_modify_view(ref_params_t* pparams);

#ifdef __cplusplus
}
#endif

#endif