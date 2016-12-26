#define _GNU_SOURCE

#ifdef __APPLE__
#define _XOPEN_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <ucontext.h>
#include "uthread.h"


int n_threads=0;
int myid=0;

void do_something()
{
    int id;
    
    id=myid;
    myid++;
    
    printf("This is ult %d\n", id);
    if(n_threads<10){
        uthread_create(do_something,2);
        n_threads++;
        uthread_create(do_something,2);
        n_threads++;
    }
    printf("This is ult %d again\n",id);
    uthread_yield(1);
    printf("This is ult %d one more time\n",id);
    uthread_exit();
}

int main()
{
    system_init();
    uthread_create(do_something,2);
    uthread_exit();
}
