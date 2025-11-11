#!/bin/bash

echo "=== 测试ARM交叉编译（避免LDM/STM指令）==="
echo "编译16点FFT定点数实现..."

# 设置编译器和优化标志
CC=arm-none-eabi-gcc
CFLAGS="-Wall -O2 -std=c99 -nostdlib -ffreestanding -fno-builtin"

# 添加避免LDM/STM指令的标志
CFLAGS="$CFLAGS -mno-unaligned-access -fno-strict-aliasing"
CFLAGS="$CFLAGS -fno-tree-loop-distribute-patterns -fno-tree-loop-vectorize"

echo "编译选项: $CFLAGS"
echo ""

echo "正在编译 cfft.c..."
$CC $CFLAGS -c cfft.c -o cfft.o

if [ $? -eq 0 ]; then
    echo "✓ 编译成功！"
    echo ""
    echo "生成的目标文件:"
    ls -la cfft.o
    echo ""
    echo "检查符号依赖:"
    arm-none-eabi-nm cfft.o | grep -E "(memcpy|memset|__aeabi_)"
    if [ $? -ne 0 ]; then
        echo "✓ 无外部库函数依赖！"
    else
        echo "⚠ 仍有外部依赖，请检查上述符号"
    fi
    
    echo ""
    echo "反汇编检查LDM/STM指令:"
    arm-none-eabi-objdump -d cfft.o | grep -E "(ldm|stm)" | head -10
    if [ $? -ne 0 ]; then
        echo "✓ 未发现LDM/STM指令！"
    else
        echo "⚠ 发现LDM/STM指令，请检查上述输出"
    fi
else
    echo "❌ 编译失败！"
    exit 1
fi

echo ""
echo "测试完成。"
