# Lab6

## [练习0] 填写已有实验

> 已完成

## [练习1] 使用 Round Robin 调度算法

[练习1.1] 请理解并分析sched_calss中各个函数指针的用法，并接合Round Robin 调度算法描ucore的调度执行过程

> 从代码上分析，主要有这几个指针：

```
struct sched_class default_sched_class = {
    .name = "stride_scheduler",
    .init = stride_init,
    .enqueue = stride_enqueue,
    .dequeue = stride_dequeue,
    .pick_next = stride_pick_next,
    .proc_tick = stride_proc_tick,
};
```

> 其中这些是函数指针：

```
    .init = stride_init,
    .enqueue = stride_enqueue,
    .dequeue = stride_dequeue,
    .pick_next = stride_pick_next,
    .proc_tick = stride_proc_tick,
```

> init在程序开始执行，所有进程创建之前被调用。
> enqueue在进程加入就绪队列时被调用。
> dequeue在进程被移出就绪队列时被调用。
> pick_next在进程切换时被调用，用它的返回值作为切换到的进程。
> proc_tick在产生时钟中断时被调用。

[练习1.2] 请在实验报告中简要说明如何设计实现”多级反馈队列调度算法“，给出概要设计，鼓励给出详细设计

> 概要设计：有一个队列的列表，它是队列的总调度器，由它决定从哪个队列里pick_next作为切换到的进程。可以有多个队列，它的功能可能就是实现一个“Round Robin调度算法”或“Stride Scheduling调度算法”。“总调度器”把一个进程绑定在哪个队列上，那么对这个进程的`enqueue`、`dequeue`都由相应的队列执行。发生`proc_tick`时，应该调用上一次选出切换进程的队列的`proc_tick`。

## [练习2] 实现 Stride Scheduling 调度算法

[练习2.0] 请在实验报告中简要说明你的设计实现过程

> 根据宏不同`USE_SKEW_HEAP`，为调度算法定义了不同的行为。宏为1时使用左式堆（斜堆），效率可能更高。

> 初始化：队列内进程数量为0，根据宏初始化堆或链表。

```
#if USE_SKEW_HEAP
    rq->lab6_run_pool = 0;
#else
    list_init(&rq->run_list);
#endif
    rq->proc_num = 0;
```

> 入队：加入堆或加入链表，为进程分配时间片。更新调度队列中进程的总数量。（如果优先级未被初始化过，在加入队列时初始化它）

```
#if USE_SKEW_HEAP
    rq->lab6_run_pool = skew_heap_insert(rq->lab6_run_pool, &(proc->lab6_run_pool), proc_stride_comp_f);
#else
    list_add(&rq->run_list, &proc->run_link);
#endif
    proc->time_slice = rq->max_time_slice;
    proc->rq = rq;
    if (proc->lab6_priority == 0) proc->lab6_priority = 1;
    rq->proc_num++;
```

> 出队：从堆中删除或从链表中删除。更新调度队列中进程的总数量。

```
#if USE_SKEW_HEAP
    rq->lab6_run_pool = skew_heap_remove(rq->lab6_run_pool, &(proc->lab6_run_pool), proc_stride_comp_f);
#else
    list_del(&proc->run_link);
#endif
    rq->proc_num--;
```

> 选取：如果使用左式堆，选取堆顶元素即可；否则，遍历链表中的元素，找出lab6_stride最小的进程。

```
#if USE_SKEW_HEAP
    if (!rq->lab6_run_pool) return NULL;
    struct proc_struct *p = le2proc(rq->lab6_run_pool, lab6_run_pool);
#else
    list_entry_t *le;
    struct proc_struct *p = 0;
    for (le = list_next(&rq->run_list); le != &rq->run_list; le = list_next(le)) {
        struct proc_struct *q = le2proc(le, run_link);
        if (!p || proc_stride_comp_f(&p->lab6_run_pool, &q->lab6_run_pool) == 1) p = q;
    }
#endif
    if (p) p->lab6_stride += BIG_STRIDE / p->lab6_priority;
    return p;
```

> 时钟中断：时间片减一。如果时间片用尽，重新调度之。

```
    if (--proc->time_slice <= 0) proc->need_resched = 1;
```


## 与参考答案的区别：

> Exercise 4我一次性用memset将新分配的proc所有成员变量都置为0，因此LAB5中alloc_proc函数不需要更新。参考答案在LAB4是将proc的各个成员变量分别赋值，因此多初始化几个成员变量需要更新代码。

> 我在[kern/schedule/sched.c](kern/schedule/sched.c)里加入一个全局函数schedule_tick来开放sched_class_proc_tick的调用接口，不修改原有代码，更符合面向对象编程原则。

> 调度算法的stride_proc_tick函数中，我比参考答案少一次判断。因此每个时钟周期到来时，我的算法都因此比参考答案快一点。

> 如果宏USE_SKEW_HEAP为1，使用斜堆来优化调度算法，那么就没有用到run_list，因此我在stride_init函数里没有调用list_init(&rq->run_list)，比参考答案效率更高。

> 参考答案中，初始化的priority被设置成最大优先级，但我认为应该把它设置成最低优先级（1）。

> 不使用斜堆，而使用链表时，可以复用斜堆的比较函数proc_stride_comp_f的代码。这使程序维护方便，也更符合封装原则，减少硬编码（hard-coding）。

## 重要的知识点

> Round Robin调度算法；

> Stride Scheduling调度算法；

> 多级反馈队列调度算法；

> 左式堆（斜堆）。//但我认为在调度算法中，堆每次至多合并一个元素，不能体现左式堆的优势，使用普通的完全二叉堆达到的算法效率是一样的。
