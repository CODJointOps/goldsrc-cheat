#ifndef UTIL_H_
#define UTIL_H_

#include <stdint.h>
#include <math.h>
#include "sdk.h"
#include "globals.h"

/* Vector 2 for 2d points */
typedef float vec2_t[2];

/* RGB color */
typedef struct {
    uint8_t r, g, b;
} rgb_t;

#define DEG2RAD(n) ((n)*M_PI / 180.0)

#define gl_drawline_points(p0, p1, w, col) \
    gl_drawline(p0[0], p0[1], p1[0], p1[1], w, col);

/*----------------------------------------------------------------------------*/

bool is_alive(cl_entity_t* ent);
bool valid_client(cl_entity_t* ent);
char* get_name(int ent_idx);
vec3_t vec3(float x, float y, float z);
bool vec_is_zero(vec3_t v);
bool world_to_screen(vec3_t vec, vec2_t screen);
void gl_drawbox(int x, int y, int w, int h, rgb_t c);
void gl_drawline(int x0, int y0, int x1, int y1, float w, rgb_t col);

#endif /* UTIL_H_ */
