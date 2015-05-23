#include <defs.h>
#include <list.h>
#include <proc.h>
#include <assert.h>
#include <default_sched.h>

#define find_first_zero(addr) ({\
        int __res;\
        __asm__ __volatile__("cld\n"\
            "1:\tlodsl\n\t"\
            "notl %%eax\n\t"\
            "bsfl %%eax,%%edx\n\t"\
            "je 2f\n\t"\
            "addl %%edx,%%ecx\n\t"\
            "jmp 3f\n"\
            "2:\t addl $32,%%ecx\n\t"\
            "cmpl $8192,%%ecx\n\t"\
            "jl 1b\n"\
            "3:"\
            :"=c"(__res):"c"(0),"S"(addr));\
        __res;})

static int
sched_o1_find_first_bit(uint32_t bitmap[BITMAP_SIZE]) {
    int i;
    for (i = 0; i < BITMAP_SIZE; i++) {
        if (bitmap[i]) {
            int x = ~bitmap[i];
            return i * 32 + 31 - find_first_zero(&x);
        }
    }
}

/*
 * stride_init initializes the run-queue rq with correct assignment for
 * member variables, including:
 *
 *   - run_list: should be a empty list after initialization.
 *   - lab6_run_pool: NULL
 *   - proc_num: 0
 *   - max_time_slice: no need here, the variable would be assigned by the caller.
 *
 * hint: see libs/list.h for routines of the list structures.
 */
static void
stride_init(struct run_queue *rq) {
     /* LAB6: 2012012017
      * (1) init the ready process list: rq->run_list
      * (2) init the run pool: rq->lab6_run_pool
      * (3) set number of process: rq->proc_num to 0       
      */
    memset(rq->array_buf, 0, sizeof(rq->array_buf));
    rq->active = &rq->array_buf[0];
    rq->expired = &rq->array_buf[1];
    int i, j;
    for (i = 0; i < 2; i++) {
        for (j = 0; j < MAX_PRIO; j++)
            list_init(&rq->array_buf[i].queue[j]);
    }
}

/*
 * stride_enqueue inserts the process ``proc'' into the run-queue
 * ``rq''. The procedure should verify/initialize the relevant members
 * of ``proc'', and then put the ``lab6_run_pool'' node into the
 * queue(since we use priority queue here). The procedure should also
 * update the meta date in ``rq'' structure.
 *
 * proc->time_slice denotes the time slices allocation for the
 * process, which should set to rq->max_time_slice.
 * 
 * hint: see libs/skew_heap.h for routines of the priority
 * queue structures.
 */
static void
stride_enqueue(struct run_queue *rq, struct proc_struct *proc) {
     /* LAB6: 2012012017
      * (1) insert the proc into rq correctly
      * NOTICE: you can use skew_heap or list. Important functions
      *         skew_heap_insert: insert a entry into skew_heap
      *         list_add_before: insert  a entry into the last of list   
      * (2) recalculate proc->time_slice
      * (3) set proc->rq pointer to rq
      * (4) increase rq->proc_num
      */
    //cprintf("  enqueue pid=%d, priority=%d\n", proc->pid, proc->lab6_priority);
    proc->time_slice = proc->lab6_priority;
    proc->rq = rq;
    if (proc->lab6_priority == 0) proc->lab6_priority = 1;
    rq->expired->nr_active++;
    uint32_t priority = proc->lab6_priority;
    rq->expired->bitmap[priority / 32] |= 1 << (32 - 1 - priority % 32);
    list_add(&rq->expired->queue[priority], &proc->run_link);
}

/*
 * stride_dequeue removes the process ``proc'' from the run-queue
 * ``rq'', the operation would be finished by the skew_heap_remove
 * operations. Remember to update the ``rq'' structure.
 *
 * hint: see libs/skew_heap.h for routines of the priority
 * queue structures.
 */
static void
stride_dequeue(struct run_queue *rq, struct proc_struct *proc) {
     /* LAB6: 2012012017
      * (1) remove the proc from rq correctly
      * NOTICE: you can use skew_heap or list. Important functions
      *         skew_heap_remove: remove a entry from skew_heap
      *         list_del_init: remove a entry from the  list
      */
    //cprintf("  dequeue pid=%d, priority=%d\n", proc->pid, proc->lab6_priority);
    rq->active->nr_active--;
    uint32_t priority = proc->lab6_priority;
    list_del(&proc->run_link);
    if (list_empty(&rq->active->queue[priority]))
        rq->active->bitmap[priority / 32] &= ~(1 << (32 - 1 - priority % 32));
}
/*
 * stride_pick_next pick the element from the ``run-queue'', with the
 * minimum value of stride, and returns the corresponding process
 * pointer. The process pointer would be calculated by macro le2proc,
 * see kern/process/proc.h for definition. Return NULL if
 * there is no process in the queue.
 *
 * When one proc structure is selected, remember to update the stride
 * property of the proc. (stride += BIG_STRIDE / priority)
 *
 * hint: see libs/skew_heap.h for routines of the priority
 * queue structures.
 */
static struct proc_struct *
stride_pick_next(struct run_queue *rq) {
     /* LAB6: 2012012017
      * (1) get a  proc_struct pointer p  with the minimum value of stride
             (1.1) If using skew_heap, we can use le2proc get the p from rq->lab6_run_poll
             (1.2) If using list, we have to search list to find the p with minimum stride value
      * (2) update p;s stride value: p->lab6_stride
      * (3) return p
      */
    if (rq->active->nr_active == 0) {
        struct prio_array *t = rq->active;
        rq->active = rq->expired;
        rq->expired = t;
    }
    int first_bit = sched_o1_find_first_bit(rq->active->bitmap);
    struct proc_struct *proc = le2proc(list_next(&rq->active->queue[first_bit]), run_link);
    //cprintf("  picked  pid=%d, priority=%d\n", proc->pid, proc->lab6_priority);
    return proc;
}

/*
 * stride_proc_tick works with the tick event of current process. You
 * should check whether the time slices for current process is
 * exhausted and update the proc struct ``proc''. proc->time_slice
 * denotes the time slices left for current
 * process. proc->need_resched is the flag variable for process
 * switching.
 */
static void
stride_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
     /* LAB6: 2012012017 */
    if (--proc->time_slice <= 0) proc->need_resched = 1;
}

struct sched_class default_sched_class = {
    .name = "stride_scheduler",
    .init = stride_init,
    .enqueue = stride_enqueue,
    .dequeue = stride_dequeue,
    .pick_next = stride_pick_next,
    .proc_tick = stride_proc_tick,
};
