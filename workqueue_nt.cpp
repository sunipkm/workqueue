#include "workqueue.h"
#include "meb_debug.h"
#include <stdlib.h>
#include <Shlwapi.h>
#include <winerror.h>
#include <time.h>
#include <errno.h>

typedef struct
{
    workqueue_t *wq = nullptr;
    workqueue_job_t jobfcn;
    workqueue_job_io *io;
    DWORD timeout;
} workqueue_handoff_t;

DWORD WINAPI workqueue_worker_thread(LPVOID _arg)
{
    workqueue_handoff_t *arg = (workqueue_handoff_t *)_arg;
    (*(arg->jobfcn))(arg->io);
    dbprintlf("Worker finished");
    ExitThread(1);
}

DWORD WINAPI workqueue_monitor_thread(LPVOID _arg)
{
    workqueue_handoff_t *arg = (workqueue_handoff_t *)_arg;
    // try to lock
    int i;
    for (i = 0;; i = (i + 1) % (arg->wq->len))
    {
        DWORD lockret = WaitForSingleObject(arg->wq->lock[i], 0);
        if (lockret == WAIT_OBJECT_0)
            break;
    }
    arg->wq->worker[i] = CreateThread(NULL, 0, workqueue_worker_thread, _arg, 0, NULL);
    if (arg->wq->worker[i] == NULL)
    {
        dbprintlf("Error creating worker thread: %d", GetLastError());
        goto exit;
    }
    if (WaitForSingleObject(arg->wq->worker[i], arg->timeout) == WAIT_TIMEOUT)
    {
        dbprintlf("Worker thread timed out on %d", i);
        TerminateThread(arg->wq->worker[i], 0);
    }
    CloseHandle(arg->wq->worker[i]);
    arg->wq->worker[i] = INVALID_HANDLE_VALUE;
exit:
    if (!ReleaseMutex(arg->wq->lock[i]))
    {
        dbprintlf("Monitor could not unlock mutex");
    }
    delete arg;
    ExitThread(1);
}

int InitWorkQueue(workqueue_t *wq, int numQueueLen)
{
    if (wq == NULL || wq == nullptr)
        return -1;
    if (numQueueLen <= 0 || numQueueLen > WORKQUEUE_MAX_LEN)
        return -2;
    wq->len = numQueueLen;
    int retryCount = 10;
    RETRY_WITH_COUNT((wq->lock = new HANDLE[wq->len]) == nullptr, retryCount);
    RETRY_WITH_COUNT((wq->worker = new HANDLE[wq->len]) == nullptr, retryCount);
    for (int i = 0; i < wq->len; i++)
    {
        wq->worker[i] = INVALID_HANDLE_VALUE;
        wq->lock[i] = CreateMutex(NULL, FALSE, NULL);
    }
    wq->monitor = INVALID_HANDLE_VALUE;
    return wq->len;
}

int InsertWithTimeout(workqueue_t *wq, workqueue_job_t callback, workqueue_job_io *io, int timeout_ms)
{
    if (wq == NULL || wq == nullptr || callback == NULL || callback == nullptr)
        return -1;

    workqueue_handoff_t *handoff = new workqueue_handoff_t;
    handoff->wq = wq;
    handoff->jobfcn = callback;
    handoff->io = io;
    handoff->timeout = timeout_ms >= 0 ? timeout_ms : INFINITE;
    wq->monitor = CreateThread(NULL, 0, workqueue_monitor_thread, handoff, 0, NULL);
    int retval = 0;
    if (wq->monitor == NULL)
    {
        retval = -2;
    }
    else
    {
        CloseHandle(wq->monitor);
        retval = 1;
    }
    wq->monitor = NULL;
    return retval;
}

int ClearWorkQueue(workqueue_t *wq)
{
    int busy = 0;
    for (int i = 0; i < wq->len; i++)
    {
        CloseHandle(wq->worker[i]);
        CloseHandle(wq->lock[i]);
    }
    delete[] wq->lock;
    delete[] wq->worker;
    CloseHandle(wq->monitor);
    wq->len = 0;
    return busy;
}