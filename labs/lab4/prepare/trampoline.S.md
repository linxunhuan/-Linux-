# 涉及从用户空间切换到内核空间并返回的程序集
```s
	#
    # 在管理模式下中断和异常发生在这里
    # 推送所有寄存器，调用 kerneltrap()，恢复，返回
.globl kerneltrap
.globl kernelvec
.align 4            // 对齐到4字节边界
kernelvec:
        // 为保存寄存器腾出空间，减少栈指针sp的值.
        addi sp, sp, -256

        // 逐个保存寄存器到栈中，从栈顶开始，每个寄存器占用 8 字节（64 位架构）
        sd ra, 0(sp)    
        sd sp, 8(sp)
        sd gp, 16(sp)
        sd tp, 24(sp)
        sd t0, 32(sp)
        sd t1, 40(sp)
        sd t2, 48(sp)
        sd s0, 56(sp)
        sd s1, 64(sp)
        sd a0, 72(sp)
        sd a1, 80(sp)
        sd a2, 88(sp)
        sd a3, 96(sp)
        sd a4, 104(sp)
        sd a5, 112(sp)
        sd a6, 120(sp)
        sd a7, 128(sp)
        sd s2, 136(sp)
        sd s3, 144(sp)
        sd s4, 152(sp)
        sd s5, 160(sp)
        sd s6, 168(sp)
        sd s7, 176(sp)
        sd s8, 184(sp)
        sd s9, 192(sp)
        sd s10, 200(sp)
        sd s11, 208(sp)
        sd t3, 216(sp)
        sd t4, 224(sp)
        sd t5, 232(sp)
        sd t6, 240(sp)

	// 调用c语言编写的trap处理函数（位于trap.c文件中）
        call kerneltrap

        // 恢复寄存器的值.
        ld ra, 0(sp)
        ld sp, 8(sp)
        ld gp, 16(sp)
        // 不恢复tp，因为可能更换了 CPUs: ld tp, 24(sp)
        ld t0, 32(sp)
        ld t1, 40(sp)
        ld t2, 48(sp)
        ld s0, 56(sp)
        ld s1, 64(sp)
        ld a0, 72(sp)
        ld a1, 80(sp)
        ld a2, 88(sp)
        ld a3, 96(sp)
        ld a4, 104(sp)
        ld a5, 112(sp)
        ld a6, 120(sp)
        ld a7, 128(sp)
        ld s2, 136(sp)
        ld s3, 144(sp)
        ld s4, 152(sp)
        ld s5, 160(sp)
        ld s6, 168(sp)
        ld s7, 176(sp)
        ld s8, 184(sp)
        ld s9, 192(sp)
        ld s10, 200(sp)
        ld s11, 208(sp)
        ld t3, 216(sp)
        ld t4, 224(sp)
        ld t5, 232(sp)
        ld t6, 240(sp)

        // 恢复指针到中断前的状态
        addi sp, sp, 256

        // 返回到中断前内核正在执行的代码.
        sret

        #
        # 机器模式定时器中断.
        #
.globl timervec
.align 4
timervec:
        // start.c 文件已经设置了 mscratch 所指向的内存
        // scratch[0,8,16] : 寄存器保存区
        // scratch[32] : CLINT 的 MTIMECMP 寄存器地址
        // scratch[40] : 中断之间的期望间隔
        
        csrrw a0, mscratch, a0  // 交换 a0 和 mscratch 的值
        sd a1, 0(a0)            // 保存 a1 到保存区的偏移 0 位置
        sd a2, 8(a0)            // 保存 a2 到保存区的偏移 8 位置
        sd a3, 16(a0)           // 保存 a3 到保存区的偏移 16 位置

        # 调度下一个定时器中断
        # 通过将间隔值添加到mtimecmp.
        ld a1, 32(a0)           // 加载 CLINT_MTIMECMP(hart) 地址到 a1
        ld a2, 40(a0)           // 加载中断间隔到 a2
        ld a3, 0(a1)            // 加载 mtimecmp 当前值到 a3
        add a3, a3, a2          // 将中断间隔添加到 mtimecmp
        sd a3, 0(a1)            // 保存新的 mtimecmp 值

        # 提升一个超级软件中断.
	li a1, 2                    // 将 2 写入 a1，表示超级软件中断
        csrw sip, a1            // 设置 sip 寄存器

        ld a3, 16(a0)           // 恢复 a3 的值
        ld a2, 8(a0)
        ld a1, 0(a0)
        csrrw a0, mscratch, a0  // 交换 a0 和 mscratch 的值

        mret                    // 从中断返回
```










