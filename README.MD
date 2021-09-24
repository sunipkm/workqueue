# Asynchronous Job Queue API for POSIX-compatible and Windows NT
Allows scheduling jobs on a single or multiprocessor system, with timeout to kill hanged jobs.
Designed to be cross-platform, using `pthreads` backend for POSIX-compatible systems and native Windows threading API on the NT systems.
The library is implemented in C for POSIX, and C++ for Windows NT systems.
The asynchronous callback does not block/report completion to the job submitter.

## Usage:
Create a work queue:
```c
    ...
    workqueue_t *wq = (workqueue_t *)malloc(sizeof(workqueue_t));
    int rc = InitWorkQueue(wq, 10); // Initialize the work queue with 10 concurrent jobs
    ...
```

Create a job callback that calculates the square of the input:
```c
    void my_job_square(workqueue_job_io *io)
    {
        int in = *(int *)(io->input); // grab the input integer
        *(int *)(io->output) = in * in; // calculate the square, and store it in the output
    }
```

Insert the job callback in the work queue:
```c
    ...
    workqueue_job_io *my_io = (workqueue_job_io *)malloc(sizeof(workqueue_job_io));
    int in = 10, out;
    my_io->input = &in;
    my_io->output = &out;
    rc = InsertWithTimeout(wq, &my_job_square, io, -1); // the job is inserted in the queue without timeout
```

Clean up:
```c
    ...
    rc = ClearWorkQueue(wq);
```
