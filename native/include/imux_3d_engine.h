#pragma once
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

int imux_world_run(HWND owner);
int imux_world_is_running(void);
void imux_world_update(float delta_seconds);
void imux_world_render(void);
void imux_world_shutdown(void);
LRESULT imux_world_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

#ifdef __cplusplus
}
#endif
