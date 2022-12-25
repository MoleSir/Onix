CFLAGS := -m32
#CFLAGS += -fno-builtin # 不需要 gcc 内置函数
#CFLAGS += -nostdinc # 不需要标准头文件
CFLAGS += -fno-pic # 不需要位置无关的代码 	position independent code
#CFLAGS += -fno-pie # 不需要位置无关的可执行程序 	position independent executable
#CFLAGS += -nostdlib # 不需要标准库
#CFLAGS += -fno-stack-protector 
CFLAGS += -Qn
CFLAGS += -fno-asynchronous-unwind-tables
CFLAGS += -mpreferred-stack-boundary=2
#CFLAGS += -fomit-frame-pointer
CFLAGS := $(strip ${CFLAGS})

.PHONY: hello.s
hello.s: hello.c
	gcc $(CFLAGS) -S $< -o $@

.PHONY: param.s
param.s: param.c
	gcc $(CFLAGS) -S $< -o $@