#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>
#define WORKQUEUE_MAX_LEN 100
typedef struct
{
    int len;
    pthread_mutex_t *lock;
    bool *done;
    pthread_t *monitor;
    pthread_t *worker;
} workqueue_t;
typedef struct
{
    void *input;
    void *output;
} workqueue_job_io;
typedef void (*workqueue_job_t)(workqueue_job_io *io);

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