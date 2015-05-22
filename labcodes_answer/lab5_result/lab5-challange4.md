Lab5 - Challange 4
==================

实验人员
--------

|       学号 | 姓名   |                邮箱
| ---------: | :----- | ------------------:
| 2012011382 | 李浩达 | lihaoda9@163.com
| 2012012017 | 袁泰凌 | yuantailing@126.com


实验结果
--------

1. 挖掘内存泄漏现象

  定位内存泄漏的第一步，是找到哪些修改会影响到内存泄漏的数量，然后追踪这些修改会影响操作系统的哪些执行部分。
  
  把forktest的进程数量调整到320，打印nr_free_pages_store,和nr_free_pages()：
  
  ```nr_free_pages_store=31823, nr_free_pages()=31800```
  
  再把proc.h里PROC_NAME_LEN改成1023，打印信息：
  
  ```nr_free_pages_store=31822, nr_free_pages()=31715```
  
  因此，内存泄漏的多少，与进程分配的数量、进程控制块的大小都有关。

2. 追踪kmalloc

  （以下都在forktest进程数量32，PROC_NAME_LEN定义为127的情况下调试）
  
  从上一步得知，内存泄漏与进程分配有关。追踪`static struct proc_struct *alloc_proc(void)`这个函数，猜想里面调用的kmalloc是内存泄漏的根源。先确定是以下两种情况中的哪一种，再做后续研究：
  
  1. 如果kmalloc、kfree不能配对，说明有的进程用kmalloc分配后，没有用kfree释放进程控制块。可能的原因是进程结束后忘记调用kfree。
  2. 如果kmalloc、kfree是配对的，说明kmalloc的内部实现有误。kmalloc内部，有kmalloc本身的代码，用到slob分配器。slob分配器有用到default_pmm。以上三部分都可能出错，定位在哪个模块出错，再调试。
  
  输出调试信息，对应代码commit版本是84d5769b6159684a4208f07256db3a9df8328a8c，输出见[附录1](#附录1-检查kmalloc与kfree配对)。仔细配对，发现kmalloc与kfree是完全配对的。
  
  至此，已确定是上述第二个原因。

3. 追踪alloc_pages

  根据alloc_pages与free_pages是否配对，可以把问题划分成两部分，定位到其中的一部分：

  1. 如果alloc_pages与free_pages不能配对，说明kmalloc调用slob_alloc与kmalloc调用slob_free的次数不匹配，或者slob_alloc调用alloc_pages与slob_free调用free_pages的次数不匹配。
  2. 如果alloc_pages与free_pages是配对的，说明default_pmm出错。
  
  输出调试信息，对应代码commit版本是46e8e38ac8cf87e1eb0386e9563aa80daec98638，输出见[附录2](#附录2-检查alloc_pages与free_pages配对)。仔细配对，发现刚好有三页alloc_pages后没有对应的调用free_pages：

  ```
    c01b3894
    c01b5058
    c01b6864
  ```

  因为内存泄漏的数量也是3页，所以估计就是这三页没有调用free_pages导致的内存泄漏。在0xc01b3894分配的时候打印栈帧，见[附录3](#附录3-未回收内存分配时的栈帧)。
  
  至此，定位内存泄漏的来源是__slob_get_free_pages和slob_alloc。

4. 检查slob_free

  发现__slob_get_free_page刚好调用了3次，即等于内存泄漏次数。猜测__slob_get_free_page分配的页都没有释放。
  
  解决方案：改进SLOB分配器，在一整页被释放后，调用free_pages释放物理页。
  
  1. 判断一整页被释放
    判断条件是`cur->units == SLOB_UNITS(PAGE_SIZE)`，即slob_block正好一整页的内容。
  
  2. 只合并同一页的slob_block
    修改了struct slob_block，记录所在的页。 如果不加这个限制，上一步可能导致跨页合并（跨页合并这在测例forktest无法体现）。不跨页合并会导致slob_block链增长，查找长度增加，但可以证明，这仅是常数增加，不会带来复杂度增加。

  3. 刚由__slob_get_free_page分配的页不释放
    slob_block链不够时，由__slob_get_free_page分配一页放入slob容器。如果刚分配就释放，将导致slob_block链永远为空，无法工作。

5. 测试

  经测试，forktest进程数量开至3200，仍然没有出现上述内存泄漏现象。
  
  至此，认为kmalloc里的内存泄漏被解决。

  ```
I am child 0
forktest pass.
all user-mode processes have quit.
nr_free_pages_store=31822, nr_free_pages()=31822
init check memory pass.
kernel panic at kern/process/proc.c:454:
    initproc exit.

  ```
  
附录
----

### 附录1 - 检查kmalloc与kfree配对

```
++ setup timer interrupts
================ nr_free_pages_store RECORDED ===================
  kmalloc: c03a81f8, size=236
kernel_execve: pid = 2, name = "forktest".
  kmalloc: c03a82f0, size=32
  kmalloc: c03a8318, size=24
  kmalloc: c03a8338, size=24
  kmalloc: c03a8358, size=24
  kmalloc: c03a8378, size=24
  kmalloc: c03a8398, size=236
  kmalloc: c03a8490, size=32
  kmalloc: c03a84b8, size=24
  kmalloc: c03a84d8, size=24
  kmalloc: c03a84f8, size=24
  kmalloc: c03a8518, size=24
  kmalloc: c03a8538, size=236
  kmalloc: c03a8630, size=32
  kmalloc: c03a8658, size=24
  kmalloc: c03a8678, size=24
  kmalloc: c03a8698, size=24
  kmalloc: c03a86b8, size=24
  kmalloc: c03a86d8, size=236
  kmalloc: c03a87d0, size=32
  kmalloc: c03a87f8, size=24
  kmalloc: c03a8818, size=24
  kmalloc: c03a8838, size=24
  kmalloc: c03a8858, size=24
  kmalloc: c03a8878, size=236
  kmalloc: c03a8970, size=32
  kmalloc: c03a8998, size=24
  kmalloc: c03a89b8, size=24
  kmalloc: c03a89d8, size=24
  kmalloc: c03a89f8, size=24
  kmalloc: c03a8a18, size=236
  kmalloc: c03a8b10, size=32
  kmalloc: c03a8b38, size=24
  kmalloc: c03a8b58, size=24
  kmalloc: c03a8b78, size=24
  kmalloc: c03a8b98, size=24
  kmalloc: c03a8bb8, size=236
  kmalloc: c03a8cb0, size=32
  kmalloc: c03a8cd8, size=24
  kmalloc: c03a8cf8, size=24
  kmalloc: c03a8d18, size=24
  kmalloc: c03a8d38, size=24
  kmalloc: c03a8d58, size=236
  kmalloc: c03a8e50, size=32
  kmalloc: c03a8e78, size=24
  kmalloc: c03a8e98, size=24
  kmalloc: c03a8eb8, size=24
  kmalloc: c03a8ed8, size=24
  kmalloc: c03a8ef8, size=236
  kmalloc: c043a008, size=32
  kmalloc: c043a030, size=24
  kmalloc: c043a050, size=24
  kmalloc: c043a070, size=24
  kmalloc: c043a090, size=24
  kmalloc: c043a0b0, size=236
  kmalloc: c043a1a8, size=32
  kmalloc: c043a1d0, size=24
  kmalloc: c043a1f0, size=24
  kmalloc: c043a210, size=24
  kmalloc: c043a230, size=24
  kmalloc: c043a250, size=236
  kmalloc: c043a348, size=32
  kmalloc: c043a370, size=24
  kmalloc: c043a390, size=24
  kmalloc: c043a3b0, size=24
  kmalloc: c043a3d0, size=24
  kmalloc: c043a3f0, size=236
  kmalloc: c043a4e8, size=32
  kmalloc: c043a510, size=24
  kmalloc: c043a530, size=24
  kmalloc: c043a550, size=24
  kmalloc: c043a570, size=24
  kmalloc: c043a590, size=236
  kmalloc: c043a688, size=32
  kmalloc: c043a6b0, size=24
  kmalloc: c043a6d0, size=24
  kmalloc: c043a6f0, size=24
  kmalloc: c043a710, size=24
  kmalloc: c043a730, size=236
  kmalloc: c043a828, size=32
  kmalloc: c043a850, size=24
  kmalloc: c043a870, size=24
  kmalloc: c043a890, size=24
  kmalloc: c043a8b0, size=24
  kmalloc: c043a8d0, size=236
  kmalloc: c043a9c8, size=32
  kmalloc: c043a9f0, size=24
  kmalloc: c043aa10, size=24
  kmalloc: c043aa30, size=24
  kmalloc: c043aa50, size=24
  kmalloc: c043aa70, size=236
  kmalloc: c043ab68, size=32
  kmalloc: c043ab90, size=24
  kmalloc: c043abb0, size=24
  kmalloc: c043abd0, size=24
  kmalloc: c043abf0, size=24
  kmalloc: c043ac10, size=236
  kmalloc: c043ad08, size=32
  kmalloc: c043ad30, size=24
  kmalloc: c043ad50, size=24
  kmalloc: c043ad70, size=24
  kmalloc: c043ad90, size=24
  kmalloc: c043adb0, size=236
  kmalloc: c043aea8, size=32
  kmalloc: c043aed0, size=24
  kmalloc: c043aef0, size=24
  kmalloc: c043af10, size=24
  kmalloc: c043af30, size=24
  kmalloc: c04e3008, size=236
  kmalloc: c04e3100, size=32
  kmalloc: c04e3128, size=24
  kmalloc: c04e3148, size=24
  kmalloc: c04e3168, size=24
  kmalloc: c04e3188, size=24
  kmalloc: c04e31a8, size=236
  kmalloc: c04e32a0, size=32
  kmalloc: c04e32c8, size=24
  kmalloc: c04e32e8, size=24
  kmalloc: c04e3308, size=24
  kmalloc: c04e3328, size=24
  kmalloc: c04e3348, size=236
  kmalloc: c04e3440, size=32
  kmalloc: c04e3468, size=24
  kmalloc: c04e3488, size=24
  kmalloc: c04e34a8, size=24
  kmalloc: c04e34c8, size=24
  kmalloc: c04e34e8, size=236
  kmalloc: c04e35e0, size=32
  kmalloc: c04e3608, size=24
  kmalloc: c04e3628, size=24
  kmalloc: c04e3648, size=24
  kmalloc: c04e3668, size=24
  kmalloc: c04e3688, size=236
  kmalloc: c04e3780, size=32
  kmalloc: c04e37a8, size=24
  kmalloc: c04e37c8, size=24
  kmalloc: c04e37e8, size=24
  kmalloc: c04e3808, size=24
  kmalloc: c04e3828, size=236
  kmalloc: c04e3920, size=32
  kmalloc: c04e3948, size=24
  kmalloc: c04e3968, size=24
  kmalloc: c04e3988, size=24
  kmalloc: c04e39a8, size=24
  kmalloc: c04e39c8, size=236
  kmalloc: c04e3ac0, size=32
  kmalloc: c04e3ae8, size=24
  kmalloc: c04e3b08, size=24
  kmalloc: c04e3b28, size=24
  kmalloc: c04e3b48, size=24
  kmalloc: c04e3b68, size=236
  kmalloc: c04e3c60, size=32
  kmalloc: c04e3c88, size=24
  kmalloc: c04e3ca8, size=24
  kmalloc: c04e3cc8, size=24
  kmalloc: c04e3ce8, size=24
  kmalloc: c04e3d08, size=236
  kmalloc: c04e3e00, size=32
  kmalloc: c04e3e28, size=24
  kmalloc: c04e3e48, size=24
  kmalloc: c04e3e68, size=24
  kmalloc: c04e3e88, size=24
  kmalloc: c04e3ea8, size=236
  kmalloc: c04e3fa0, size=32
  kmalloc: c04e3fc8, size=24
  kmalloc: c04e3fe8, size=24
  kmalloc: c043af50, size=24
  kmalloc: c043af70, size=24
  kmalloc: c058e008, size=236
  kmalloc: c058e100, size=32
  kmalloc: c058e128, size=24
  kmalloc: c058e148, size=24
  kmalloc: c058e168, size=24
  kmalloc: c058e188, size=24
  kmalloc: c058e1a8, size=236
  kmalloc: c058e2a0, size=32
  kmalloc: c058e2c8, size=24
  kmalloc: c058e2e8, size=24
  kmalloc: c058e308, size=24
  kmalloc: c058e328, size=24
  kmalloc: c058e348, size=236
  kmalloc: c058e440, size=32
  kmalloc: c058e468, size=24
  kmalloc: c058e488, size=24
  kmalloc: c058e4a8, size=24
  kmalloc: c058e4c8, size=24
  kmalloc: c058e4e8, size=236
  kmalloc: c058e5e0, size=32
  kmalloc: c058e608, size=24
  kmalloc: c058e628, size=24
  kmalloc: c058e648, size=24
  kmalloc: c058e668, size=24
  kmalloc: c058e688, size=236
  kmalloc: c058e780, size=32
  kmalloc: c058e7a8, size=24
  kmalloc: c058e7c8, size=24
  kmalloc: c058e7e8, size=24
  kmalloc: c058e808, size=24
I am child 31
  kfree: c058e808
  kfree: c058e7e8
  kfree: c058e7c8
  kfree: c058e7a8
  kfree: c058e780
I am child 30
  kfree: c058e668
  kfree: c058e648
  kfree: c058e628
  kfree: c058e608
  kfree: c058e5e0
I am child 29
  kfree: c058e4c8
  kfree: c058e4a8
  kfree: c058e488
  kfree: c058e468
  kfree: c058e440
I am child 28
  kfree: c058e328
  kfree: c058e308
  kfree: c058e2e8
  kfree: c058e2c8
  kfree: c058e2a0
I am child 27
  kfree: c058e188
  kfree: c058e168
  kfree: c058e148
  kfree: c058e128
  kfree: c058e100
I am child 26
  kfree: c043af70
  kfree: c043af50
  kfree: c04e3fe8
  kfree: c04e3fc8
  kfree: c04e3fa0
I am child 25
  kfree: c04e3e88
  kfree: c04e3e68
  kfree: c04e3e48
  kfree: c04e3e28
  kfree: c04e3e00
I am child 24
  kfree: c04e3ce8
  kfree: c04e3cc8
  kfree: c04e3ca8
  kfree: c04e3c88
  kfree: c04e3c60
I am child 23
  kfree: c04e3b48
  kfree: c04e3b28
  kfree: c04e3b08
  kfree: c04e3ae8
  kfree: c04e3ac0
I am child 22
  kfree: c04e39a8
  kfree: c04e3988
  kfree: c04e3968
  kfree: c04e3948
  kfree: c04e3920
I am child 21
  kfree: c04e3808
  kfree: c04e37e8
  kfree: c04e37c8
  kfree: c04e37a8
  kfree: c04e3780
I am child 20
  kfree: c04e3668
  kfree: c04e3648
  kfree: c04e3628
  kfree: c04e3608
  kfree: c04e35e0
I am child 19
  kfree: c04e34c8
  kfree: c04e34a8
  kfree: c04e3488
  kfree: c04e3468
  kfree: c04e3440
I am child 18
  kfree: c04e3328
  kfree: c04e3308
  kfree: c04e32e8
  kfree: c04e32c8
  kfree: c04e32a0
I am child 17
  kfree: c04e3188
  kfree: c04e3168
  kfree: c04e3148
  kfree: c04e3128
  kfree: c04e3100
I am child 16
  kfree: c043af30
  kfree: c043af10
  kfree: c043aef0
  kfree: c043aed0
  kfree: c043aea8
I am child 15
  kfree: c043ad90
  kfree: c043ad70
  kfree: c043ad50
  kfree: c043ad30
  kfree: c043ad08
I am child 14
  kfree: c043abf0
  kfree: c043abd0
  kfree: c043abb0
  kfree: c043ab90
  kfree: c043ab68
I am child 13
  kfree: c043aa50
  kfree: c043aa30
  kfree: c043aa10
  kfree: c043a9f0
  kfree: c043a9c8
I am child 12
  kfree: c043a8b0
  kfree: c043a890
  kfree: c043a870
  kfree: c043a850
  kfree: c043a828
I am child 11
  kfree: c043a710
  kfree: c043a6f0
  kfree: c043a6d0
  kfree: c043a6b0
  kfree: c043a688
I am child 10
  kfree: c043a570
  kfree: c043a550
  kfree: c043a530
  kfree: c043a510
  kfree: c043a4e8
I am child 9
  kfree: c043a3d0
  kfree: c043a3b0
  kfree: c043a390
  kfree: c043a370
  kfree: c043a348
I am child 8
  kfree: c043a230
  kfree: c043a210
  kfree: c043a1f0
  kfree: c043a1d0
  kfree: c043a1a8
I am child 7
  kfree: c043a090
  kfree: c043a070
  kfree: c043a050
  kfree: c043a030
  kfree: c043a008
I am child 6
  kfree: c03a8ed8
  kfree: c03a8eb8
  kfree: c03a8e98
  kfree: c03a8e78
  kfree: c03a8e50
I am child 5
  kfree: c03a8d38
  kfree: c03a8d18
  kfree: c03a8cf8
  kfree: c03a8cd8
  kfree: c03a8cb0
I am child 4
  kfree: c03a8b98
  kfree: c03a8b78
  kfree: c03a8b58
  kfree: c03a8b38
  kfree: c03a8b10
I am child 3
  kfree: c03a89f8
  kfree: c03a89d8
  kfree: c03a89b8
  kfree: c03a8998
  kfree: c03a8970
I am child 2
  kfree: c03a8858
  kfree: c03a8838
  kfree: c03a8818
  kfree: c03a87f8
  kfree: c03a87d0
I am child 1
  kfree: c03a86b8
  kfree: c03a8698
  kfree: c03a8678
  kfree: c03a8658
  kfree: c03a8630
I am child 0
  kfree: c03a8518
  kfree: c03a84f8
  kfree: c03a84d8
  kfree: c03a84b8
  kfree: c03a8490
  kfree: c058e688
  kfree: c058e4e8
  kfree: c058e348
  kfree: c058e1a8
  kfree: c058e008
  kfree: c04e3ea8
  kfree: c04e3d08
  kfree: c04e3b68
  kfree: c04e39c8
  kfree: c04e3828
  kfree: c04e3688
  kfree: c04e34e8
  kfree: c04e3348
  kfree: c04e31a8
  kfree: c04e3008
  kfree: c043adb0
  kfree: c043ac10
  kfree: c043aa70
  kfree: c043a8d0
  kfree: c043a730
  kfree: c043a590
  kfree: c043a3f0
  kfree: c043a250
  kfree: c043a0b0
  kfree: c03a8ef8
  kfree: c03a8d58
  kfree: c03a8bb8
  kfree: c03a8a18
  kfree: c03a8878
  kfree: c03a86d8
  kfree: c03a8538
  kfree: c03a8398
forktest pass.
  kfree: c03a8318
  kfree: c03a8338
  kfree: c03a8358
  kfree: c03a8378
  kfree: c03a82f0
  kfree: c03a81f8
all user-mode processes have quit.
nr_free_pages_store=31822, nr_free_pages()=31819
kernel panic at kern/process/proc.c:839:
    assertion failed: nr_free_pages_store == nr_free_pages()
```

### 附录2 - 检查alloc_pages与free_pages配对

```
++ setup timer interrupts
================ nr_free_pages_store RECORDED ===================
    alloc_pages(2) => c01b252c
kernel_execve: pid = 2, name = "forktest".
    alloc_pages(1) => c01b2574
    alloc_pages(1) => c01b2598
    alloc_pages(1) => c01b25bc
    alloc_pages(1) => c01b25e0
    alloc_pages(1) => c01b2604
    alloc_pages(1) => c01b2628
    alloc_pages(1) => c01b264c
    alloc_pages(1) => c01b2670
    alloc_pages(1) => c01b2694
    alloc_pages(1) => c01b26b8
    alloc_pages(1) => c01b26dc
    alloc_pages(1) => c01b2700
    alloc_pages(1) => c01b2724
    alloc_pages(1) => c01b2748
    alloc_pages(1) => c01b276c
    alloc_pages(2) => c01b2790
    alloc_pages(1) => c01b27d8
    alloc_pages(1) => c01b27fc
    alloc_pages(1) => c01b2820
    alloc_pages(1) => c01b2844
    alloc_pages(1) => c01b2868
    alloc_pages(1) => c01b288c
    alloc_pages(1) => c01b28b0
    alloc_pages(1) => c01b28d4
    alloc_pages(1) => c01b28f8
    alloc_pages(1) => c01b291c
    alloc_pages(1) => c01b2940
    alloc_pages(1) => c01b2964
    alloc_pages(1) => c01b2988
    alloc_pages(1) => c01b29ac
    alloc_pages(1) => c01b29d0
    alloc_pages(2) => c01b29f4
    alloc_pages(1) => c01b2a3c
    alloc_pages(1) => c01b2a60
    alloc_pages(1) => c01b2a84
    alloc_pages(1) => c01b2aa8
    alloc_pages(1) => c01b2acc
    alloc_pages(1) => c01b2af0
    alloc_pages(1) => c01b2b14
    alloc_pages(1) => c01b2b38
    alloc_pages(1) => c01b2b5c
    alloc_pages(1) => c01b2b80
    alloc_pages(1) => c01b2ba4
    alloc_pages(1) => c01b2bc8
    alloc_pages(1) => c01b2bec
    alloc_pages(1) => c01b2c10
    alloc_pages(1) => c01b2c34
    alloc_pages(2) => c01b2c58
    alloc_pages(1) => c01b2ca0
    alloc_pages(1) => c01b2cc4
    alloc_pages(1) => c01b2ce8
    alloc_pages(1) => c01b2d0c
    alloc_pages(1) => c01b2d30
    alloc_pages(1) => c01b2d54
    alloc_pages(1) => c01b2d78
    alloc_pages(1) => c01b2d9c
    alloc_pages(1) => c01b2dc0
    alloc_pages(1) => c01b2de4
    alloc_pages(1) => c01b2e08
    alloc_pages(1) => c01b2e2c
    alloc_pages(1) => c01b2e50
    alloc_pages(1) => c01b2e74
    alloc_pages(1) => c01b2e98
    alloc_pages(2) => c01b2ebc
    alloc_pages(1) => c01b2f04
    alloc_pages(1) => c01b2f28
    alloc_pages(1) => c01b2f4c
    alloc_pages(1) => c01b2f70
    alloc_pages(1) => c01b2f94
    alloc_pages(1) => c01b2fb8
    alloc_pages(1) => c01b2fdc
    alloc_pages(1) => c01b3000
    alloc_pages(1) => c01b3024
    alloc_pages(1) => c01b3048
    alloc_pages(1) => c01b306c
    alloc_pages(1) => c01b3090
    alloc_pages(1) => c01b30b4
    alloc_pages(1) => c01b30d8
    alloc_pages(1) => c01b30fc
    alloc_pages(2) => c01b3120
    alloc_pages(1) => c01b3168
    alloc_pages(1) => c01b318c
    alloc_pages(1) => c01b31b0
    alloc_pages(1) => c01b31d4
    alloc_pages(1) => c01b31f8
    alloc_pages(1) => c01b321c
    alloc_pages(1) => c01b3240
    alloc_pages(1) => c01b3264
    alloc_pages(1) => c01b3288
    alloc_pages(1) => c01b32ac
    alloc_pages(1) => c01b32d0
    alloc_pages(1) => c01b32f4
    alloc_pages(1) => c01b3318
    alloc_pages(1) => c01b333c
    alloc_pages(1) => c01b3360
    alloc_pages(2) => c01b3384
    alloc_pages(1) => c01b33cc
    alloc_pages(1) => c01b33f0
    alloc_pages(1) => c01b3414
    alloc_pages(1) => c01b3438
    alloc_pages(1) => c01b345c
    alloc_pages(1) => c01b3480
    alloc_pages(1) => c01b34a4
    alloc_pages(1) => c01b34c8
    alloc_pages(1) => c01b34ec
    alloc_pages(1) => c01b3510
    alloc_pages(1) => c01b3534
    alloc_pages(1) => c01b3558
    alloc_pages(1) => c01b357c
    alloc_pages(1) => c01b35a0
    alloc_pages(1) => c01b35c4
    alloc_pages(2) => c01b35e8
    alloc_pages(1) => c01b3630
    alloc_pages(1) => c01b3654
    alloc_pages(1) => c01b3678
    alloc_pages(1) => c01b369c
    alloc_pages(1) => c01b36c0
    alloc_pages(1) => c01b36e4
    alloc_pages(1) => c01b3708
    alloc_pages(1) => c01b372c
    alloc_pages(1) => c01b3750
    alloc_pages(1) => c01b3774
    alloc_pages(1) => c01b3798
    alloc_pages(1) => c01b37bc
    alloc_pages(1) => c01b37e0
    alloc_pages(1) => c01b3804
    alloc_pages(1) => c01b3828
    alloc_pages(2) => c01b384c
    alloc_pages(1) => c01b3894
    alloc_pages(1) => c01b38b8
    alloc_pages(1) => c01b38dc
    alloc_pages(1) => c01b3900
    alloc_pages(1) => c01b3924
    alloc_pages(1) => c01b3948
    alloc_pages(1) => c01b396c
    alloc_pages(1) => c01b3990
    alloc_pages(1) => c01b39b4
    alloc_pages(1) => c01b39d8
    alloc_pages(1) => c01b39fc
    alloc_pages(1) => c01b3a20
    alloc_pages(1) => c01b3a44
    alloc_pages(1) => c01b3a68
    alloc_pages(1) => c01b3a8c
    alloc_pages(1) => c01b3ab0
    alloc_pages(2) => c01b3ad4
    alloc_pages(1) => c01b3b1c
    alloc_pages(1) => c01b3b40
    alloc_pages(1) => c01b3b64
    alloc_pages(1) => c01b3b88
    alloc_pages(1) => c01b3bac
    alloc_pages(1) => c01b3bd0
    alloc_pages(1) => c01b3bf4
    alloc_pages(1) => c01b3c18
    alloc_pages(1) => c01b3c3c
    alloc_pages(1) => c01b3c60
    alloc_pages(1) => c01b3c84
    alloc_pages(1) => c01b3ca8
    alloc_pages(1) => c01b3ccc
    alloc_pages(1) => c01b3cf0
    alloc_pages(1) => c01b3d14
    alloc_pages(2) => c01b3d38
    alloc_pages(1) => c01b3d80
    alloc_pages(1) => c01b3da4
    alloc_pages(1) => c01b3dc8
    alloc_pages(1) => c01b3dec
    alloc_pages(1) => c01b3e10
    alloc_pages(1) => c01b3e34
    alloc_pages(1) => c01b3e58
    alloc_pages(1) => c01b3e7c
    alloc_pages(1) => c01b3ea0
    alloc_pages(1) => c01b3ec4
    alloc_pages(1) => c01b3ee8
    alloc_pages(1) => c01b3f0c
    alloc_pages(1) => c01b3f30
    alloc_pages(1) => c01b3f54
    alloc_pages(1) => c01b3f78
    alloc_pages(2) => c01b3f9c
    alloc_pages(1) => c01b3fe4
    alloc_pages(1) => c01b4008
    alloc_pages(1) => c01b402c
    alloc_pages(1) => c01b4050
    alloc_pages(1) => c01b4074
    alloc_pages(1) => c01b4098
    alloc_pages(1) => c01b40bc
    alloc_pages(1) => c01b40e0
    alloc_pages(1) => c01b4104
    alloc_pages(1) => c01b4128
    alloc_pages(1) => c01b414c
    alloc_pages(1) => c01b4170
    alloc_pages(1) => c01b4194
    alloc_pages(1) => c01b41b8
    alloc_pages(1) => c01b41dc
    alloc_pages(2) => c01b4200
    alloc_pages(1) => c01b4248
    alloc_pages(1) => c01b426c
    alloc_pages(1) => c01b4290
    alloc_pages(1) => c01b42b4
    alloc_pages(1) => c01b42d8
    alloc_pages(1) => c01b42fc
    alloc_pages(1) => c01b4320
    alloc_pages(1) => c01b4344
    alloc_pages(1) => c01b4368
    alloc_pages(1) => c01b438c
    alloc_pages(1) => c01b43b0
    alloc_pages(1) => c01b43d4
    alloc_pages(1) => c01b43f8
    alloc_pages(1) => c01b441c
    alloc_pages(1) => c01b4440
    alloc_pages(2) => c01b4464
    alloc_pages(1) => c01b44ac
    alloc_pages(1) => c01b44d0
    alloc_pages(1) => c01b44f4
    alloc_pages(1) => c01b4518
    alloc_pages(1) => c01b453c
    alloc_pages(1) => c01b4560
    alloc_pages(1) => c01b4584
    alloc_pages(1) => c01b45a8
    alloc_pages(1) => c01b45cc
    alloc_pages(1) => c01b45f0
    alloc_pages(1) => c01b4614
    alloc_pages(1) => c01b4638
    alloc_pages(1) => c01b465c
    alloc_pages(1) => c01b4680
    alloc_pages(1) => c01b46a4
    alloc_pages(2) => c01b46c8
    alloc_pages(1) => c01b4710
    alloc_pages(1) => c01b4734
    alloc_pages(1) => c01b4758
    alloc_pages(1) => c01b477c
    alloc_pages(1) => c01b47a0
    alloc_pages(1) => c01b47c4
    alloc_pages(1) => c01b47e8
    alloc_pages(1) => c01b480c
    alloc_pages(1) => c01b4830
    alloc_pages(1) => c01b4854
    alloc_pages(1) => c01b4878
    alloc_pages(1) => c01b489c
    alloc_pages(1) => c01b48c0
    alloc_pages(1) => c01b48e4
    alloc_pages(1) => c01b4908
    alloc_pages(2) => c01b492c
    alloc_pages(1) => c01b4974
    alloc_pages(1) => c01b4998
    alloc_pages(1) => c01b49bc
    alloc_pages(1) => c01b49e0
    alloc_pages(1) => c01b4a04
    alloc_pages(1) => c01b4a28
    alloc_pages(1) => c01b4a4c
    alloc_pages(1) => c01b4a70
    alloc_pages(1) => c01b4a94
    alloc_pages(1) => c01b4ab8
    alloc_pages(1) => c01b4adc
    alloc_pages(1) => c01b4b00
    alloc_pages(1) => c01b4b24
    alloc_pages(1) => c01b4b48
    alloc_pages(1) => c01b4b6c
    alloc_pages(2) => c01b4b90
    alloc_pages(1) => c01b4bd8
    alloc_pages(1) => c01b4bfc
    alloc_pages(1) => c01b4c20
    alloc_pages(1) => c01b4c44
    alloc_pages(1) => c01b4c68
    alloc_pages(1) => c01b4c8c
    alloc_pages(1) => c01b4cb0
    alloc_pages(1) => c01b4cd4
    alloc_pages(1) => c01b4cf8
    alloc_pages(1) => c01b4d1c
    alloc_pages(1) => c01b4d40
    alloc_pages(1) => c01b4d64
    alloc_pages(1) => c01b4d88
    alloc_pages(1) => c01b4dac
    alloc_pages(1) => c01b4dd0
    alloc_pages(2) => c01b4df4
    alloc_pages(1) => c01b4e3c
    alloc_pages(1) => c01b4e60
    alloc_pages(1) => c01b4e84
    alloc_pages(1) => c01b4ea8
    alloc_pages(1) => c01b4ecc
    alloc_pages(1) => c01b4ef0
    alloc_pages(1) => c01b4f14
    alloc_pages(1) => c01b4f38
    alloc_pages(1) => c01b4f5c
    alloc_pages(1) => c01b4f80
    alloc_pages(1) => c01b4fa4
    alloc_pages(1) => c01b4fc8
    alloc_pages(1) => c01b4fec
    alloc_pages(1) => c01b5010
    alloc_pages(1) => c01b5034
    alloc_pages(1) => c01b5058
    alloc_pages(2) => c01b507c
    alloc_pages(1) => c01b50c4
    alloc_pages(1) => c01b50e8
    alloc_pages(1) => c01b510c
    alloc_pages(1) => c01b5130
    alloc_pages(1) => c01b5154
    alloc_pages(1) => c01b5178
    alloc_pages(1) => c01b519c
    alloc_pages(1) => c01b51c0
    alloc_pages(1) => c01b51e4
    alloc_pages(1) => c01b5208
    alloc_pages(1) => c01b522c
    alloc_pages(1) => c01b5250
    alloc_pages(1) => c01b5274
    alloc_pages(1) => c01b5298
    alloc_pages(1) => c01b52bc
    alloc_pages(2) => c01b52e0
    alloc_pages(1) => c01b5328
    alloc_pages(1) => c01b534c
    alloc_pages(1) => c01b5370
    alloc_pages(1) => c01b5394
    alloc_pages(1) => c01b53b8
    alloc_pages(1) => c01b53dc
    alloc_pages(1) => c01b5400
    alloc_pages(1) => c01b5424
    alloc_pages(1) => c01b5448
    alloc_pages(1) => c01b546c
    alloc_pages(1) => c01b5490
    alloc_pages(1) => c01b54b4
    alloc_pages(1) => c01b54d8
    alloc_pages(1) => c01b54fc
    alloc_pages(1) => c01b5520
    alloc_pages(2) => c01b5544
    alloc_pages(1) => c01b558c
    alloc_pages(1) => c01b55b0
    alloc_pages(1) => c01b55d4
    alloc_pages(1) => c01b55f8
    alloc_pages(1) => c01b561c
    alloc_pages(1) => c01b5640
    alloc_pages(1) => c01b5664
    alloc_pages(1) => c01b5688
    alloc_pages(1) => c01b56ac
    alloc_pages(1) => c01b56d0
    alloc_pages(1) => c01b56f4
    alloc_pages(1) => c01b5718
    alloc_pages(1) => c01b573c
    alloc_pages(1) => c01b5760
    alloc_pages(1) => c01b5784
    alloc_pages(2) => c01b57a8
    alloc_pages(1) => c01b57f0
    alloc_pages(1) => c01b5814
    alloc_pages(1) => c01b5838
    alloc_pages(1) => c01b585c
    alloc_pages(1) => c01b5880
    alloc_pages(1) => c01b58a4
    alloc_pages(1) => c01b58c8
    alloc_pages(1) => c01b58ec
    alloc_pages(1) => c01b5910
    alloc_pages(1) => c01b5934
    alloc_pages(1) => c01b5958
    alloc_pages(1) => c01b597c
    alloc_pages(1) => c01b59a0
    alloc_pages(1) => c01b59c4
    alloc_pages(1) => c01b59e8
    alloc_pages(2) => c01b5a0c
    alloc_pages(1) => c01b5a54
    alloc_pages(1) => c01b5a78
    alloc_pages(1) => c01b5a9c
    alloc_pages(1) => c01b5ac0
    alloc_pages(1) => c01b5ae4
    alloc_pages(1) => c01b5b08
    alloc_pages(1) => c01b5b2c
    alloc_pages(1) => c01b5b50
    alloc_pages(1) => c01b5b74
    alloc_pages(1) => c01b5b98
    alloc_pages(1) => c01b5bbc
    alloc_pages(1) => c01b5be0
    alloc_pages(1) => c01b5c04
    alloc_pages(1) => c01b5c28
    alloc_pages(1) => c01b5c4c
    alloc_pages(2) => c01b5c70
    alloc_pages(1) => c01b5cb8
    alloc_pages(1) => c01b5cdc
    alloc_pages(1) => c01b5d00
    alloc_pages(1) => c01b5d24
    alloc_pages(1) => c01b5d48
    alloc_pages(1) => c01b5d6c
    alloc_pages(1) => c01b5d90
    alloc_pages(1) => c01b5db4
    alloc_pages(1) => c01b5dd8
    alloc_pages(1) => c01b5dfc
    alloc_pages(1) => c01b5e20
    alloc_pages(1) => c01b5e44
    alloc_pages(1) => c01b5e68
    alloc_pages(1) => c01b5e8c
    alloc_pages(1) => c01b5eb0
    alloc_pages(2) => c01b5ed4
    alloc_pages(1) => c01b5f1c
    alloc_pages(1) => c01b5f40
    alloc_pages(1) => c01b5f64
    alloc_pages(1) => c01b5f88
    alloc_pages(1) => c01b5fac
    alloc_pages(1) => c01b5fd0
    alloc_pages(1) => c01b5ff4
    alloc_pages(1) => c01b6018
    alloc_pages(1) => c01b603c
    alloc_pages(1) => c01b6060
    alloc_pages(1) => c01b6084
    alloc_pages(1) => c01b60a8
    alloc_pages(1) => c01b60cc
    alloc_pages(1) => c01b60f0
    alloc_pages(1) => c01b6114
    alloc_pages(2) => c01b6138
    alloc_pages(1) => c01b6180
    alloc_pages(1) => c01b61a4
    alloc_pages(1) => c01b61c8
    alloc_pages(1) => c01b61ec
    alloc_pages(1) => c01b6210
    alloc_pages(1) => c01b6234
    alloc_pages(1) => c01b6258
    alloc_pages(1) => c01b627c
    alloc_pages(1) => c01b62a0
    alloc_pages(1) => c01b62c4
    alloc_pages(1) => c01b62e8
    alloc_pages(1) => c01b630c
    alloc_pages(1) => c01b6330
    alloc_pages(1) => c01b6354
    alloc_pages(1) => c01b6378
    alloc_pages(2) => c01b639c
    alloc_pages(1) => c01b63e4
    alloc_pages(1) => c01b6408
    alloc_pages(1) => c01b642c
    alloc_pages(1) => c01b6450
    alloc_pages(1) => c01b6474
    alloc_pages(1) => c01b6498
    alloc_pages(1) => c01b64bc
    alloc_pages(1) => c01b64e0
    alloc_pages(1) => c01b6504
    alloc_pages(1) => c01b6528
    alloc_pages(1) => c01b654c
    alloc_pages(1) => c01b6570
    alloc_pages(1) => c01b6594
    alloc_pages(1) => c01b65b8
    alloc_pages(1) => c01b65dc
    alloc_pages(2) => c01b6600
    alloc_pages(1) => c01b6648
    alloc_pages(1) => c01b666c
    alloc_pages(1) => c01b6690
    alloc_pages(1) => c01b66b4
    alloc_pages(1) => c01b66d8
    alloc_pages(1) => c01b66fc
    alloc_pages(1) => c01b6720
    alloc_pages(1) => c01b6744
    alloc_pages(1) => c01b6768
    alloc_pages(1) => c01b678c
    alloc_pages(1) => c01b67b0
    alloc_pages(1) => c01b67d4
    alloc_pages(1) => c01b67f8
    alloc_pages(1) => c01b681c
    alloc_pages(1) => c01b6840
    alloc_pages(1) => c01b6864
    alloc_pages(2) => c01b6888
    alloc_pages(1) => c01b68d0
    alloc_pages(1) => c01b68f4
    alloc_pages(1) => c01b6918
    alloc_pages(1) => c01b693c
    alloc_pages(1) => c01b6960
    alloc_pages(1) => c01b6984
    alloc_pages(1) => c01b69a8
    alloc_pages(1) => c01b69cc
    alloc_pages(1) => c01b69f0
    alloc_pages(1) => c01b6a14
    alloc_pages(1) => c01b6a38
    alloc_pages(1) => c01b6a5c
    alloc_pages(1) => c01b6a80
    alloc_pages(1) => c01b6aa4
    alloc_pages(1) => c01b6ac8
    alloc_pages(2) => c01b6aec
    alloc_pages(1) => c01b6b34
    alloc_pages(1) => c01b6b58
    alloc_pages(1) => c01b6b7c
    alloc_pages(1) => c01b6ba0
    alloc_pages(1) => c01b6bc4
    alloc_pages(1) => c01b6be8
    alloc_pages(1) => c01b6c0c
    alloc_pages(1) => c01b6c30
    alloc_pages(1) => c01b6c54
    alloc_pages(1) => c01b6c78
    alloc_pages(1) => c01b6c9c
    alloc_pages(1) => c01b6cc0
    alloc_pages(1) => c01b6ce4
    alloc_pages(1) => c01b6d08
    alloc_pages(1) => c01b6d2c
    alloc_pages(2) => c01b6d50
    alloc_pages(1) => c01b6d98
    alloc_pages(1) => c01b6dbc
    alloc_pages(1) => c01b6de0
    alloc_pages(1) => c01b6e04
    alloc_pages(1) => c01b6e28
    alloc_pages(1) => c01b6e4c
    alloc_pages(1) => c01b6e70
    alloc_pages(1) => c01b6e94
    alloc_pages(1) => c01b6eb8
    alloc_pages(1) => c01b6edc
    alloc_pages(1) => c01b6f00
    alloc_pages(1) => c01b6f24
    alloc_pages(1) => c01b6f48
    alloc_pages(1) => c01b6f6c
    alloc_pages(1) => c01b6f90
    alloc_pages(2) => c01b6fb4
    alloc_pages(1) => c01b6ffc
    alloc_pages(1) => c01b7020
    alloc_pages(1) => c01b7044
    alloc_pages(1) => c01b7068
    alloc_pages(1) => c01b708c
    alloc_pages(1) => c01b70b0
    alloc_pages(1) => c01b70d4
    alloc_pages(1) => c01b70f8
    alloc_pages(1) => c01b711c
    alloc_pages(1) => c01b7140
    alloc_pages(1) => c01b7164
    alloc_pages(1) => c01b7188
    alloc_pages(1) => c01b71ac
    alloc_pages(1) => c01b71d0
    alloc_pages(1) => c01b71f4
    alloc_pages(2) => c01b7218
    alloc_pages(1) => c01b7260
    alloc_pages(1) => c01b7284
    alloc_pages(1) => c01b72a8
    alloc_pages(1) => c01b72cc
    alloc_pages(1) => c01b72f0
    alloc_pages(1) => c01b7314
    alloc_pages(1) => c01b7338
    alloc_pages(1) => c01b735c
    alloc_pages(1) => c01b7380
    alloc_pages(1) => c01b73a4
    alloc_pages(1) => c01b73c8
    alloc_pages(1) => c01b73ec
    alloc_pages(1) => c01b7410
    alloc_pages(1) => c01b7434
    alloc_pages(1) => c01b7458
I am child 31
    free_pages(1) => c01b73ec
    free_pages(1) => c01b7410
    free_pages(1) => c01b7434
    free_pages(1) => c01b7458
    free_pages(1) => c01b7380
    free_pages(1) => c01b73a4
    free_pages(1) => c01b735c
    free_pages(1) => c01b72a8
    free_pages(1) => c01b72cc
    free_pages(1) => c01b72f0
    free_pages(1) => c01b7314
    free_pages(1) => c01b73c8
    free_pages(1) => c01b7338
    free_pages(1) => c01b7284
    free_pages(1) => c01b7260
I am child 30
    free_pages(1) => c01b7188
    free_pages(1) => c01b71ac
    free_pages(1) => c01b71d0
    free_pages(1) => c01b71f4
    free_pages(1) => c01b711c
    free_pages(1) => c01b7140
    free_pages(1) => c01b70f8
    free_pages(1) => c01b7044
    free_pages(1) => c01b7068
    free_pages(1) => c01b708c
    free_pages(1) => c01b70b0
    free_pages(1) => c01b7164
    free_pages(1) => c01b70d4
    free_pages(1) => c01b7020
    free_pages(1) => c01b6ffc
I am child 29
    free_pages(1) => c01b6f24
    free_pages(1) => c01b6f48
    free_pages(1) => c01b6f6c
    free_pages(1) => c01b6f90
    free_pages(1) => c01b6eb8
    free_pages(1) => c01b6edc
    free_pages(1) => c01b6e94
    free_pages(1) => c01b6de0
    free_pages(1) => c01b6e04
    free_pages(1) => c01b6e28
    free_pages(1) => c01b6e4c
    free_pages(1) => c01b6f00
    free_pages(1) => c01b6e70
    free_pages(1) => c01b6dbc
    free_pages(1) => c01b6d98
I am child 28
    free_pages(1) => c01b6cc0
    free_pages(1) => c01b6ce4
    free_pages(1) => c01b6d08
    free_pages(1) => c01b6d2c
    free_pages(1) => c01b6c54
    free_pages(1) => c01b6c78
    free_pages(1) => c01b6c30
    free_pages(1) => c01b6b7c
    free_pages(1) => c01b6ba0
    free_pages(1) => c01b6bc4
    free_pages(1) => c01b6be8
    free_pages(1) => c01b6c9c
    free_pages(1) => c01b6c0c
    free_pages(1) => c01b6b58
    free_pages(1) => c01b6b34
I am child 27
    free_pages(1) => c01b6a5c
    free_pages(1) => c01b6a80
    free_pages(1) => c01b6aa4
    free_pages(1) => c01b6ac8
    free_pages(1) => c01b69f0
    free_pages(1) => c01b6a14
    free_pages(1) => c01b69cc
    free_pages(1) => c01b6918
    free_pages(1) => c01b693c
    free_pages(1) => c01b6960
    free_pages(1) => c01b6984
    free_pages(1) => c01b6a38
    free_pages(1) => c01b69a8
    free_pages(1) => c01b68f4
    free_pages(1) => c01b68d0
I am child 26
    free_pages(1) => c01b67d4
    free_pages(1) => c01b67f8
    free_pages(1) => c01b681c
    free_pages(1) => c01b6840
    free_pages(1) => c01b6768
    free_pages(1) => c01b678c
    free_pages(1) => c01b6744
    free_pages(1) => c01b6690
    free_pages(1) => c01b66b4
    free_pages(1) => c01b66d8
    free_pages(1) => c01b66fc
    free_pages(1) => c01b67b0
    free_pages(1) => c01b6720
    free_pages(1) => c01b666c
    free_pages(1) => c01b6648
I am child 25
    free_pages(1) => c01b6570
    free_pages(1) => c01b6594
    free_pages(1) => c01b65b8
    free_pages(1) => c01b65dc
    free_pages(1) => c01b6504
    free_pages(1) => c01b6528
    free_pages(1) => c01b64e0
    free_pages(1) => c01b642c
    free_pages(1) => c01b6450
    free_pages(1) => c01b6474
    free_pages(1) => c01b6498
    free_pages(1) => c01b654c
    free_pages(1) => c01b64bc
    free_pages(1) => c01b6408
    free_pages(1) => c01b63e4
I am child 24
    free_pages(1) => c01b630c
    free_pages(1) => c01b6330
    free_pages(1) => c01b6354
    free_pages(1) => c01b6378
    free_pages(1) => c01b62a0
    free_pages(1) => c01b62c4
    free_pages(1) => c01b627c
    free_pages(1) => c01b61c8
    free_pages(1) => c01b61ec
    free_pages(1) => c01b6210
    free_pages(1) => c01b6234
    free_pages(1) => c01b62e8
    free_pages(1) => c01b6258
    free_pages(1) => c01b61a4
    free_pages(1) => c01b6180
I am child 23
    free_pages(1) => c01b60a8
    free_pages(1) => c01b60cc
    free_pages(1) => c01b60f0
    free_pages(1) => c01b6114
    free_pages(1) => c01b603c
    free_pages(1) => c01b6060
    free_pages(1) => c01b6018
    free_pages(1) => c01b5f64
    free_pages(1) => c01b5f88
    free_pages(1) => c01b5fac
    free_pages(1) => c01b5fd0
    free_pages(1) => c01b6084
    free_pages(1) => c01b5ff4
    free_pages(1) => c01b5f40
    free_pages(1) => c01b5f1c
I am child 22
    free_pages(1) => c01b5e44
    free_pages(1) => c01b5e68
    free_pages(1) => c01b5e8c
    free_pages(1) => c01b5eb0
    free_pages(1) => c01b5dd8
    free_pages(1) => c01b5dfc
    free_pages(1) => c01b5db4
    free_pages(1) => c01b5d00
    free_pages(1) => c01b5d24
    free_pages(1) => c01b5d48
    free_pages(1) => c01b5d6c
    free_pages(1) => c01b5e20
    free_pages(1) => c01b5d90
    free_pages(1) => c01b5cdc
    free_pages(1) => c01b5cb8
I am child 21
    free_pages(1) => c01b5be0
    free_pages(1) => c01b5c04
    free_pages(1) => c01b5c28
    free_pages(1) => c01b5c4c
    free_pages(1) => c01b5b74
    free_pages(1) => c01b5b98
    free_pages(1) => c01b5b50
    free_pages(1) => c01b5a9c
    free_pages(1) => c01b5ac0
    free_pages(1) => c01b5ae4
    free_pages(1) => c01b5b08
    free_pages(1) => c01b5bbc
    free_pages(1) => c01b5b2c
    free_pages(1) => c01b5a78
    free_pages(1) => c01b5a54
I am child 20
    free_pages(1) => c01b597c
    free_pages(1) => c01b59a0
    free_pages(1) => c01b59c4
    free_pages(1) => c01b59e8
    free_pages(1) => c01b5910
    free_pages(1) => c01b5934
    free_pages(1) => c01b58ec
    free_pages(1) => c01b5838
    free_pages(1) => c01b585c
    free_pages(1) => c01b5880
    free_pages(1) => c01b58a4
    free_pages(1) => c01b5958
    free_pages(1) => c01b58c8
    free_pages(1) => c01b5814
    free_pages(1) => c01b57f0
I am child 19
    free_pages(1) => c01b5718
    free_pages(1) => c01b573c
    free_pages(1) => c01b5760
    free_pages(1) => c01b5784
    free_pages(1) => c01b56ac
    free_pages(1) => c01b56d0
    free_pages(1) => c01b5688
    free_pages(1) => c01b55d4
    free_pages(1) => c01b55f8
    free_pages(1) => c01b561c
    free_pages(1) => c01b5640
    free_pages(1) => c01b56f4
    free_pages(1) => c01b5664
    free_pages(1) => c01b55b0
    free_pages(1) => c01b558c
I am child 18
    free_pages(1) => c01b54b4
    free_pages(1) => c01b54d8
    free_pages(1) => c01b54fc
    free_pages(1) => c01b5520
    free_pages(1) => c01b5448
    free_pages(1) => c01b546c
    free_pages(1) => c01b5424
    free_pages(1) => c01b5370
    free_pages(1) => c01b5394
    free_pages(1) => c01b53b8
    free_pages(1) => c01b53dc
    free_pages(1) => c01b5490
    free_pages(1) => c01b5400
    free_pages(1) => c01b534c
    free_pages(1) => c01b5328
I am child 17
    free_pages(1) => c01b5250
    free_pages(1) => c01b5274
    free_pages(1) => c01b5298
    free_pages(1) => c01b52bc
    free_pages(1) => c01b51e4
    free_pages(1) => c01b5208
    free_pages(1) => c01b51c0
    free_pages(1) => c01b510c
    free_pages(1) => c01b5130
    free_pages(1) => c01b5154
    free_pages(1) => c01b5178
    free_pages(1) => c01b522c
    free_pages(1) => c01b519c
    free_pages(1) => c01b50e8
    free_pages(1) => c01b50c4
I am child 16
    free_pages(1) => c01b4fc8
    free_pages(1) => c01b4fec
    free_pages(1) => c01b5010
    free_pages(1) => c01b5034
    free_pages(1) => c01b4f5c
    free_pages(1) => c01b4f80
    free_pages(1) => c01b4f38
    free_pages(1) => c01b4e84
    free_pages(1) => c01b4ea8
    free_pages(1) => c01b4ecc
    free_pages(1) => c01b4ef0
    free_pages(1) => c01b4fa4
    free_pages(1) => c01b4f14
    free_pages(1) => c01b4e60
    free_pages(1) => c01b4e3c
I am child 15
    free_pages(1) => c01b4d64
    free_pages(1) => c01b4d88
    free_pages(1) => c01b4dac
    free_pages(1) => c01b4dd0
    free_pages(1) => c01b4cf8
    free_pages(1) => c01b4d1c
    free_pages(1) => c01b4cd4
    free_pages(1) => c01b4c20
    free_pages(1) => c01b4c44
    free_pages(1) => c01b4c68
    free_pages(1) => c01b4c8c
    free_pages(1) => c01b4d40
    free_pages(1) => c01b4cb0
    free_pages(1) => c01b4bfc
    free_pages(1) => c01b4bd8
I am child 14
    free_pages(1) => c01b4b00
    free_pages(1) => c01b4b24
    free_pages(1) => c01b4b48
    free_pages(1) => c01b4b6c
    free_pages(1) => c01b4a94
    free_pages(1) => c01b4ab8
    free_pages(1) => c01b4a70
    free_pages(1) => c01b49bc
    free_pages(1) => c01b49e0
    free_pages(1) => c01b4a04
    free_pages(1) => c01b4a28
    free_pages(1) => c01b4adc
    free_pages(1) => c01b4a4c
    free_pages(1) => c01b4998
    free_pages(1) => c01b4974
I am child 13
    free_pages(1) => c01b489c
    free_pages(1) => c01b48c0
    free_pages(1) => c01b48e4
    free_pages(1) => c01b4908
    free_pages(1) => c01b4830
    free_pages(1) => c01b4854
    free_pages(1) => c01b480c
    free_pages(1) => c01b4758
    free_pages(1) => c01b477c
    free_pages(1) => c01b47a0
    free_pages(1) => c01b47c4
    free_pages(1) => c01b4878
    free_pages(1) => c01b47e8
    free_pages(1) => c01b4734
    free_pages(1) => c01b4710
I am child 12
    free_pages(1) => c01b4638
    free_pages(1) => c01b465c
    free_pages(1) => c01b4680
    free_pages(1) => c01b46a4
    free_pages(1) => c01b45cc
    free_pages(1) => c01b45f0
    free_pages(1) => c01b45a8
    free_pages(1) => c01b44f4
    free_pages(1) => c01b4518
    free_pages(1) => c01b453c
    free_pages(1) => c01b4560
    free_pages(1) => c01b4614
    free_pages(1) => c01b4584
    free_pages(1) => c01b44d0
    free_pages(1) => c01b44ac
I am child 11
    free_pages(1) => c01b43d4
    free_pages(1) => c01b43f8
    free_pages(1) => c01b441c
    free_pages(1) => c01b4440
    free_pages(1) => c01b4368
    free_pages(1) => c01b438c
    free_pages(1) => c01b4344
    free_pages(1) => c01b4290
    free_pages(1) => c01b42b4
    free_pages(1) => c01b42d8
    free_pages(1) => c01b42fc
    free_pages(1) => c01b43b0
    free_pages(1) => c01b4320
    free_pages(1) => c01b426c
    free_pages(1) => c01b4248
I am child 10
    free_pages(1) => c01b4170
    free_pages(1) => c01b4194
    free_pages(1) => c01b41b8
    free_pages(1) => c01b41dc
    free_pages(1) => c01b4104
    free_pages(1) => c01b4128
    free_pages(1) => c01b40e0
    free_pages(1) => c01b402c
    free_pages(1) => c01b4050
    free_pages(1) => c01b4074
    free_pages(1) => c01b4098
    free_pages(1) => c01b414c
    free_pages(1) => c01b40bc
    free_pages(1) => c01b4008
    free_pages(1) => c01b3fe4
I am child 9
    free_pages(1) => c01b3f0c
    free_pages(1) => c01b3f30
    free_pages(1) => c01b3f54
    free_pages(1) => c01b3f78
    free_pages(1) => c01b3ea0
    free_pages(1) => c01b3ec4
    free_pages(1) => c01b3e7c
    free_pages(1) => c01b3dc8
    free_pages(1) => c01b3dec
    free_pages(1) => c01b3e10
    free_pages(1) => c01b3e34
    free_pages(1) => c01b3ee8
    free_pages(1) => c01b3e58
    free_pages(1) => c01b3da4
    free_pages(1) => c01b3d80
I am child 8
    free_pages(1) => c01b3ca8
    free_pages(1) => c01b3ccc
    free_pages(1) => c01b3cf0
    free_pages(1) => c01b3d14
    free_pages(1) => c01b3c3c
    free_pages(1) => c01b3c60
    free_pages(1) => c01b3c18
    free_pages(1) => c01b3b64
    free_pages(1) => c01b3b88
    free_pages(1) => c01b3bac
    free_pages(1) => c01b3bd0
    free_pages(1) => c01b3c84
    free_pages(1) => c01b3bf4
    free_pages(1) => c01b3b40
    free_pages(1) => c01b3b1c
I am child 7
    free_pages(1) => c01b3a44
    free_pages(1) => c01b3a68
    free_pages(1) => c01b3a8c
    free_pages(1) => c01b3ab0
    free_pages(1) => c01b39d8
    free_pages(1) => c01b39fc
    free_pages(1) => c01b39b4
    free_pages(1) => c01b3900
    free_pages(1) => c01b3924
    free_pages(1) => c01b3948
    free_pages(1) => c01b396c
    free_pages(1) => c01b3a20
    free_pages(1) => c01b3990
    free_pages(1) => c01b38dc
    free_pages(1) => c01b38b8
I am child 6
    free_pages(1) => c01b37bc
    free_pages(1) => c01b37e0
    free_pages(1) => c01b3804
    free_pages(1) => c01b3828
    free_pages(1) => c01b3750
    free_pages(1) => c01b3774
    free_pages(1) => c01b372c
    free_pages(1) => c01b3678
    free_pages(1) => c01b369c
    free_pages(1) => c01b36c0
    free_pages(1) => c01b36e4
    free_pages(1) => c01b3798
    free_pages(1) => c01b3708
    free_pages(1) => c01b3654
    free_pages(1) => c01b3630
I am child 5
    free_pages(1) => c01b3558
    free_pages(1) => c01b357c
    free_pages(1) => c01b35a0
    free_pages(1) => c01b35c4
    free_pages(1) => c01b34ec
    free_pages(1) => c01b3510
    free_pages(1) => c01b34c8
    free_pages(1) => c01b3414
    free_pages(1) => c01b3438
    free_pages(1) => c01b345c
    free_pages(1) => c01b3480
    free_pages(1) => c01b3534
    free_pages(1) => c01b34a4
    free_pages(1) => c01b33f0
    free_pages(1) => c01b33cc
I am child 4
    free_pages(1) => c01b32f4
    free_pages(1) => c01b3318
    free_pages(1) => c01b333c
    free_pages(1) => c01b3360
    free_pages(1) => c01b3288
    free_pages(1) => c01b32ac
    free_pages(1) => c01b3264
    free_pages(1) => c01b31b0
    free_pages(1) => c01b31d4
    free_pages(1) => c01b31f8
    free_pages(1) => c01b321c
    free_pages(1) => c01b32d0
    free_pages(1) => c01b3240
    free_pages(1) => c01b318c
    free_pages(1) => c01b3168
I am child 3
    free_pages(1) => c01b3090
    free_pages(1) => c01b30b4
    free_pages(1) => c01b30d8
    free_pages(1) => c01b30fc
    free_pages(1) => c01b3024
    free_pages(1) => c01b3048
    free_pages(1) => c01b3000
    free_pages(1) => c01b2f4c
    free_pages(1) => c01b2f70
    free_pages(1) => c01b2f94
    free_pages(1) => c01b2fb8
    free_pages(1) => c01b306c
    free_pages(1) => c01b2fdc
    free_pages(1) => c01b2f28
    free_pages(1) => c01b2f04
I am child 2
    free_pages(1) => c01b2e2c
    free_pages(1) => c01b2e50
    free_pages(1) => c01b2e74
    free_pages(1) => c01b2e98
    free_pages(1) => c01b2dc0
    free_pages(1) => c01b2de4
    free_pages(1) => c01b2d9c
    free_pages(1) => c01b2ce8
    free_pages(1) => c01b2d0c
    free_pages(1) => c01b2d30
    free_pages(1) => c01b2d54
    free_pages(1) => c01b2e08
    free_pages(1) => c01b2d78
    free_pages(1) => c01b2cc4
    free_pages(1) => c01b2ca0
I am child 1
    free_pages(1) => c01b2bc8
    free_pages(1) => c01b2bec
    free_pages(1) => c01b2c10
    free_pages(1) => c01b2c34
    free_pages(1) => c01b2b5c
    free_pages(1) => c01b2b80
    free_pages(1) => c01b2b38
    free_pages(1) => c01b2a84
    free_pages(1) => c01b2aa8
    free_pages(1) => c01b2acc
    free_pages(1) => c01b2af0
    free_pages(1) => c01b2ba4
    free_pages(1) => c01b2b14
    free_pages(1) => c01b2a60
    free_pages(1) => c01b2a3c
I am child 0
    free_pages(1) => c01b2964
    free_pages(1) => c01b2988
    free_pages(1) => c01b29ac
    free_pages(1) => c01b29d0
    free_pages(1) => c01b28f8
    free_pages(1) => c01b291c
    free_pages(1) => c01b28d4
    free_pages(1) => c01b2820
    free_pages(1) => c01b2844
    free_pages(1) => c01b2868
    free_pages(1) => c01b288c
    free_pages(1) => c01b2940
    free_pages(1) => c01b28b0
    free_pages(1) => c01b27fc
    free_pages(1) => c01b27d8
    free_pages(2) => c01b7218
    free_pages(2) => c01b6fb4
    free_pages(2) => c01b6d50
    free_pages(2) => c01b6aec
    free_pages(2) => c01b6888
    free_pages(2) => c01b6600
    free_pages(2) => c01b639c
    free_pages(2) => c01b6138
    free_pages(2) => c01b5ed4
    free_pages(2) => c01b5c70
    free_pages(2) => c01b5a0c
    free_pages(2) => c01b57a8
    free_pages(2) => c01b5544
    free_pages(2) => c01b52e0
    free_pages(2) => c01b507c
    free_pages(2) => c01b4df4
    free_pages(2) => c01b4b90
    free_pages(2) => c01b492c
    free_pages(2) => c01b46c8
    free_pages(2) => c01b4464
    free_pages(2) => c01b4200
    free_pages(2) => c01b3f9c
    free_pages(2) => c01b3d38
    free_pages(2) => c01b3ad4
    free_pages(2) => c01b384c
    free_pages(2) => c01b35e8
    free_pages(2) => c01b3384
    free_pages(2) => c01b3120
    free_pages(2) => c01b2ebc
    free_pages(2) => c01b2c58
    free_pages(2) => c01b29f4
    free_pages(2) => c01b2790
forktest pass.
    free_pages(1) => c01b2598
    free_pages(1) => c01b25e0
    free_pages(1) => c01b2604
    free_pages(1) => c01b2628
    free_pages(1) => c01b264c
    free_pages(1) => c01b2694
    free_pages(1) => c01b26b8
    free_pages(1) => c01b276c
    free_pages(1) => c01b2748
    free_pages(1) => c01b2724
    free_pages(1) => c01b26dc
    free_pages(1) => c01b25bc
    free_pages(1) => c01b2670
    free_pages(1) => c01b2700
    free_pages(1) => c01b2574
    free_pages(2) => c01b252c
all user-mode processes have quit.
nr_free_pages_store=31819, nr_free_pages()=31816
kernel panic at kern/process/proc.c:839:
    assertion failed: nr_free_pages_store == nr_free_pages()
```

### 附录3 - 未回收内存分配时的栈帧

  ```
ebp:0xc03b4cc8 eip:0xc0101f62 args:0xc010e0f1 0x00000001 0xc01b3894 0xc03b4d3c 
    kern/debug/kdebug.c:350: print_stackframe+21
ebp:0xc03b4d38 eip:0xc0104cc8 args:0x00000001 0x00000000 0x00000000 0xffffffff 
    kern/mm/default_pmm.c:120: default_alloc_pages+380
ebp:0xc03b4d68 eip:0xc010666e args:0x00000001 0xc03b4d8c 0xc03b4d98 0xc0105b09 
    kern/mm/pmm.c:163: alloc_pages+36
ebp:0xc03b4d98 eip:0xc0105c63 args:0x00000000 0x00000000 0xc03b4dc8 0xc0105b09 
    kern/mm/kmalloc.c:83: __slob_get_free_pages+27
ebp:0xc03b4dd8 eip:0xc0105e40 args:0x00000028 0x00000000 0x00000000 0xc01b3850 
    kern/mm/kmalloc.c:142: slob_alloc+395
ebp:0xc03b4e08 eip:0xc010604b args:0x00000020 0x00000000 0xc01b3880 0xc01b3870 
    kern/mm/kmalloc.c:230: __kmalloc+43
ebp:0xc03b4e38 eip:0xc0106127 args:0x00000020 0x00000000 0xc03b4e68 0xc010c57e 
    kern/mm/kmalloc.c:262: kmalloc+24
ebp:0xc03b4e68 eip:0xc0109456 args:0xc01b384c 0x00000000 0x0000007f 0xc03abc80 
    kern/mm/vmm.c:45: mm_create+17
ebp:0xc03b4e98 eip:0xc010b20f args:0x00000000 0xc03abef8 0xc03b4fb4 0x037a08cc 
    kern/process/proc.c:323: copy_mm+64
ebp:0xc03b4ec8 eip:0xc010b45f args:0x00000000 0xafffff30 0xc03b4fb4 0x00000000 
    kern/process/proc.c:416: do_fork+156
ebp:0xc03b4ef8 eip:0xc010c73f args:0xc03b4f24 0x0000000a 0x00000000 0x00000000 
    kern/syscall/syscall.c:19: sys_fork+51
ebp:0xc03b4f48 eip:0xc010c896 args:0x00000000 0x00000000 0x00000000 0x00000000 
    kern/syscall/syscall.c:93: syscall+115
ebp:0xc03b4f78 eip:0xc0103d39 args:0xc03b4fb4 0x00000000 0x00800020 0x0000001b 
    kern/trap/trap.c:216: trap_dispatch+299
ebp:0xc03b4fa8 eip:0xc0103eae args:0xc03b4fb4 0x00000000 0xafffffa8 0xafffff5c 
    kern/trap/trap.c:287: trap+74
ebp:0xafffff5c eip:0xc0103f03 args:0x00000002 0xafffff78 0x008002d0 0x00000000 
    kern/trap/trapentry.S:24: <unknown>+0
ebp:0xafffff68 eip:0x00800210 args:0x00000000 0x00000000 0xafffffa8 0x00800fa9 
    user/libs/syscall.c:40: sys_fork+17
ebp:0xafffff78 eip:0x008002d0 args:0x00000000 0x00000000 0x00000000 0x00000000 
    user/libs/ulib.c:15: fork+10
ebp:0xafffffa8 eip:0x00800fa9 args:0x00000000 0x00000000 0x00000000 0x00000000 
    user/forktest.c:10: main+23
ebp:0xafffffd8 eip:0x0080034d args:0x00000000 0x00000000 0x00000000 0x00000000 
    user/libs/umain.c:7: umain+10
  ```
