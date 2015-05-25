Lab6 - Challange 2
==================

参考Linux2.6的O(1)调度器，实现简化的O(1)调度算法

实验人员
--------

|       学号 | 姓名   |                邮箱
| ---------: | :----- | ------------------:
| 2012011382 | 李浩达 | lihaoda9@163.com
| 2012012017 | 袁泰凌 | yuantailing@126.com


实验结果
--------

1. 进程优先级划分

  模仿Linux 2.6内核，划分为140个优先级。尽管Lab6的所有测例并没有用到这么多优先级，我已经支持了这么多优先级。

2. 就绪队列

  就绪队列用如下数据结构描述：

  ```
struct prio_array {
    int nr_active;
    list_entry_t queue[MAX_PRIO];
    uint32_t bitmap[BITMAP_SIZE];
};
  ```

  一个运行队列有一个就绪队列active、一个已调度队列expired。进程加入运行队列时，加入expired队列。选择进程时，从就绪队列选。如果就绪队列为空，那么交换就绪队列与运行队列。交换就绪队列和运行队列，而不需要遍历队列内所有进程，这是O(1)调度器的关键所在。

3. 位图

  queue[i]是优先级为i的进程链表。为了快速定位哪个链表非空，使用bitmap。

  bitmap的第i位为1表示queue[i]链表非空，为0表示链表为空。find_first_zero使用一系列汇编指令，高效地找出bitmap里的第一位0。封装这个方法，可以高效地找出第一位1。

  位图应该与进程队列同步更新。进程加入队列时，对应位应该置1。进程被取出队列时，如果队列变为空，应把对应位置为0。

4. 时间片

  Linux 2.4内核在所有就绪进程的时间片都耗完后再在调度器中一次性重算。新的Linux调度程序减少了对循环的依赖，取而代之的是为每个处理器维护两个优先级数组：活动数组和过期数组。活动数组内的队列上的进程还有时间片剩余；而过期数组内的队列上的进程都耗尽了时间片。当一个进程的时间片耗尽时，它会被移至过期数组。但在此之前，时间片已经重算好了，接下来只要在活动和过期数组之间切换即可。

  本来Linux的优先级定义为0为最高，139为最低。但Lab6的priority测例要求优先级之比为a:b的两个进程的运行时间，应该约等于a:b，相当于优先级数值大的优先级最高，这与Linux的优先级定义是相反的。在本次实验里，为了兼容priority测例，按实验里的约定，令时间片大小等于优先级，这样就可以正常通过该测例了。

5. 总结

  综上，每次取出需要找到第一位1，移出链表，更新运行队列内进程数量。以上操作都可以在常数时间内完成。

  把进程加入运行队列，需要加入链表，位图置为，更新运行队列内进程数量等操作，都可以在常数时间内完成。

  在位图的帮助下，可以快速找到优先级最高的队列，优化常数。


参考资料：

  > 张永选. Linux 2.6内核O(1)调度算法剖析. 韶关学院学报. Jun.2009, Vol.30, No.6


附录1 - `make run-waitkill`打印运行队列变化的全过程

```
(THU.CST) os is loading ...

Special kernel symbols:
  entry  0xc010002a (phys)
  etext  0xc010bf69 (phys)
  edata  0xc01addd4 (phys)
  end    0xc01b1958 (phys)
Kernel executable memory footprint: 711KB
ebp:0xc012bf38 eip:0xc0100ae4 args:0x00010094 0x00000000 0xc012bf68 0xc01000d8 
    kern/debug/kdebug.c:351: print_stackframe+21
ebp:0xc012bf48 eip:0xc0100dd3 args:0x00000000 0x00000000 0x00000000 0xc012bfb8 
    kern/debug/kmonitor.c:129: mon_backtrace+10
ebp:0xc012bf68 eip:0xc01000d8 args:0x00000000 0xc012bf90 0xffff0000 0xc012bf94 
    kern/init/init.c:59: grade_backtrace2+33
ebp:0xc012bf88 eip:0xc0100101 args:0x00000000 0xffff0000 0xc012bfb4 0x0000002a 
    kern/init/init.c:64: grade_backtrace1+38
ebp:0xc012bfa8 eip:0xc010011f args:0x00000000 0xc010002a 0xffff0000 0x0000001d 
    kern/init/init.c:69: grade_backtrace0+23
ebp:0xc012bfc8 eip:0xc0100144 args:0xc010bf9c 0xc010bf80 0x00003b84 0x00000000 
    kern/init/init.c:74: grade_backtrace+34
ebp:0xc012bff8 eip:0xc010007f args:0x00000000 0x00000000 0x0000ffff 0x40cf9a00 
    kern/init/init.c:33: kern_init+84
memory management: default_pmm_manager
e820map:
  memory: 0009fc00, [00000000, 0009fbff], type = 1.
  memory: 00000400, [0009fc00, 0009ffff], type = 2.
  memory: 00010000, [000f0000, 000fffff], type = 2.
  memory: 07efe000, [00100000, 07ffdfff], type = 1.
  memory: 00002000, [07ffe000, 07ffffff], type = 2.
  memory: 00040000, [fffc0000, ffffffff], type = 2.
check_alloc_page() succeeded!
check_pgdir() succeeded!
check_boot_pgdir() succeeded!
-------------------- BEGIN --------------------
PDE(0e0) c0000000-f8000000 38000000 urw
  |-- PTE(38000) c0000000-f8000000 38000000 -rw
PDE(001) fac00000-fb000000 00400000 -rw
  |-- PTE(000e0) faf00000-fafe0000 000e0000 urw
  |-- PTE(00001) fafeb000-fafec000 00001000 -rw
--------------------- END ---------------------
use SLOB allocator
kmalloc_init() succeeded!
check_vma_struct() succeeded!
page fault at 0x00000100: K/W [no page found].
check_pgfault() succeeded!
check_vmm() succeeded.
sched class: stride_scheduler
  enqueue pid=1, priority=0
ide 0:      10000(sectors), 'QEMU HARDDISK'.
ide 1:     262144(sectors), 'QEMU HARDDISK'.
SWAP: manager = fifo swap manager
BEGIN check_swap: count 31848, total 31848
setup Page Table for vaddr 0X1000, so alloc a page
setup Page Table vaddr 0~4MB OVER!
set up init env for check_swap begin!
page fault at 0x00001000: K/W [no page found].
page fault at 0x00002000: K/W [no page found].
page fault at 0x00003000: K/W [no page found].
page fault at 0x00004000: K/W [no page found].
set up init env for check_swap over!
write Virt Page c in fifo_check_swap
write Virt Page a in fifo_check_swap
write Virt Page d in fifo_check_swap
write Virt Page b in fifo_check_swap
write Virt Page e in fifo_check_swap
page fault at 0x00005000: K/W [no page found].
swap_out: i 0, store page in vaddr 0x1000 to disk swap entry 2
write Virt Page b in fifo_check_swap
write Virt Page a in fifo_check_swap
page fault at 0x00001000: K/W [no page found].
swap_out: i 0, store page in vaddr 0x2000 to disk swap entry 3
swap_in: load disk swap entry 2 with swap_page in vadr 0x1000
write Virt Page b in fifo_check_swap
page fault at 0x00002000: K/W [no page found].
swap_out: i 0, store page in vaddr 0x3000 to disk swap entry 4
swap_in: load disk swap entry 3 with swap_page in vadr 0x2000
write Virt Page c in fifo_check_swap
page fault at 0x00003000: K/W [no page found].
swap_out: i 0, store page in vaddr 0x4000 to disk swap entry 5
swap_in: load disk swap entry 4 with swap_page in vadr 0x3000
write Virt Page d in fifo_check_swap
page fault at 0x00004000: K/W [no page found].
swap_out: i 0, store page in vaddr 0x5000 to disk swap entry 6
swap_in: load disk swap entry 5 with swap_page in vadr 0x4000
count is 5, total is 5
check_swap() succeeded!
++ setup timer interrupts
  picked  pid=1, priority=1
  dequeue pid=1, priority=1
  enqueue pid=2, priority=0
  picked  pid=2, priority=1
  dequeue pid=2, priority=1
kernel_execve: pid = 2, name = "waitkill".
  enqueue pid=3, priority=0
  enqueue pid=4, priority=0
wait child 1.
  enqueue pid=2, priority=1
  picked  pid=2, priority=1
  dequeue pid=2, priority=1
  picked  pid=4, priority=1
  dequeue pid=4, priority=1
child 2.
  enqueue pid=4, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
child 1.
  enqueue pid=3, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
  enqueue pid=3, priority=1
  picked  pid=4, priority=1
  dequeue pid=4, priority=1
  enqueue pid=4, priority=1
  picked  pid=4, priority=1
  dequeue pid=4, priority=1
  enqueue pid=4, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
  enqueue pid=3, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
  enqueue pid=3, priority=1
  picked  pid=4, priority=1
  dequeue pid=4, priority=1
  enqueue pid=4, priority=1
  picked  pid=4, priority=1
  dequeue pid=4, priority=1
  enqueue pid=4, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
  enqueue pid=3, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
  enqueue pid=3, priority=1
  picked  pid=4, priority=1
  dequeue pid=4, priority=1
  enqueue pid=4, priority=1
  picked  pid=4, priority=1
  dequeue pid=4, priority=1
  enqueue pid=4, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
  enqueue pid=3, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
  enqueue pid=3, priority=1
  picked  pid=4, priority=1
  dequeue pid=4, priority=1
  enqueue pid=4, priority=1
  picked  pid=4, priority=1
  dequeue pid=4, priority=1
  enqueue pid=4, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
  enqueue pid=3, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
  enqueue pid=3, priority=1
  picked  pid=4, priority=1
  dequeue pid=4, priority=1
  enqueue pid=4, priority=1
  picked  pid=4, priority=1
  dequeue pid=4, priority=1
  enqueue pid=4, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
  enqueue pid=3, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
  enqueue pid=3, priority=1
  picked  pid=4, priority=1
  dequeue pid=4, priority=1
  enqueue pid=4, priority=1
  picked  pid=4, priority=1
  dequeue pid=4, priority=1
  enqueue pid=2, priority=1
kill parent ok.
  enqueue pid=4, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
  enqueue pid=3, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
  enqueue pid=3, priority=1
  picked  pid=4, priority=1
  dequeue pid=4, priority=1
  enqueue pid=4, priority=1
  picked  pid=2, priority=1
  dequeue pid=2, priority=1
  enqueue pid=1, priority=1
  picked  pid=1, priority=1
  dequeue pid=1, priority=1
  enqueue pid=1, priority=1
  picked  pid=4, priority=1
  dequeue pid=4, priority=1
  enqueue pid=4, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
  enqueue pid=3, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
  enqueue pid=3, priority=1
  picked  pid=4, priority=1
  dequeue pid=4, priority=1
  enqueue pid=4, priority=1
  picked  pid=1, priority=1
  dequeue pid=1, priority=1
  picked  pid=4, priority=1
  dequeue pid=4, priority=1
  enqueue pid=4, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
  enqueue pid=3, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
  enqueue pid=3, priority=1
  picked  pid=4, priority=1
  dequeue pid=4, priority=1
  enqueue pid=4, priority=1
  picked  pid=4, priority=1
  dequeue pid=4, priority=1
  enqueue pid=4, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
  enqueue pid=3, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
  enqueue pid=3, priority=1
  picked  pid=4, priority=1
  dequeue pid=4, priority=1
  enqueue pid=4, priority=1
  picked  pid=4, priority=1
  dequeue pid=4, priority=1
  enqueue pid=4, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
  enqueue pid=3, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
  enqueue pid=3, priority=1
  picked  pid=4, priority=1
  dequeue pid=4, priority=1
  enqueue pid=4, priority=1
  picked  pid=4, priority=1
  dequeue pid=4, priority=1
  enqueue pid=4, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
  enqueue pid=3, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
  enqueue pid=3, priority=1
  picked  pid=4, priority=1
  dequeue pid=4, priority=1
kill child1 ok.
  enqueue pid=1, priority=1
  picked  pid=1, priority=1
  dequeue pid=1, priority=1
  enqueue pid=1, priority=1
  picked  pid=3, priority=1
  dequeue pid=3, priority=1
  picked  pid=1, priority=1
  dequeue pid=1, priority=1
  enqueue pid=1, priority=1
  picked  pid=1, priority=1
  dequeue pid=1, priority=1
all user-mode processes have quit.
init check memory pass.
kernel panic at kern/process/proc.c:456:
    initproc exit.

Welcome to the kernel debug monitor!!
Type 'help' for a list of commands.
K>
```
