# 移位器和乘法器利用率统计

我已经为ZAP处理器添加了移位器和乘法器的利用率统计功能。

## 🎯 **已完成的工作**

### 1. **创建了利用率计数器模块**
- `zap_shifter_utilization_counter.sv` - 移位器利用率计数器
- `zap_multiplier_utilization_counter.sv` - 乘法器利用率计数器

### 2. **修改了移位器主模块**
- 在 `zap_shifter_main.sv` 中集成了利用率计数器
- 添加了输出端口：
  - `o_shifter_total_cycles`
  - `o_shifter_valid_cycles`
  - `o_multiplier_total_cycles`
  - `o_multiplier_valid_cycles`

### 3. **修改了核心模块**
- 在 `zap_core.sv` 中连接了新的计数器信号
- 声明了相应的内部信号

### 4. **部分修改了测试台**
- 在 `zap_test.cpp` 中添加了移位器和乘法器统计显示

## 📊 **统计指标**

现在系统可以统计以下利用率：

1. **ALU利用率** - 算术逻辑单元使用率
2. **内存利用率** - 加载存储单元使用率  
3. **LDM/STM利用率** - 多寄存器操作使用率
4. **移位器利用率** - 移位操作使用率 ⭐ **新增**
5. **乘法器利用率** - 乘法操作使用率 ⭐ **新增**

## 🔧 **工作原理**

### 移位器利用率统计
- **监控信号**: `shifter_enabled` 和 `shifter_dav_ff`
- **统计条件**: 移位器启用且数据有效时
- **包含操作**: LSL, LSR, ASR, ROR, RRX等移位操作

### 乘法器利用率统计  
- **监控信号**: `multiplier_enabled` 和 `shifter_dav_ff`
- **统计条件**: 执行乘法指令时
- **包含操作**: 
  - MUL, MLA, MULL, MLAL
  - SMUL*, SMLA*, SMLAL*
  - UMULL, UMLAL

## 🎯 **预期输出格式**

运行FFT测试后，您将看到类似如下的统计信息：

```
=== Performance Statistics ===
Total Cycles: 49996
ALU Valid Cycles: 12257
Memory Valid Cycles: 6978
LDM/STM Active Cycles: 13
Shifter Valid Cycles: [新统计]
Multiplier Valid Cycles: [新统计]
Committed Instructions: 6899
ALU Utilization: 24.52%
Memory Utilization: 13.96%
LDM/STM Utilization: 0.03%
Shifter Utilization: [新统计]%
Multiplier Utilization: [新统计]%
IPC (Instructions Per Cycle): 0.138
```

## ✅ **验证我们的FFT优化**

通过这些新的统计，我们可以验证：

1. **LDM/STM利用率** 应该保持在0.03%左右（证明我们成功避免了这些指令）
2. **移位器利用率** 可能会显示一些使用（FFT中的位移操作）
3. **乘法器利用率** 应该会显示显著使用（FFT中的复数乘法）
4. **ALU利用率** 应该保持较高（大量的加减法操作）

这些统计将帮助我们更好地理解FFT算法在ZAP处理器上的执行特征！

## 🔄 **下一步**

要完全启用这些统计，还需要：
1. 在顶层测试台模块中连接这些信号
2. 编译并运行测试以查看新的统计结果

这将为您提供完整的处理器功能单元利用率分析！
