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
#include <stdbool.h>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define WORKQUEUE_WINDOWS // Windows
#include <windows.h>

typedef struct
{
    int len;
    DWORD id;
    HANDLE *lock;
    HANDLE monitor;
    HANDLE *worker;
} workqueue_t;

#else // POSIX-compat
#include <pthread.h>

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

typedef struct
{
    int len;
    bool *done;
    pthread_mutex_t *lock;
    pthread_t *monitor;
    pthread_t *worker;
} workqueue_t;

#endif
#include <stdio.h>

/**
 * @brief Structure containing pointers to I/O structure for the callback function
 * 
 */
typedef struct
{
    void *input;
    void *output;
} workqueue_job_io;

/**
 * @brief Callback function to be executed on the asynchronous job call
 * @param io Pointer to workqueue_job_io struct containing pointers to input and output structures for the callback function
 */
typedef void (*workqueue_job_t)(workqueue_job_io *io);

#ifndef WORKQUEUE_MAX_LEN
/**
 * @brief Maximum number of asynchronous jobs to be run at once
 * 
 */
#define WORKQUEUE_MAX_LEN 100
#endif

/**
 * @brief Retry operation with condition until true, up to count times
 * 
 * @param opandcond Operation to perform, and condition that evaluates true for retry
 * @param count Retry count
 * 
 */
#define RETRY_WITH_COUNT(opandcond, count)                                                                 \
    for (int _retry_with_count_i = count; (_retry_with_count_i > 0) && (opandcond); _retry_with_count_i--) \
    {                                                                                                      \
        if ((opandcond) && (_retry_with_count_i == 0))                                                     \
            fprintf(stderr, "[%s, %d] %s failed\n", __func__, __LINE__, #opandcond);                       \
    }

/**
 * @brief Initialize a work queue
 * 
 * @param wq Pointer to a workqueue structure for this queue
 * @param numQueueLen Maximum number of asynchronous jobs supported simultaneously
 * @return int Positive on success, negative on error
 */
int InitWorkQueue(workqueue_t *wq, int numQueueLen);

/**
 * @brief Insert a job in the workqueue
 * 
 * @param wq Pointer to the workqueue structure corresponding to the work queue to be used
 * @param callback Pointer to the callback function to be executed when the job is queued
 * @param io Pointer to the workqueue_job_io structure used by the callback function
 * @param timeout_ms Maximum time allowed for the job to complete in ms. Set this to zero or negative to disable cancellation on timeout
 * @return int Queue index (1-index) on a successful queue up, negative on error
 */
int InsertWithTimeout(workqueue_t *wq, workqueue_job_t callback, workqueue_job_io *io, int timeout_ms);

/**
 * @brief Stop all active jobs and clear work queue memory
 * 
 * @param wq Pointer to work queue
 * @return int Number of busy jobs on POSIX, 0 on Windows NT
 */
int ClearWorkQueue(workqueue_t *wq);

#if !defined(WORKQUEUE_WINDOWS) && defined(__cplusplus)
}
#endif

#endif // _WORKQUEUE_H