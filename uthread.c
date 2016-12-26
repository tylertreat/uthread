#define _GNU_SOURCE

#ifdef __APPLE__
#define _XOPEN_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <ucontext.h>
#include "uthread.h"

#define STACK_SIZE 16384


/////////////////////////////////////////////////////////////////////
//        Thread queue definitions and related operations          //
/////////////////////////////////////////////////////////////////////


// The best approach would probably be to use a heap-based priority
// queue, but since there don't appear to be restrictions, I will
// use a simple circular, doubly-linked list.

// Represents a uthread consisting of a priority, function, context,
// and links to other threads in the queue.
typedef struct node
{
    int priority;           // Thread priority
    void (*func)();         // Thread function code
    ucontext_t *context;    // Thread context
    struct node *next;      // Next thread in the queue
    struct node *prev;      // Previous thread in the queue
} uthread_t;

// Represents a queue of threads consisting of linked uthreads, a
// pointer to the head of the queue, the queue size, and the
// currently running uthread, which is no longer in the queue.
typedef struct queue
{
    uthread_t *head;    // Head of the queue (get to the tail by following one link forward)
    int size;           // Queue size
    uthread_t *active;  // The currently running thread
} queue_t;


// Adds the uthread to the queue.
static void add(queue_t **queue, uthread_t *item)
{
    if ((*queue)->size == 0)
    {
        // Queue is empty
        (*queue)->head = item;
        item->next = item;
        item->prev = item;
    }
    else if ((*queue)->size == 1)
    {
        // Single item in queue
        item->next = (*queue)->head;
        item->prev = (*queue)->head;
        (*queue)->head->next = item;
        (*queue)->head->prev = item;
    }
    else
    {
        // More than 1 items in queue
        (*queue)->head->next->prev = item;
        item->next = (*queue)->head->next;
        item->prev = (*queue)->head;
        (*queue)->head->next = item;
    }
    (*queue)->size++;
}

// Retrieves the next highest priority thread and removes it from the
// queue. If there are multiple threads with the same priority, it
// will take the oldest of them.
static uthread_t* get_priority_thread(queue_t **queue)
{
    if ((*queue)->size == 0)
    {
        // Empty queue
        return NULL;
    }
    
    if ((*queue)->size == 1)
    {
        // Single item in queue
        uthread_t *curr = (*queue)->head;
        (*queue)->head = NULL;
        (*queue)->size = 0;
        return curr;
    }
    
    // Find the node with the highest priority
    uthread_t *curr = (*queue)->head;
    uthread_t *priority_node = curr;
    int highest_priority = curr->priority;
    while (curr->prev != (*queue)->head)
    {
        curr = curr->prev;
        if (curr->priority < highest_priority)
        {
            priority_node = curr;
            highest_priority = curr->priority;
        }
    }
    
    // Unlink the node
    if (priority_node == (*queue)->head)
    {
        (*queue)->head = priority_node->prev;
    }
    priority_node->prev->next = priority_node->next;
    priority_node->next->prev = priority_node->prev;
    (*queue)->size--;
    
    return priority_node;
}

// Frees the memory associated with the queue.
static void cleanup_queue(queue_t *queue)
{
    uthread_t *curr = queue->head;
    while (curr)
    {
        uthread_t *to_free = curr;
        curr = curr->next;
        free(to_free);
    }
    free(queue->active);
    free(queue);
}


/////////////////////////////////////////////////////////////////////
//                     Library implementation                      //
/////////////////////////////////////////////////////////////////////


// Locking isn't actually necessary since only a single thread will
// be accessing the thread queue due to the many-to-one mapping, but
// I will use a sempahore in case we want to use a different mapping
// scheme, like many-to-many, in the future.
sem_t lock;
queue_t *thread_queue;

// This function is called before any other uthread library
// functions can be called. It initializes the uthread system.
void system_init()
{
    // Initialize thread queue
    thread_queue = (queue_t *) malloc(sizeof(queue_t));
    thread_queue->size = 0;
    
    // Initialize the semaphore
    sem_init(&lock, 0, 1);
}

// This function creates a new user-level thread which runs func(),
// with priority number specified by argument priority. This function
// returns 0 if succeeds, or -1 otherwise.
int uthread_create(void func(), int priority)
{
    // Allocate a node for the uthread
    uthread_t *thread = (uthread_t *) malloc(sizeof(uthread_t));
    if (!thread)
    {
        return -1;
    }
    
    thread->priority = priority;
    thread->func = func;
    
    // Allocate the thread context
    thread->context = (ucontext_t *) malloc(sizeof(ucontext_t));
    if (!thread->context)
    {
        return -1;
    }
    
    getcontext(thread->context);
    thread->context->uc_stack.ss_sp = malloc(STACK_SIZE);
    thread->context->uc_stack.ss_size = STACK_SIZE;
    makecontext(thread->context, thread->func, 0);
    
    // Add the thread to the queue
    sem_wait(&lock);
    add(&thread_queue, thread);
    sem_post(&lock);
    
    return 0;
}

// The calling thread requests to yield the kernel thread that
// it is currently running to one of other user threads which
// has the highest priority level among the ready threads if
// there is one or more other threads ready to run. If no
// any other thread is ready to run, the calling thread should
// proceed to run on the kernel thread. This function returns 0
// if succeeds, or -1 otherwise.
int uthread_yield(int priority)
{
    sem_wait(&lock);
    if (thread_queue->size == 0)
    {
        sem_post(&lock);
        return -1;
    }
    
    thread_queue->active->priority = priority;
    
    // Find the next thread to run
    uthread_t *thread = get_priority_thread(&thread_queue);
    
    // Add the yielding thread back into the queue and swap contexts
    uthread_t *save = thread_queue->active;
    add(&thread_queue, save);
    thread_queue->active = thread;
    sem_post(&lock);
    swapcontext(save->context, thread->context);
    
    return 0;
}

// The calling user-level thread ends its execution.
void uthread_exit()
{
    // Terminate when there are no more threads ready
    sem_wait(&lock);
    if (thread_queue->size == 0)
    {
        sem_post(&lock);
        cleanup_queue(thread_queue);
        sem_destroy(&lock);
        exit(0);
    }
    
    // Retrieve a uthread from the queue
    uthread_t *thread = get_priority_thread(&thread_queue);
    
    // Set the context and run the thread
    free(thread_queue->active);
    thread_queue->active = thread;
    sem_post(&lock);
    setcontext(thread->context);
}

