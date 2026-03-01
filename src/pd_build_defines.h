#ifndef PD_BUILD_DEFINES_H
#define PD_BUILD_DEFINES_H

// Core Pd defines for embedded/libpd build
#define PD
#define USEAPI_DUMMY
#define PD_INTERNAL
#define HAVE_UNISTD_H
#define HAVE_ALLOCA_H
#define PD_HEADLESS
#define SYMTABHASHSIZE 512

// libpd instance support (single instance for now)
// #define PDINSTANCE

// Exclude FFT to save code space
#define PD_NO_FFT

// ESP32 newlib doesn't have lstat (no symlinks on flash filesystems)
#define lstat stat

// ESP32 newlib may not link signal() - provide a no-op macro
#include <signal.h>
#define signal(sig, handler) ((void)(sig), (void)(handler), (void (*)(int))0)

#endif // PD_BUILD_DEFINES_H
