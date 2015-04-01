# Lab1

## [练习1] 理解通过make生成执行文件的过程

[练习1.1] 操作系统镜像文件ucore.img是如何一步一步生成的？(需要比较详细地解释Makefile中每一条相关命令和命令参数的含义，以及说明命令导致的结果)

````
约定本文中的组织格式
A
| B
| > B
|   | D
| C
意思是生成A需要依赖B，执行C；生成B又需要执行D。

ucore.img
| $(UCOREIMG): $(kernel) $(bootblock)
| 
| > $(kernel)
|   | $(kernel): tools/kernel.ld
|   | $(kernel): $(KOBJS)
|   |
|   | > $(KOBJS)
|   |   | $(call add_files_cc,$(call listf_cc,$(KSRCDIR)),kernel,$(KCFLAGS))
|   |   |   | 这里生成.o文件，例如生成init.o的实际命令是gcc -Ikern/init/ -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/ -c kern/init/init.c -o obj/kern/init/init.o。生成其它.o文件的过程类似，不再赘述
|   |   |   | gcc命令的参数：
|   |   |   |   | -ggdb                生成可供gdb使用的调试信息。
|   |   |   |   | -m32                 生成适用于32位环境的代码。
|   |   |   |   | -gstabs              生成stabs格式的调试信息。
|   |   |   |   | -nostdinc            不使用标准库。
|   |   |   |   | -fno-stack-protector 不生成用于检测缓冲区溢出的代码。
|   |   |   |   | -Os                  为减小代码大小而进行优化。
|   |   |   |   | -I<dir>              添加搜索头文件的路径
|   | 
|   | @echo + ld $@
|   | $(V)$(LD) $(LDFLAGS) -T tools/kernel.ld -o $@ $(KOBJS)
|   |   | 实际执行的命令是ld -m    elf_i386 -nostdlib -T tools/kernel.ld -o bin/kernel  obj/kern/init/init.o obj/kern/libs/readline.o obj/kern/libs/stdio.o obj/kern/debug/kdebug.o obj/kern/debug/kmonitor.o obj/kern/debug/panic.o obj/kern/driver/clock.o obj/kern/driver/console.o obj/kern/driver/intr.o obj/kern/driver/picirq.o obj/kern/trap/trap.o obj/kern/trap/trapentry.o obj/kern/trap/vectors.o obj/kern/mm/pmm.o  obj/libs/printfmt.o obj/libs/string.o
|   |   | ld 的参数
|   |   |   | -m <emulation>  模拟为i386上的连接器
|   |   |   | -nostdlib       不使用标准库
|   |   |   | -N              设置代码段和数据段均可读写
|   |   |   | -e <entry>      指定入口
|   |   |   | -Ttext          指定代码段开始位置
|   |   |   | -T <scriptfile> 让连接器使用指定的脚本
|   | @$(OBJDUMP) -S $@ > $(call asmfile,kernel)
|   | | makefile的几条指令中有@前缀的都不必需
|   | @$(OBJDUMP) -t $@ | $(SED) '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(call symfile,kernel)
| 
| > $(bootblock)
|   | (bootblock): $(call toobj,$(bootfiles)) | $(call totarget,sign)
|   | > bootfiles
|   |   | bootfiles = $(call listf_cc,boot)
|   |   | $(foreach f,$(bootfiles),$(call cc_compile,$(f),$(CC),$(CFLAGS) -Os -nostdinc))
|   |   |   | 以生成bootasm.o为例，执行的命令是gcc -Iboot/ -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Os -nostdinc -c boot/bootasm.S -o obj/boot/bootasm.o。生成其它.o文件也类似，不再赘述
|   | 
|   | > sign
|   |   | $(call add_files_host,tools/sign.c,sign,sign)
|   |   |   | gcc -Itools/ -g -Wall -O2 -c tools/sign.c -o obj/sign/tools/sign.o
|   |   | $(call create_target_host,sign,sign)
|   |   |   | gcc -g -Wall -O2 obj/sign/tools/sign.o -o bin/sign
|   | 
|   | @echo + ld $@
|   | $(V)$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 $^ -o $(call toobj,bootblock)
|   |   | ld -m    elf_i386 -nostdlib -N -e start -Ttext 0x7C00 obj/boot/bootasm.o obj/boot/bootmain.o -o obj/bootblock.o
|   | @$(OBJDUMP) -S $(call objfile,bootblock) > $(call asmfile,bootblock)
|   | @$(OBJDUMP) -t $(call objfile,bootblock) | $(SED) '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(call symfile,bootblock)
|   | @$(OBJCOPY) -S -O binary $(call objfile,bootblock) $(call outfile,bootblock)
|   | @$(call totarget,sign) $(call outfile,bootblock) $(bootblock)
| 
| $(V)dd if=/dev/zero of=$@ count=10000
|   | dd if=/dev/zero of=bin/ucore.img count=10000
| $(V)dd if=$(bootblock) of=$@ conv=notrunc
|   | dd if=bin/bootblock of=bin/ucore.img conv=notrunc
| $(V)dd if=$(kernel) of=$@ seek=1 conv=notrunc
|   | dd if=bin/kernel of=bin/ucore.img seek=1 conv=notrunc
```

[练习1.2] 一个被系统认为是符合规范的硬盘主引导扇区的特征是什么？

> 扇区以0x55 0xAA结束。

## [练习2] 使用qemu执行并调试lab1中的软件

[练习2.1] 从CPU加电后执行的第一条指令开始，单步跟踪BIOS的执行。

> 把tools/gdbinit改成

```
file bin/kernel
target remote :1234
set architecture i8086
```

> 然后执行`make debug`，显示`PC: 0xfff0`即CPU加电后第一条指令。

[练习2.2] 在初始化位置0x7c00设置实地址断点,测试断点正常。

> 输入`b *0x7c00`和`continue`，显示

```
Continuing.

Breakpoint 1, 0x00007c00 in ?? ()
```

> 此时PC值为`PC: 0x7c00`，即证明断点正常。

[练习2.3] 从0x7c00开始跟踪代码运行,将单步跟踪反汇编得到的代码与bootasm.S和bootblock.asm进行比较。

> 在上一题基础上，输入`x /10i $pc`，显示

```
=> 0x7c00:      cli
   0x7c01:      cld
   0x7c02:      xor    %ax,%ax
   0x7c04:      mov    %ax,%ds
   0x7c06:      mov    %ax,%es
   0x7c08:      mov    %ax,%ss
   0x7c0a:      in     $0x64,%al
   0x7c0c:      test   $0x2,%al
   0x7c0e:      jne    0x7c0a
   0x7c10:      mov    $0xd1,%al
```

> bootasm.S的内容：

```
start:
.code16                                             # Assemble for 16-bit mode
    cli                                             # Disable interrupts
    cld                                             # String operations increment

    # Set up the important data segment registers (DS, ES, SS).
    xorw %ax, %ax                                   # Segment number zero
    movw %ax, %ds                                   # -> Data Segment
    movw %ax, %es                                   # -> Extra Segment
    movw %ax, %ss                                   # -> Stack Segment
```

> bootblock.asm的内容：

```
.globl start
start:
.code16                                             # Assemble for 16-bit mode
    cli                                             # Disable interrupts
    7c00:	fa                   	cli
    cld                                             # String operations increment
    7c01:	fc                   	cld

    # Set up the important data segment registers (DS, ES, SS).
    xorw %ax, %ax                                   # Segment number zero
    7c02:	31 c0                	xor    %eax,%eax
    movw %ax, %ds                                   # -> Data Segment
    7c04:	8e d8                	mov    %eax,%ds
    movw %ax, %es                                   # -> Extra Segment
    7c06:	8e c0                	mov    %eax,%es
    movw %ax, %ss                                   # -> Stack Segment
    7c08:	8e d0                	mov    %eax,%ss
```

> 它们是一致的。

[练习2.4] 自己找一个bootloader或内核中的代码位置，设置断点并进行测试。

> 在上一题基础上，输入`break kern_init`和`continue`，显示

```
   │16      int                                                                │
B+>│17      kern_init(void) {                                                  │
   │18          extern char edata[], end[];                                    │
   │19          memset(edata, 0, end - edata);                                 │
```

> 此时PC值为`PC: 0x100000`，再输入`x /5i $pc`，显示

```
=> 0x100000 <kern_init>:        push   %bp
   0x100001 <kern_init+1>:      mov    %sp,%bp
   0x100003 <kern_init+3>:      sub    $0x28,%sp
   0x100006 <kern_init+6>:      mov    $0xfd20,%dx
   0x100009 <kern_init+9>:      adc    %al,(%bx,%si)
```

> 即可查看kern_init的汇编代码。

## [练习3] 分析bootloader进入保护模式的过程

> 执行这些汇编指令（于bootasm.S中）后进入保护模式

```
.globl start
start:
.code16                                             # Assemble for 16-bit mode
    cli                                             # Disable interrupts
    cld                                             # String operations increment

    # Set up the important data segment registers (DS, ES, SS).
    xorw %ax, %ax                                   # Segment number zero
    movw %ax, %ds                                   # -> Data Segment
    movw %ax, %es                                   # -> Extra Segment
    movw %ax, %ss                                   # -> Stack Segment

    # Enable A20:
    #  For backwards compatibility with the earliest PCs, physical
    #  address line 20 is tied low, so that addresses higher than
    #  1MB wrap around to zero by default. This code undoes this.
seta20.1:
    inb $0x64, %al                                  # Wait for not busy(8042 input buffer empty).
    testb $0x2, %al
    jnz seta20.1

    movb $0xd1, %al                                 # 0xd1 -> port 0x64
    outb %al, $0x64                                 # 0xd1 means: write data to 8042's P2 port

seta20.2:
    inb $0x64, %al                                  # Wait for not busy(8042 input buffer empty).
    testb $0x2, %al
    jnz seta20.2

    movb $0xdf, %al                                 # 0xdf -> port 0x60
    outb %al, $0x60                                 # 0xdf = 11011111, means set P2's A20 bit(the 1 bit) to 1

    # Switch from real to protected mode, using a bootstrap GDT
    # and segment translation that makes virtual addresses
    # identical to physical addresses, so that the
    # effective memory map does not change during the switch.
    lgdt gdtdesc
    movl %cr0, %eax
    orl $CR0_PE_ON, %eax
    movl %eax, %cr0
```

> 进入保护模式的标志是，把CR0的R0_PE标志位置为1。此前做了一些准备工作：清理环境，开启A20，初始化GDT。

[练习3.1] 为何开启A20，以及如何开启A20

> 因为早期机器支持的物理地址只有20位，为了支持更大地址空间，所以需要开启A20。开启A20的方法是：

```
seta20.1:
    inb $0x64, %al                                  # Wait for not busy(8042 input buffer empty).
    testb $0x2, %al
    jnz seta20.1

    movb $0xd1, %al                                 # 0xd1 -> port 0x64
    outb %al, $0x64                                 # 0xd1 means: write data to 8042's P2 port

seta20.2:
    inb $0x64, %al                                  # Wait for not busy(8042 input buffer empty).
    testb $0x2, %al
    jnz seta20.2

    movb $0xdf, %al                                 # 0xdf -> port 0x60
    outb %al, $0x60                                 # 0xdf = 11011111, means set P2's A20 bit(the 1 bit) to 1
```

[练习3.2] 如何初始化GDT表

> 直接使用Load GDT指令`lgdt gdtdesc`载入GDT表。

[练习3.3] 如何使能和进入保护模式

> 把cr0寄存器的CR0_PE位置为1：

```
    movl %cr0, %eax
    orl $CR0_PE_ON, %eax
    movl %eax, %cr0
```

## [练习4] 分析bootloader加载ELF格式的OS的过程

[练习4.1] bootloader如何读取硬盘扇区的

> 用readsect函数读取磁盘扇区。readsect函数调用了waitdisk、outb、insl这三个基本磁盘操作。

```
static void
readsect(void *dst, uint32_t secno) {
    // wait for disk to be ready
    waitdisk();

    outb(0x1F2, 1);                         // count = 1
    outb(0x1F3, secno & 0xFF);
    outb(0x1F4, (secno >> 8) & 0xFF);
    outb(0x1F5, (secno >> 16) & 0xFF);
    outb(0x1F6, ((secno >> 24) & 0xF) | 0xE0);
    outb(0x1F7, 0x20);                      // cmd 0x20 - read sectors

    // wait for disk to be ready
    waitdisk();

    // read a sector
    insl(0x1F0, dst, SECTSIZE / 4);
}
```

[练习4.2] bootloader是如何加载ELF格式的OS

> 在bootmain函数中加载ELF格式的OS。

```
    // read the 1st page off disk
    readseg((uintptr_t)ELFHDR, SECTSIZE * 8, 0);

    // is this a valid ELF?
    if (ELFHDR->e_magic != ELF_MAGIC) {
        goto bad;
    }

    struct proghdr *ph, *eph;

    // load each program segment (ignores ph flags)
    ph = (struct proghdr *)((uintptr_t)ELFHDR + ELFHDR->e_phoff);
    eph = ph + ELFHDR->e_phnum;
    for (; ph < eph; ph ++) {
        readseg(ph->p_va & 0xFFFFFF, ph->p_memsz, ph->p_offset);
    }

    // call the entry point from the ELF header
    // note: does not return
    ((void (*)(void))(ELFHDR->e_entry & 0xFFFFFF))();
```

> 先读出磁盘第一个扇区上的ELF Header，验证Header的正确性。ELF头扇区的格式为：

```
struct elfhdr {
    uint32_t e_magic;     // must equal ELF_MAGIC
    uint8_t e_elf[12];
    uint16_t e_type;      // 1=relocatable, 2=executable, 3=shared object, 4=core image
    uint16_t e_machine;   // 3=x86, 4=68K, etc.
    uint32_t e_version;   // file version, always 1
    uint32_t e_entry;     // entry point if executable
    uint32_t e_phoff;     // file position of program header or 0
    uint32_t e_shoff;     // file position of section header or 0
    uint32_t e_flags;     // architecture-specific flags, usually 0
    uint16_t e_ehsize;    // size of this elf header
    uint16_t e_phentsize; // size of an entry in program header
    uint16_t e_phnum;     // number of entries in program header or 0
    uint16_t e_shentsize; // size of an entry in section header
    uint16_t e_shnum;     // number of entries in section header or 0
    uint16_t e_shstrndx;  // section number that contains section name strings
};
```

> 利用Header里面的信息加载OS。

## [练习5] 实现函数调用堆栈跟踪函数

[练习5.1] 看看输出是否与上述显示大致一致，并解释最后一行各个数值的含义

> 输出是

```
ebp:0x00007b08 eip:0x001009a6 args:0x00010094 0x00000000 0x00007b38 0x00100092 
    kern/debug/kdebug.c:306: print_stackframe+21
ebp:0x00007b18 eip:0x00100c95 args:0x00000000 0x00000000 0x00000000 0x00007b88 
    kern/debug/kmonitor.c:125: mon_backtrace+10
ebp:0x00007b38 eip:0x00100092 args:0x00000000 0x00007b60 0xffff0000 0x00007b64 
    kern/init/init.c:48: grade_backtrace2+33
ebp:0x00007b58 eip:0x001000bb args:0x00000000 0xffff0000 0x00007b84 0x00000029 
    kern/init/init.c:53: grade_backtrace1+38
ebp:0x00007b78 eip:0x001000d9 args:0x00000000 0x00100000 0xffff0000 0x0000001d 
    kern/init/init.c:58: grade_backtrace0+23
ebp:0x00007b98 eip:0x001000fe args:0x0010341c 0x00103400 0x0000130a 0x00000000 
    kern/init/init.c:63: grade_backtrace+34
ebp:0x00007bc8 eip:0x00100055 args:0x00000000 0x00000000 0x00000000 0x00010094 
    kern/init/init.c:28: kern_init+84
ebp:0x00007bf8 eip:0x00007d68 args:0xc031fcfa 0xc08ed88e 0x64e4d08e 0xfa7502a8 
    <unknow>: -- 0x00007d67 --
```

> 基本一致。ebp是栈顶指针，[ebp]是调用者的ebp；[ebp+4]是return address，即调用者调用处的后一条指令地址；[ebp+8]是第一个调用参数，[ebp+b]是第二个参数，以此类推。

## [练习6] 完善中断初始化和处理

[练习6.1] 中断描述符表（也可简称为保护模式下的中断向量表）中一个表项占多少字节？其中哪几位代表中断处理代码的入口？

> 8字节。

```
struct gatedesc {
    unsigned gd_off_15_0 : 16;        // low 16 bits of offset in segment
    unsigned gd_ss : 16;            // segment selector
    unsigned gd_args : 5;            // # args, 0 for interrupt/trap gates
    unsigned gd_rsv1 : 3;            // reserved(should be zero I guess)
    unsigned gd_type : 4;            // type(STS_{TG,IG32,TG32})
    unsigned gd_s : 1;                // must be 0 (system)
    unsigned gd_dpl : 2;            // descriptor(meaning new) privilege level
    unsigned gd_p : 1;                // Present
    unsigned gd_off_31_16 : 16;        // high bits of offset in segment
};
```

> 第0~15位、48~63位拼起来代表中断处理代码的入口。

[练习6.2] 请编程完善kern/trap/trap.c中对中断向量表进行初始化的函数idt_init。在idt_init函数中，依次对所有中断入口进行初始化。使用mmu.h中的SETGATE宏，填充idt数组内容。每个中断的入口由tools/vectors.c生成，使用trap.c中声明的vectors数组即可。

> 完成。

[练习6.3] 请编程完善trap.c中的中断处理函数trap，在对时钟中断进行处理的部分填写trap函数中处理时钟中断的部分，使操作系统每遇到100次时钟中断后，调用print_ticks子程序，向屏幕上打印一行文字”100 ticks”。

> 完成。

## 与参考答案的区别：

> 1. 练习6.2中，参考答案是错误的，没有把T_SYSCALL的级别设为用户级3，在今后lab5加入system call之后会出错。我暂时没有做challange，所以没有把T_SWITCH_TOK的级别设为用户级3，而且这个中断允许用户程序随意切换到核心态权限，本身就不安全，不应该实现它。
> 2. 练习6.3中，参考答案是错误的，当ticks = 2147483600后，加47会变成2147483647，再加一会溢出，变成-2147483648，再加48会变成-2147483600，再次触发print_ticks，打印出`100 ticks`，但实际上只经过了96个时钟中断。我给出的解答考虑了这种情况，避免溢出造成的错误。

## 重要的知识点

> 中断描述符表。对应知识点：中断描述符表。含义：将每个异常或中断向量分别与它们的处理过程联系起来，记录它们的入口地址、特权级等信息的表。
