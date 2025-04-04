#pragma once

#include <stdbool.h>
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_opengl2.h"
#include "sdk.h"

#ifdef __cplusplus
extern "C" {
#endif

bool menu_init(void);
void menu_shutdown(void);
void menu_render(void);
void menu_key_event(int keynum, int down);

extern bool g_menu_open;
extern bool g_imgui_initialized;
extern ImGuiContext* g_imgui_context;

#ifdef __cplusplus
}
#endif 