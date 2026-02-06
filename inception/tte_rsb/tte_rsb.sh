# 可选第 4 个参数：传给 clang 的额外编译选项
if [ "$#" -lt 3 ]; then
  echo "Usage: $0 CORE1 CORE2 OUTPUT_DIR [CLANG_ARGS]" >&2
  exit 1
fi

mkdir -p $3 

# 关闭 sibling CPU; SMT sibling 干扰;其他进程被调度到同一物理核心
sudo bash -c "echo 0 > /sys/devices/system/cpu/cpu$2/online"

# CALLS_CNT=16：调用深度 / 次数
clang -DBTB -DCALLS_CNT=16 tte_rsb.c -o tte_rsb $4
# 强制程序运行在 CORE1;$4 出现在命令末尾：对运行程序几乎无意义
taskset -c $1 ./tte_rsb > $3/btb_16_calls.txt $4

clang -DBTB -DCALLS_CNT=32 tte_rsb.c -o tte_rsb $4

taskset -c $1 ./tte_rsb > $3/btb_32_calls.txt $4

clang -DRSB -DCALLS_CNT=16 tte_rsb.c -o tte_rsb $4

taskset -c $1 ./tte_rsb > $3/rsb_16_calls.txt $4

clang -DRSB -DCALLS_CNT=32 tte_rsb.c -o tte_rsb $4

taskset -c $1 ./tte_rsb > $3/rsb_32_calls.txt $4

clang -DPHT -DCALLS_CNT=16 tte_rsb.c -o tte_rsb $4

taskset -c $1 ./tte_rsb > $3/pht_16_calls.txt $4

clang -DPHT -DCALLS_CNT=32 tte_rsb.c -o tte_rsb $4

taskset -c $1 ./tte_rsb > $3/pht_32_calls.txt $4

sudo bash -c "echo 1 > /sys/devices/system/cpu/cpu$2/online"
