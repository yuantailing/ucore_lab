# Lab2

## [练习0] 填写已有实验

> 已完成

## [练习1] 实现 first-fit 连续物理内存分配算法

[练习1.1] 你的first fit算法是否有进一步的改进空间

> 有。比如：可以适当建立索引，用于分配和释放时加速定位。default_free_pages函数可以把两次循环合并到一起，但要考虑更多情况。

## [练习2] 实现寻找虚拟地址对应的页表项

[练习2.1] 请描述页目录项（Pag Director Entry）和页表（Page Table Entry）中每个组成部分的含义和以及对ucore而言的潜在用处。

> PDE中有对应的二级页表基地址，以及三个权限位：是否有效、可写、可被用户访问；PTE中有对应页的物理基地址，以及两个权限位：是否有效、可写。 这些权限位用于判断页面是否存在，用来管理内存以及保护数据。

> 各位含义及说明的代码在mmu.h里：

```
/* page table/directory entry flags */
#define PTE_P           0x001                   // Present
#define PTE_W           0x002                   // Writeable
#define PTE_U           0x004                   // User
#define PTE_PWT         0x008                   // Write-Through
#define PTE_PCD         0x010                   // Cache-Disable
#define PTE_A           0x020                   // Accessed
#define PTE_D           0x040                   // Dirty
#define PTE_PS          0x080                   // Page Size
#define PTE_MBZ         0x180                   // Bits must be zero
#define PTE_AVAIL       0xE00                   // Available for software use
                                                // The PTE_AVAIL bits aren't used by the kernel or interpreted by the
                                                // hardware, so user processes are allowed to set them arbitrarily.
```

[练习2.2] 如果ucore执行过程中访问内存，出现了页访问异常，请问硬件要做哪些事情？

> 硬件需要触发异常，从GDT中找到页访问异常服务程序的入口，然后交给操作系统软件处理。

## [练习3] 释放某虚地址所在的页并取消对应二级页表项的映射

[练习3.1] 数据结构Page的全局变量（其实是一个数组）的每一项与页表中的页目录项和页表项有无对应关系？如果有，其对应关系是啥？

> 有对应关系，页表是从end开始的连续一段内存存放的。

[练习3.2] 如果希望虚拟地址与物理地址相等，则需要如何修改lab2，完成此事？

> 页分配算法中，alloc_page加入一个参数，是虚拟地址（即希望得到的物理地址）。在所有可分配的页中，找物理地址等于虚拟地址的页。

> 这并不总是可以做到，alloc_page只能尽力而为，如果没有这样的页，或者这样的页已经被分配出去了，那么就不可能做到虚拟地址与物理地址相等。

## 与参考答案的区别：

> 1. 参考答案的first-fit算法效率很低，一块100页的空间，它在链表里有100项，而我只需要存一项，在分配和释放时再做处理，可以极大加快查找时间。
> 2. 我做了代码复用，移除了`default_init_memmap`函数，因为它的功能可以由`default_free_pages`代替，而且支持init多块的情况。移除一个函数还能减小操作系统内核大小。

## 重要的知识点

> first-fit算法。对应知识点：最先匹配算法。含义：分配页时，大于所需大小的连续可分配页中，地址最小的被匹配。关系：是同一个算法。差异：名称不同。
