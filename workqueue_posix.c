#include "workqueue.h"
#include "meb_debug.h"
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

typedef struct workqueue_internal_handoff
{
    int id;
    workqueue_t *q;
    workqueue_job_io *io;
    workqueue_job_t jobfcn;
    int timeout;
    pthread_cond_t *wakeup;
} workqueue_internal_handoff;

static pthread_attr_t monitor_attr;

static int get_timeout(struct timespec *ts, int timeout_ms)
{
    if (ts == NULL)
    {
        // debug
        return -1;
    }
    int ret = clock_gettime(CLOCK_REALTIME, ts);
    if (ret == -1)
    {
        // debug
        return -errno;
    }
    if (timeout_ms < 0)
    {
        return 0;
    }
    ts->tv_sec += timeout_ms / 1000;
    timeout_ms = timeout_ms % 1000;
    ts->tv_nsec += timeout_ms * 1000000L;
    ts->tv_sec += (ts->tv_nsec / 1000000000L);
    ts->tv_nsec = ts->tv_nsec % 1000000000L;
    return 1;
}

void *workqueue_worker_thread(void *_arg)
{
    workqueue_internal_handoff *arg = (workqueue_internal_handoff *)_arg;
    (*(arg->jobfcn))(arg->io);
    pthread_cond_signal(arg->wakeup);
    pthread_exit(NULL);
}

void *workqueue_monitor_thread(void *_arg)
{
    void *ret;
    workqueue_internal_handoff *arg = (workqueue_internal_handoff *)_arg;
    struct timespec ts;
    pthread_cond_t wakeup = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t wakelock = PTHREAD_MUTEX_INITIALIZER;
    arg->wakeup = &wakeup;
    int rc;
    if (get_timeout(&ts, arg->timeout) < 0)
    {
        dbprintlf(FATAL "Could not set timeout");
        goto exit;
    }
    while ((rc = pthread_create(&(arg->q->worker[arg->id]), NULL, &workqueue_worker_thread, arg)) == EAGAIN)
        ;
    if (rc)
    {
        dbprintlf(FATAL "Could not create worker thread: %d", rc);
        goto exit;
    }
    if (arg->timeout <= 0) // timeout not allowed
    {
        goto joinwait;
    }
    rc = pthread_cond_timedwait(&wakeup, &wakelock, &ts);
    if (rc == ETIMEDOUT)
    {
        dbprintlf(RED_BG "Job timed out");
        pthread_cancel(arg->q->worker[arg->id]);
    }
    else if (rc != 0) // other stuff
    {
        dbprintlf(FATAL "pthread_cond_timedwait returned %d", rc);
    }
joinwait:
    rc = pthread_join(arg->q->worker[arg->id], &ret);
    if (rc == ESRCH)
    {
        dbprintlf(RED_BG "pthread_join: ESRCH");
        goto exit;
    }
    else if (rc == EINVAL)
    {
        dbprintlf(RED_BG "pthread_join: EINVAL");
        goto exit;
    }
    else if (rc == EDEADLK)
    {
        dbprintlf(RED_BG "pthread_join: EDEADLK");
        goto exit;
    }
    if (ret == PTHREAD_CANCELED)
    {
        dbprintlf(RED_FG "pthread_join: Job cancelled");
    }
exit:
    arg->q->done[arg->id] = true; // indicate done
    free(arg);
    pthread_exit(&ret);
}

int InitWorkQueue(workqueue_t *wq, int numQueueLen)
{
    if (wq == NULL)
        return -1;
    if (numQueueLen <= 0 || numQueueLen > WORKQUEUE_MAX_LEN)
        return -2;
    wq->len = numQueueLen;
    int retryCount = 10;
    RETRY_WITH_COUNT((wq->lock = (pthread_mutex_t *)malloc(wq->len * sizeof(pthread_mutex_t))) == NULL, retryCount);
    RETRY_WITH_COUNT((wq->monitor = (pthread_t *)malloc(wq->len * sizeof(pthread_t))) == NULL, retryCount);
    RETRY_WITH_COUNT((wq->worker = (pthread_t *)malloc(wq->len * sizeof(pthread_t))) == NULL, retryCount);
    RETRY_WITH_COUNT((wq->done = (bool *)malloc(wq->len * sizeof(bool))) == NULL, retryCount);

    for (int i = 0; i < wq->len; i++)
    {
        wq->done[i] = true;
        wq->monitor[i] = 0;
        wq->worker[i] = 0;
    }
    pthread_mutexattr_t lockattr;
    pthread_mutexattr_init(&lockattr);
    pthread_mutexattr_settype(&lockattr, PTHREAD_MUTEX_ERRORCHECK);
    for (int i = 0; i < wq->len; i++)
        pthread_mutex_init(&(wq->lock[i]), &lockattr);
    pthread_mutexattr_destroy(&lockattr);
    pthread_attr_init(&monitor_attr);
    pthread_attr_setdetachstate(&monitor_attr, PTHREAD_CREATE_DETACHED);
    return wq->len;
}

int ClearWorkQueue(workqueue_t *wq)
{
    int busy = 0;
    for (int i = 0; i < wq->len; i++)
    {
        if (pthread_mutex_trylock(&wq->lock[i]) == EBUSY && (wq->done[i] == false))
        {
            pthread_cancel(wq->worker[i]);
            pthread_cancel(wq->monitor[i]);
            busy++;
        }
    }
    free(wq->lock);
    free(wq->monitor);
    free(wq->worker);
    free(wq->done);
    wq->len = 0;
    return busy;
}

int InsertWithTimeout(workqueue_t *wq, workqueue_job_t jobfcn, workqueue_job_io *io, int timeout_ms)
{
    if (wq == NULL || jobfcn == NULL)
        return -1;
    int retval = 1;
    bool start_work = false;
    for (int i = 0; start_work == false; i = (i + 1) % wq->len) // circular
    {
        retval = i + 1;
        // 1. Check if position is open
        int lockret = pthread_mutex_trylock(&wq->lock[i]);
        if (lockret == 0) // lock acquired immediately
        {
            wq->done[i] = false;
            start_work = true;
        }
        else if ((lockret == EDEADLK || lockret == EBUSY) && (wq->done[i])) // could not acquire lock but process is done
        {
            wq->done[i] = false;
            start_work = true;
        }
        if (start_work == true) // empty process, can start here
        {
            workqueue_internal_handoff *handoff = (workqueue_internal_handoff *)malloc(sizeof(workqueue_internal_handoff));
            handoff->id = i;
            handoff->q = wq;
            handoff->jobfcn = jobfcn;
            handoff->io = io;
            handoff->timeout = timeout_ms;
            int rc;
            while ((rc = pthread_create(&(wq->monitor[i]), &monitor_attr, &workqueue_monitor_thread, handoff)) == EAGAIN)
                ;
            if (rc == EINVAL || rc == EPERM)
            {
                dbprintlf(FATAL "Error %d creating monitor thread at queue %d", rc, i);
                return -1;
            }
        }
        // else, not available, continue
    }
    return retval;
}