#ifndef HOOKS_H_
#define HOOKS_H_

/*----------------------------------------------------------------------------*/

#include "sdk.h"

#include <dlfcn.h> /* dlsym */
#include <GL/gl.h> /* GLFloat */
#include <stdint.h>

typedef void (*hook_func_t)(void);

typedef int (*key_event_func_t)(int down, int keynum, const char* pszCurrentBinding);

/*
 * Table of prefixes:
 *   prefix | meaning
 *   -------+----------------------------
 *   *_t    | typedef (function type)
 *   h_*    | hook function (ours)
 *   ho_*   | hook original (ptr to orig)
 *   hp_*   | hook pointer (pointer to function pointer)
 *
 *
 * DECL_HOOK_EXTERN: Version for the header. typedef's the function pointer with
 * the return type, name and args. Example:
 *
 *   DECL_HOOK_EXTERN(int, test, double, double);
 *     typedef int (*test_t)(double, double);
 *     extern test_t ho_test;                  // Original
 *     int h_test(double, double);             // Our func
 *
 *
 * DECL_HOOK: Macro for declaring the global function pointer for the original
 * in the source file. Example:
 *
 *   DECL_HOOK(test);
 *     test_t ho_test = NULL;       // Original
 *
 *
 * HOOK: Macro for storing the original function ptr of an interface and hooking
 * our own. Example:
 *
 *   HOOK(gp_client, CL_CreateMove);
 *     ho_CL_CreateMove = gp_client->CL_CreateMove;     // Original
 *     gp_client->CL_CreateMove = h_CL_CreateMove;      // Our func
 *
 *
 * ORIGINAL: Macro for calling the original function. Example:
 *
 *   ORIGINAL(CL_CreateMove, frametime, cmd, active);
 *     ho_CL_CreateMove(frametime, cmd, active);        // Original
 *
 *
 * GL_HOOK: Hooks a OpenGL function. Example:
 *
 *   GL_HOOK(glColor4f);
 *
 *   Will store the original function in o_glColor4f and set the function to
 *   h_glColor4f (which must be defined)
 *
 * GL_UNHOOK: Restores a OpenGL hook created by GL_HOOK. Example:
 *
 *   GL_UNHOOK(glColor4f);
 *
 *   Will restore the original function from o_glColor4f to the original pointer.
 */
#define DECL_HOOK_EXTERN(type, name, ...)  \
    typedef type (*name##_t)(__VA_ARGS__); \
    extern name##_t ho_##name;             \
    type h_##name(__VA_ARGS__);

#define DECL_HOOK(name) name##_t ho_##name = NULL;

#define HOOK(interface, name)          \
    ho_##name       = interface->name; \
    interface->name = h_##name;

#define ORIGINAL(name, ...) ho_##name(__VA_ARGS__);

#define GL_HOOK(ret_type, name, ...) \
    typedef ret_type (*name##_t)(__VA_ARGS__); \
    name##_t h_##name; \
    name##_t o_##name;

#define GL_UNHOOK(name) \
    o_##name = h_##name;

/* For GL hooking */
typedef void (*glColor4f_t)(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
typedef void* (*wglSwapBuffers_t)(void*);

/* OpenGL hooks */
extern void h_glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
extern void* o_glColor4f;

extern wglSwapBuffers_t o_wglSwapBuffers;

/* HUD_Key_Event hook */
extern int h_HUD_Key_Event(int down, int keynum, const char* pszCurrentBinding);
extern key_event_func_t ho_HUD_Key_Event;

/* VMT hooks */
DECL_HOOK_EXTERN(void, CL_CreateMove, float, usercmd_t*, int);
DECL_HOOK_EXTERN(int, HUD_Redraw, float, int);
DECL_HOOK_EXTERN(void, StudioRenderModel, void*);
DECL_HOOK_EXTERN(void, CalcRefdef, ref_params_t*);
DECL_HOOK_EXTERN(void, HUD_PostRunCmd, struct local_state_s*,
                 struct local_state_s*, struct usercmd_s*, int, double,
                 unsigned int);

/* Detour hooks */
DECL_HOOK_EXTERN(void, CL_Move);

/* Detour for CL_Move */
#define ORIGINAL_DETOUR(type) ((type)detour_get_original(&detour_data_clmove))()

/*----------------------------------------------------------------------------*/

bool hooks_init(void);
void hooks_restore(void);
void hooks_schedule_uninject(void);

#endif /* HOOKS_H_ */
