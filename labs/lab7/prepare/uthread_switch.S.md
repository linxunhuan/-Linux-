```S
	.text

	/*
        * 保存旧线程的寄存器，
        * 恢复新线程的寄存器。
         */

	.globl thread_switch  // 使得 thread_switch 成为全局符号，可以被其他文件引用
thread_switch:          
	/* save old registers */
	// 将当前线程（旧线程）的寄存器值保存到栈或指定位置，以便后续恢复
	sd sp, 0(a0)     // 保存栈指针 (sp) 到 a0 寄存器所指向的内存位置的偏移量 0
	sd s0, 8(a0)     // 保存 s0 寄存器到 a0 寄存器所指向的内存位置的偏移量 8
	sd s1, 16(a0)    // 保存 s1 寄存器到 a0 寄存器所指向的内存位置的偏移量 16
	sd s2, 24(a0)    // 保存 s2 寄存器到 a0 寄存器所指向的内存位置的偏移量 24
	sd s3, 32(a0)    // 保存 s3 寄存器到 a0 寄存器所指向的内存位置的偏移量 32
	sd s4, 40(a0)    // 保存 s4 寄存器到 a0 寄存器所指向的内存位置的偏移量 40
	sd s5, 48(a0)    // 保存 s5 寄存器到 a0 寄存器所指向的内存位置的偏移量 48
	sd s6, 56(a0)    // 保存 s6 寄存器到 a0 寄存器所指向的内存位置的偏移量 56
	sd s7, 64(a0)    // 保存 s7 寄存器到 a0 寄存器所指向的内存位置的偏移量 64
	sd s8, 72(a0)    // 保存 s8 寄存器到 a0 寄存器所指向的内存位置的偏移量 72
	sd s9, 80(a0)    // 保存 s9 寄存器到 a0 寄存器所指向的内存位置的偏移量 80
	sd s10, 88(a0)   // 保存 s10 寄存器到 a0 寄存器所指向的内存位置的偏移量 88
	sd s11, 96(a0)   // 保存 s11 寄存器到 a0 寄存器所指向的内存位置的偏移量 96
	sd ra, 104(a0)   // 保存返回地址寄存器 (ra) 到 a0 寄存器所指向的内存位置的偏移量 104

	/* load new registers */
	// 恢复新线程的寄存器状态
	ld sp, 0(a1)     // 加载新的栈指针 (sp) 从 a1 寄存器所指向的内存位置的偏移量 0
	ld s0, 8(a1)     // 加载新的 s0 寄存器的值
	ld s1, 16(a1)    // 加载新的 s1 寄存器的值
	ld s2, 24(a1)    // 加载新的 s2 寄存器的值
	ld s3, 32(a1)    // 加载新的 s3 寄存器的值
	ld s4, 40(a1)    // 加载新的 s4 寄存器的值
	ld s5, 48(a1)    // 加载新的 s5 寄存器的值
	ld s6, 56(a1)    // 加载新的 s6 寄存器的值
	ld s7, 64(a1)    // 加载新的 s7 寄存器的值
	ld s8, 72(a1)    // 加载新的 s8 寄存器的值
	ld s9, 80(a1)    // 加载新的 s9 寄存器的值
	ld s10, 88(a1)   // 加载新的 s10 寄存器的值
	ld s11, 96(a1)   // 加载新的 s11 寄存器的值
	ld ra, 104(a1)   // 加载新的返回地址寄存器 (ra) 的值

	ret    // 返回，跳转到恢复的栈指针 (sp) 和返回地址寄存器 (ra) 指向的地址处继续执行新线程

```