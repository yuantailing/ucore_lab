# Lab7

## [练习0] 填写已有实验

> 已完成

## [练习1] 理解内核级信号量的实现和基于内核级信号量的哲学家就餐问题

[练习1.1] 请在实验报告中给出内核级信号量的设计描述，并说其大致执行流流程。

> 内核级信号量的定义：

```
typedef struct {
    int value;
    wait_queue_t wait_queue;
} semaphore_t;
```

> value就是信号量的值，wait_queue是等待队列，当P操作后，如果等待队列非空，那么会唤醒其中一个。它有四个方法，分别是初始化（sem_init）、P操作（up）、V操作（down）和非阻塞的V操作（try_down）：

```
void sem_init(semaphore_t *sem, int value);
void up(semaphore_t *sem);
void down(semaphore_t *sem);
bool try_down(semaphore_t *sem);
```

> 初始化、P操作、V操作与理论课上讲的是一样的，不再赘述。实验里主要增加了一个非阻塞的V操作（try_down），如果信号量大于0，那么减一，返回true；如果信号量小于或等于0，那么返回false，表示V操作失败。这个功能也是很有用的。

[练习1.2] 请在实验报告中给出给用户态进程/线程提供信号量机制的设计方案，并比较说明给内核级提供信号量机制的异同。

> 增加一个系统调用SYS_SEM。SYS_SEM有两个参数，第一个参数是信号量的地址，第二个参数是要做的操作（例如初始化为0，P操作为1，V操作为2，非阻塞的V操作为3）。如果操作是初始化，那么有第三个参数，即初始化的数字value；如果操作是非阻塞的V操作，那么有一个返回值，是V操作是否成功。

> 不同：为用户态提供信号量需要用系统调用来实现，需要在用户态和内核态之间切换，可能造成效率比较低下。

> 相同：都提供了完整的信号量机制，对用户的接口可以封装成相同的。

## [练习2] 完成内核级条件变量和基于内核级条件变量的哲学家就餐问题

[练习2.1] 请在实验报告中给出内核级条件变量的设计描述，并说其大致执行流流程。

> 内核级条件变量的描述如下：

```
typedef struct condvar{
    semaphore_t sem;        // the sem semaphore  is used to down the waiting proc, and the signaling proc should up the waiting proc
    int count;              // the number of waiters on condvar
    monitor_t * owner;      // the owner(monitor) of this condvar
} condvar_t;
```

> 成员变量有信号量、等待计数、所属的管程。设计的方法有：

```
// Unlock one of threads waiting on the condition variable. 
void     cond_signal (condvar_t *cvp);
// Suspend calling thread on a condition variable waiting for condition atomically unlock mutex in monitor,
// and suspends calling thread on conditional variable after waking up locks mutex.
void     cond_wait (condvar_t *cvp);
```

> 执行流程：cond_signal的作用是唤醒一个正在wait同一个monitor的进程/线程，把它从等待队列移动到就绪队列。cond_wait的作用是进入所属monitor的等待队列，等待被其它进程/线程唤醒。

[练习2.2] 请在实验报告中给出给用户态进程/线程提供条件变量机制的设计方案，并比较说明给内核级提供条件变量机制的异同。

> 增加一个系统调用SYS_COND。SYS_COND有两个参数，第一个参数是要做的操作（例如signal为0，wait为1），第二个变量是信号量的地址。遇到这个系统调用的时候，根据第一个参数来选择调用cond_signal还是cond_wait。

> 不同：为用户态提供条件变量需要用系统调用来实现，需要在用户态和内核态之间切换，可能造成效率比较低下。

> 相同：都提供了完整的条件变量机制，对用户的接口可以封装成相同的。

## 与参考答案的区别：

> 本次实验比较简单，做法也比较固定，除了我认为自己的代码风格比较好以外，与参考答案没有别的区别。

## 重要的知识点

> - 信号量
> - 条件变量
> - 管程

> 区别：实验中的信号量比理论课学的信号量多一个`try_down`方法，实用性更强。
