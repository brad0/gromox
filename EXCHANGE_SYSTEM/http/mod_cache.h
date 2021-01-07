#pragma once
#include "common_types.h"

struct HTTP_CONTEXT;

#ifdef __cplusplus
extern "C" {
#endif

void mod_cache_init(int context_num, const char *list_path);
extern int mod_cache_run(void);
extern int mod_cache_stop(void);
extern void mod_cache_free(void);
BOOL mod_cache_check_caching(HTTP_CONTEXT *phttp);

BOOL mod_cache_get_context(HTTP_CONTEXT *phttp);

void mod_cache_put_context(HTTP_CONTEXT *phttp);

BOOL mod_cache_check_responded(HTTP_CONTEXT *phttp);

BOOL mod_cache_read_response(HTTP_CONTEXT *phttp);

#ifdef __cplusplus
} /* extern "C" */
#endif
