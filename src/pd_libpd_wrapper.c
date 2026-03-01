/*
 * pd_libpd_wrapper.c - C wrapper functions for libpd API
 *
 * This file isolates the libpd C API from C++ and Arduino headers,
 * avoiding the 'word' typedef conflict between Arduino.h and m_pd.h.
 * ESPdLib.cpp calls these wrapper functions instead of libpd directly.
 */

#include "pd_build_defines.h"
#include "pure_data/src/z_libpd.h"
#include "pure_data/src/z_queued.h"

/* ===== Init & Audio ===== */

int pdw_init(void) {
    return libpd_init();
}

int pdw_init_audio(int inChannels, int outChannels, int sampleRate) {
    return libpd_init_audio(inChannels, outChannels, sampleRate);
}

void pdw_add_to_search_path(const char *path) {
    libpd_add_to_search_path(path);
}

/* ===== Patch Management ===== */

void *pdw_openfile(const char *name, const char *dir) {
    return libpd_openfile(name, dir);
}

void pdw_closefile(void *p) {
    libpd_closefile(p);
}

/* ===== Audio Processing ===== */

int pdw_process_short(int ticks, const short *inBuffer, short *outBuffer) {
    return libpd_process_short(ticks, inBuffer, outBuffer);
}

int pdw_process_float(int ticks, const float *inBuffer, float *outBuffer) {
    return libpd_process_float(ticks, inBuffer, outBuffer);
}

/* ===== Messaging ===== */

int pdw_bang(const char *recv) {
    return libpd_bang(recv);
}

int pdw_float(const char *recv, float x) {
    return libpd_float(recv, x);
}

int pdw_symbol(const char *recv, const char *symbol) {
    return libpd_symbol(recv, symbol);
}

int pdw_start_message(int maxlen) {
    return libpd_start_message(maxlen);
}

void pdw_add_float(float x) {
    libpd_add_float(x);
}

int pdw_finish_message(const char *recv, const char *msg) {
    return libpd_finish_message(recv, msg);
}

/* ===== Receive / Subscribe ===== */

void *pdw_bind(const char *recv) {
    return libpd_bind(recv);
}

void pdw_unbind(void *p) {
    libpd_unbind(p);
}

/* ===== Hooks ===== */

void pdw_set_printhook(t_libpd_printhook hook) {
    libpd_set_printhook(hook);
}

void pdw_set_banghook(t_libpd_banghook hook) {
    libpd_set_banghook(hook);
}

void pdw_set_floathook(t_libpd_floathook hook) {
    libpd_set_floathook(hook);
}

/* ===== Arrays ===== */

int pdw_read_array(float *dest, const char *name, int offset, int n) {
    return libpd_read_array(dest, name, offset, n);
}

int pdw_write_array(const char *name, int offset, const float *src, int n) {
    return libpd_write_array(name, offset, src, n);
}
