#ifndef PD_LIBPD_WRAPPER_H
#define PD_LIBPD_WRAPPER_H

/*
 * C-linkage wrapper for libpd functions, avoiding the Arduino.h 'word'
 * typedef conflict with m_pd.h's 'union word'.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Hook typedefs (duplicated here to avoid including z_libpd.h) */
typedef void (*pdw_printhook)(const char *s);
typedef void (*pdw_banghook)(const char *recv);
typedef void (*pdw_floathook)(const char *recv, float x);

/* Init & Audio */
int pdw_init(void);
int pdw_init_audio(int inChannels, int outChannels, int sampleRate);
void pdw_add_to_search_path(const char *path);

/* Patch Management */
void *pdw_openfile(const char *name, const char *dir);
void pdw_closefile(void *p);

/* Audio Processing */
int pdw_process_short(int ticks, const short *inBuffer, short *outBuffer);
int pdw_process_float(int ticks, const float *inBuffer, float *outBuffer);

/* Messaging */
int pdw_bang(const char *recv);
int pdw_float(const char *recv, float x);
int pdw_symbol(const char *recv, const char *symbol);
int pdw_start_message(int maxlen);
void pdw_add_float(float x);
int pdw_finish_message(const char *recv, const char *msg);

/* Receive / Subscribe */
void *pdw_bind(const char *recv);
void pdw_unbind(void *p);

/* Hooks */
void pdw_set_printhook(pdw_printhook hook);
void pdw_set_banghook(pdw_banghook hook);
void pdw_set_floathook(pdw_floathook hook);

/* Arrays */
int pdw_read_array(float *dest, const char *name, int offset, int n);
int pdw_write_array(const char *name, int offset, const float *src, int n);

#ifdef __cplusplus
}
#endif

#endif /* PD_LIBPD_WRAPPER_H */
