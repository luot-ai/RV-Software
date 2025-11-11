# 16点复数FFT实现 (纯定点数版本)

这是一个为ZAP RISC-V处理器优化的16点复数快速傅里叶变换(FFT)实现，使用纯定点数算法。

## 特性

- **纯定点数算法**: 使用Q15格式定点数，完全避免浮点运算
- **无浮点依赖**: 适用于没有FPU的嵌入式系统，无需软浮点库
- **避免LDM/STM指令**: 优化代码结构，避免生成多寄存器加载/存储指令
- **无库函数依赖**: 不依赖memcpy、memset等标准库函数
- **预计算旋转因子**: 提高运算效率
- **经典测试用例**: 包含脉冲、DC、单频、IFFT正确性测试
- **高效实现**: 使用Cooley-Tukey算法，4级蝶形运算

## 文件说明

- `cfft.h` - 头文件，定义Q15定点数据结构和函数接口
- `cfft.c` - 主实现文件，包含纯定点数FFT算法
- `Config.cfg` - 配置文件
- `test.s` - 汇编测试文件
- `README.md` - 本说明文档

## 数据格式

### Q15定点数格式
- **数据类型**: `int32_t`
- **有效范围**: [-1.0, 1.0)
- **缩放因子**: 32768 (2^15)
- **精度**: 约15位有效精度

### 示例转换
```
 1.0    → 32767
 0.5    → 16384
 0.0    → 0
-0.5    → -16384
-1.0    → -32768
```

### 复数结构
```c
typedef struct {
    int32_t real;  // Q15格式实部
    int32_t imag;  // Q15格式虚部
} complex_t;
```

## API接口

### 核心函数
```c
// 16点FFT
void fft_16_point(const complex_t input[16], complex_t output[16]);

// 16点逆FFT
void ifft_16_point(const complex_t input[16], complex_t output[16]);

// 综合测试函数
int test_fft_16(void);
```

## 编译

### ARM交叉编译（避免LDM/STM指令）
```bash
arm-none-eabi-gcc -Wall -O2 -std=c99 -nostdlib -ffreestanding \
  -fno-builtin -mno-unaligned-access -fno-strict-aliasing \
  -fno-tree-loop-distribute-patterns -fno-tree-loop-vectorize \
  cfft.c -o cfft_test
```

### RISC-V交叉编译
```bash
riscv64-unknown-elf-gcc -Wall -O2 -std=c99 -nostdlib -ffreestanding \
  -fno-builtin cfft.c -o cfft_test
```

### 本地测试编译
```bash
gcc -Wall -O2 -std=c99 cfft.c -o cfft_test
```

## 测试用例

程序内置4个测试用例，所有预期结果都已预先计算：

### 1. 脉冲信号测试
- **输入**: [32767, 0, 0, ..., 0] (Q15格式的[1, 0, 0, ..., 0])
- **预期**: 所有频率分量幅值相等
- **验证**: 检查所有频率分量的幅值在12.5%容差内

### 2. DC信号测试  
- **输入**: [32767, 32767, ..., 32767] (Q15格式的[1, 1, ..., 1])
- **预期**: 只有第0个频率分量(DC)非零，其他接近0
- **验证**: DC分量幅值远大于其他分量

### 3. 单频信号测试
- **输入**: 预计算的cos(2πk/16) - j*sin(2πk/16)，k=0到15
- **预期**: 能量集中在第1个频率分量
- **验证**: 第1个分量幅值远大于其他分量

### 4. IFFT正确性测试
- **测试**: IFFT(FFT(x)) = x
- **验证**: 原信号与IFFT重构信号的误差在容差范围内

## 算法详解

### Cooley-Tukey FFT算法
1. **位反转重排**: 使用查找表快速重排序输入数据
2. **4级蝶形运算**: 16点FFT需要4级处理
3. **旋转因子**: 预计算的8个复数旋转因子
4. **原位计算**: 节省内存空间

### 定点数乘法
- 使用64位中间结果避免溢出
- Q15 × Q15 = Q30，然后右移15位回到Q15
- 复数乘法: (a+bj)(c+dj) = (ac-bd) + (ad+bc)j

## 性能特点

### 优势
- **无浮点运算**: 避免软浮点库依赖和性能损失
- **避免LDM/STM**: 单寄存器操作，兼容更多ARM处理器
- **无库函数调用**: 避免memcpy/memset等标准库依赖
- **预计算常数**: 所有三角函数值预先计算
- **内联函数**: 减少函数调用开销
- **位操作**: 使用移位代替除法

### 精度
- Q15格式提供约4.6位十进制精度
- 测试容差设为12.5%以适应定点数误差
- 适用于大多数实时信号处理应用

## 使用示例

```c
#include "cfft.h"

int main() {
    // 定义输入信号 (Q15格式)
    complex_t input[16] = {
        {32767, 0},    // 1.0 + 0.0j
        {16384, 0},    // 0.5 + 0.0j
        {0, 0},        // 0.0 + 0.0j
        // ... 其余初始化为0
    };
    
    complex_t output[16];
    
    // 执行16点FFT
    fft_16_point(input, output);
    
    // 处理频域结果
    for (int i = 0; i < 16; i++) {
        // output[i].real 和 output[i].imag 包含频域数据
        // 注意：这些是Q15格式的定点数
    }
    
    // 如果需要，可以执行逆FFT
    complex_t reconstructed[16];
    ifft_16_point(output, reconstructed);
    
    return 0;
}
```

## 注意事项

1. **输入范围**: 确保输入数据在[-32768, 32767]范围内
2. **溢出检查**: 虽然算法内部有溢出保护，但建议输入数据不要接近边界值
3. **精度损失**: 定点数运算会有舍入误差，这在测试中已考虑
4. **归一化**: FFT输出未归一化，IFFT会自动除以16

## 编译优化建议

```bash
# 高度优化版本
gcc -Wall -O3 -std=c99 -march=native cfft.c -o cfft_test

# 调试版本
gcc -Wall -g -std=c99 cfft.c -o cfft_test_debug
```

## 错误码

- **返回0**: 所有测试通过
- **返回-1**: 测试失败

运行程序后检查返回码即可知道FFT实现是否正确。