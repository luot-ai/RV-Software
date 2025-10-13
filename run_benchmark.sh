#!/bin/bash

# 用法: ./run_benchmark.sh <dir> <num_runs>
dir=$1
runs=$2

if [ -z "$dir" ] || [ -z "$runs" ]; then
    echo "Usage: $0 <dir> <num_runs>"
    exit 1
fi

logfile="$dir/run_log.txt"
mkdir -p "$dir"
> "$logfile"  # 清空日志文件

total_cycles_sum=0
total_ipc_sum=0

for ((i=1;i<=runs;i++))
do
    echo "---- Run #$i ----"
    # clean 仿真环境
    make -C "$dir" clean >/dev/null 2>&1
    # 运行 make run
    output=$(make -C "$dir" run 2>/dev/null)

    # 从输出中抓取 Total cycles / IPC
    line=$(echo "$output" | grep "Total cycles:" | sed 's/\x1b\[[0-9;]*m//g')
    echo "$line"
    echo "$line" >> "$logfile"

    # 提取数字
    cycles=$(echo "$line" | awk '{print $3}' | tr -cd '0-9')
    ipc=$(echo "$line" | awk '{print $9}' | tr -cd '0-9.')

    # 如果 ipc 为空则设为 0
    if [ -z "$ipc" ]; then
        ipc=0
    fi

    # 累加
    total_cycles_sum=$(echo "$total_cycles_sum + $cycles" | bc)
    total_ipc_sum=$(echo "$total_ipc_sum + $ipc" | bc -l)
done

# 计算平均值
avg_cycles=$(echo "scale=2; $total_cycles_sum / $runs" | bc)
avg_ipc=$(echo "scale=5; $total_ipc_sum / $runs" | bc)

echo "✅ 平均 Total cycles: $avg_cycles"
echo "✅ 平均 IPC: $avg_ipc"
echo "结果已保存到: $logfile"

echo "✅ 平均 Total cycles: $avg_cycles" >> "$logfile"
echo "✅ 平均 IPC: $avg_ipc" >> "$logfile"
