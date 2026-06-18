/*
 * pd_s_loader.c — abstraction loader for ESPdLib
 *
 * Includes s_loader.inc so that .pd abstractions (objects referenced by
 * name from a parent patch) are found and instantiated at runtime.
 *
 * Binary externals (.so/.dll) are never attempted on ESP32 because
 * HAVE_LIBDL and _WIN32 are both undefined, so those code-paths compile
 * away. The "No dynamic loading mechanism" #warning from s_loader.inc is
 * therefore expected and harmless — suppress it to keep the build clean.
 *
 * The companion stub for sys_deken_specifier() (normally defined in the
 * excluded s_inter.c) lives in pd_esp32_stubs.c.
 */
#pragma GCC diagnostic ignored "-Wcpp"
#include "../pd_build_defines.h"
#include "src/s_loader.inc"
