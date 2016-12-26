/////////////////////////////////////////////////////////////////////
//                     Library API prototypes                      //
/////////////////////////////////////////////////////////////////////


// This function is called before any other uthread library
// functions can be called. It initializes the uthread system.
void system_init();

// This function creates a new user-level thread which runs func(),
// with priority number specified by argument priority. This function
// returns 0 if succeeds, or -1 otherwise.
int uthread_create(void func(), int priority);

// The calling thread requests to yield the kernel thread that
// it is currently running to one of other user threads which
// has the highest priority level among the ready threads if
// there is one or more other threads ready to run. If no
// any other thread is ready to run, the calling thread should
// proceed to run on the kernel thread. This function returns 0
// if succeeds, or -1 otherwise.
int uthread_yield(int priority);

// The calling user-level thread ends its execution.
void uthread_exit();
