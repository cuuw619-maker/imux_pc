#pragma once
#ifdef __cplusplus
extern "C" {
#endif
const char* imux_native_version(void);
int imux_native_initialize(void);
void imux_native_shutdown(void);
int imux_native_c_bootstrap(void);
#ifdef __cplusplus
}
#endif