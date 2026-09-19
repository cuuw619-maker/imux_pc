#include "imux_native.h"
static int imux_c_bootstrap_value(void) { return 1; }
int imux_native_c_bootstrap(void) { return imux_c_bootstrap_value(); }