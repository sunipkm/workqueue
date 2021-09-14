/**
 * @file workqueue.h
 * @author Sunip K. Mukherjee (sunipkmukherjee@gmail.com)
 * @brief Work Queue API (POSIX and NT compatible)
 * @version 1.0
 * @date 2021-09-14
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#ifndef _WORKQUEUE_H
#define _WORKQUEUE_H

#if defined(WORKQUEUE_WINDOWS)
#undef WORKQUEUE_WONDOWS
#endif

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define WORKQUEUE_WINDOWS // Windows
#include <windows.h>
#include <mfapi.h>
#pragma comment(lib, "mfapi.lib")

typedef struct
{
    int len = 0;
    DWORD id = 0;
    HANDLE *lock = nullptr;
    bool *done = nullptr;
    HANDLE *monitor = nullptr;
    DWORD *monitor_id = nullptr;
} workqueue_t;

#else // POSIX-compat
#include <pthread.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct
{
    int len;
    pthread_mutex_t *lock;
    bool *done;
    pthread_t *monitor;
    pthread_t *worker;
} workqueue_t;

#endif
#include <stdio.h>

typedef struct
{
    void *input;
    void *output;
} workqueue_job_io;
typedef void (*workqueue_job_t)(workqueue_job_io *io);

#ifndef WORKQUEUE_MAX_LEN
#define WORKQUEUE_MAX_LEN 100
#endif

/**
 * @brief Retry operation with condition until true, up to count times
 * 
 * @param opandcond Operation to perform, and condition that evaluates true for retry
 * @param count Retry count
 * 
 */
#define RETRY_WITH_COUNT(opandcond, count) \
for (int _retry_with_count_i = count; (_retry_with_count_i > 0) && (opandcond); _retry_with_count_i--) \
{ \
    if ((opandcond) && (_retry_with_count_i == 0)) \
        fprintf(stderr, "[%s, %d] %s failed\n", __func__, __LINE__, #opandcond); \
}
int InitWorkQueue(workqueue_t *wq, int numQueueLen);
int InsertWithTimeout(workqueue_t *wq, workqueue_job_t callback, workqueue_job_io *io, int timeout_ms);
int ClearWorkQueue(workqueue_t *wq);

#if !defined(WORKQUEUE_WINDOWS) && defined(__cplusplus)
}
#endif

#endif // _WORKQUEUE_H