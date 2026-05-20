/* elib_log_err.h - Log Module Error Codes */

#ifndef ELIB_LOG_ERR_H
#define ELIB_LOG_ERR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ELIB_LOG_OK = 0,
    ELIB_LOG_ERR_INVALID_PARAM,
    ELIB_LOG_ERR_NOT_INITIALIZED,
} elib_log_err_t;

#ifdef __cplusplus
}
#endif

#endif /* ELIB_LOG_ERR_H */
