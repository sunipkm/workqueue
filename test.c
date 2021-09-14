#include "workqueue.h"
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

volatile sig_atomic_t done = 0;
void sighandler(int sig)
{
    done = 1;
}

void testjob(workqueue_job_io *io)
{
    int in = *(int *)(io->input);
    *(int *)(io->output) = printf("Received input: %d\n", in);
    fflush(stdout);
}

int main(void)
{
    workqueue_t wq[1];
    srand(time(NULL));
    signal(SIGINT, sighandler);
    InitWorkQueue(wq, 10);
    workqueue_job_io io[1];
    int input = 0, output = 0;
    io->input = &input;
    io->output = &output;
    while(!done)
    {
        int sleeptime = rand() % 5; // 5 seconds at most
        input = rand() % 0xffff;
        printf("Setting up job 1: %d | ", InsertWithTimeout(wq, &testjob, io, 100));
        sleep(sleeptime);
        input = rand() % 0xff;
        printf("Setting up job 2: %d | ", InsertWithTimeout(wq, &testjob, io, 100));
        sleep(1);
    }
    printf("Ongoing jobs: %d\n", ClearWorkQueue(wq));
    return 0;
}