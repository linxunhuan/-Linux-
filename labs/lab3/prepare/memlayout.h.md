```c
// 物理内存布局 
 
// qemu -machine virt 设置如下，
// 基于 qemu 的 hw/riscv/virt.c: 
// 
// 00001000 -- 启动 ROM，由 qemu 提供 
// 02000000 -- CLINT 
// 0C000000 -- PLIC 
// 10000000 -- uart0 
// 10001000 -- virtio 磁盘 
// 80000000 -- 启动 ROM 在机器模式下跳转到此处 
// -kernel 在此处加载内核 
// 80000000 之后未使用的 RAM。 
 
// 内核使用物理内存如下： 
// 80000000 -- entry.S，然后是内核文本和数据 
// end -- 内核页面分配区域的开始 
// PHYSTOP -- 使用的 RAM 结束内核 
 
// qemu 将 UART 寄存器放在物理内存中
#define UART0 0x10000000L
#define UART0_IRQ 10

// virtio mmio 接口
#define VIRTIO0 0x10001000
#define VIRTIO0_IRQ 1

// 本地中断控制器，包含计时器
#define CLINT 0x2000000L
#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8*(hartid))
#define CLINT_MTIME (CLINT + 0xBFF8) // 自启动以来的循环次数

// qemu 将可编程中断控制器放在这里
#define PLIC 0x0c000000L
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_MENABLE(hart) (PLIC + 0x2000 + (hart)*0x100)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_MPRIORITY(hart) (PLIC + 0x200000 + (hart)*0x2000)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_MCLAIM(hart) (PLIC + 0x200004 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)

// 内核期望有 RAM 
// 供内核和用户页面 
// 从物理地址 0x80000000 到 PHYSTOP 使用
#define KERNBASE 0x80000000L
#define PHYSTOP (KERNBASE + 128*1024*1024)

// 将 trampoline 页面映射到用户空间和内核空间中的最高地址 
#define TRAMPOLINE (MAXVA - PGSIZE)

// 将内核堆栈映射到蹦床下方，
// 每个都被无效的保护页包围
#define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*PGSIZE)

// 用户内存布局。 
// 地址零优先： 
// 文本 
// 原始数据和 bss 
// 固定大小堆栈 
// 可扩展堆 
// ... 
// TRAPFRAME (p->trapframe，由 trampoline 使用) 
// TRAMPOLINE (与内核中的页面相同)
#define TRAPFRAME (TRAMPOLINE - PGSIZE)
```