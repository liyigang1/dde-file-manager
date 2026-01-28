#!/bin/bash
#
# Coredump 批量分析脚本
# 功能：提取所有 coredump 的 gdb 堆栈信息，每个保存为单独文件
# 文件名格式：<时间>_<PID>_<进程名>.txt
#
# 使用方法：
#     ./dump_coredumps.sh              # 分析所有 coredump
#     ./dump_coredumps.sh --recent 10  # 只分析最近 10 个
#     ./dump_coredumps.sh --name dde-file-manager  # 只分析指定进程
#
# 依赖：
#     sudo apt install gdb systemd-coredump
#

set +e  # 不要因为命令失败就退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 默认输出目录
OUTPUT_DIR="${HOME}/压测日志/coredump_analysis"
RECENT_COUNT=0
FILTER_NAME=""
VERBOSE=0  # 是否输出详细信息（bt full, thread apply all bt）

# 打印帮助
print_help() {
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  --recent N       只分析最近 N 个 coredump"
    echo "  --name NAME      只分析进程名包含 NAME 的 coredump"
    echo "  --output DIR     指定输出目录 (默认: ~/压测日志/coredump_analysis)"
    echo "  --verbose, -v    输出详细信息（bt full, thread apply all bt）"
    echo "  --help           显示帮助"
    echo ""
    echo "示例:"
    echo "  $0                              # 分析所有 coredump（简洁模式）"
    echo "  $0 --recent 10                  # 分析最近 10 个"
    echo "  $0 --name dde-file-manager      # 只分析文件管理器的 coredump"
    echo "  $0 --verbose                    # 输出详细堆栈信息"
}

# 解析参数
while [[ $# -gt 0 ]]; do
    case $1 in
        --recent)
            RECENT_COUNT="$2"
            shift 2
            ;;
        --name)
            FILTER_NAME="$2"
            shift 2
            ;;
        --output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --verbose|-v)
            VERBOSE=1
            shift
            ;;
        --help)
            print_help
            exit 0
            ;;
        *)
            echo -e "${RED}未知参数: $1${NC}"
            print_help
            exit 1
            ;;
    esac
done

# 检查 coredumpctl 是否可用
check_coredumpctl() {
    if command -v coredumpctl &> /dev/null; then
        return 0
    else
        return 1
    fi
}

# 检查 gdb 是否可用
check_gdb() {
    if command -v gdb &> /dev/null; then
        return 0
    else
        echo -e "${RED}[错误] gdb 未安装，请运行: sudo apt install gdb${NC}"
        exit 1
    fi
}

# 使用 coredumpctl 分析
analyze_with_coredumpctl() {
    echo -e "${BLUE}[信息] 使用 coredumpctl 分析 coredump...${NC}"

    # 获取 coredump 列表
    DUMPS=$(coredumpctl list --no-pager 2>/dev/null | tail -n +2 | grep -v "^$" || true)

    if [[ -z "$DUMPS" ]]; then
        echo -e "${YELLOW}[警告] 没有找到任何 coredump${NC}"
        return
    fi

    # 过滤进程名
    if [[ -n "$FILTER_NAME" ]]; then
        DUMPS=$(echo "$DUMPS" | grep "$FILTER_NAME" || true)
        if [[ -z "$DUMPS" ]]; then
            echo -e "${YELLOW}[警告] 没有找到进程名包含 '$FILTER_NAME' 的 coredump${NC}"
            return
        fi
    fi

    # 限制数量
    if [[ "$RECENT_COUNT" -gt 0 ]]; then
        DUMPS=$(echo "$DUMPS" | tail -n "$RECENT_COUNT")
    fi

    # 创建输出目录
    mkdir -p "$OUTPUT_DIR"

    COUNT=0
    TOTAL=$(echo "$DUMPS" | wc -l)

    echo -e "${BLUE}[信息] 找到 $TOTAL 个 coredump，开始分析...${NC}"
    echo ""

    # 逐个分析
    while IFS= read -r line; do
        # 解析 coredumpctl 输出
        # 格式类似: Mon 2024-01-15 10:30:00 CST  12345  1000  1000  11 present /usr/bin/dde-file-manager

        # 提取时间、PID、进程名
        TIMESTAMP=$(echo "$line" | awk '{print $1" "$2" "$3}')
        PID=$(echo "$line" | awk '{print $5}')
        PROCNAME=$(echo "$line" | awk '{print $NF}' | xargs basename 2>/dev/null || echo "unknown")

        # 格式化时间用于文件名
        TIME_STR=$(echo "$TIMESTAMP" | sed 's/[: ]/_/g' | sed 's/[-]//g')

        # 生成文件名
        OUTFILE="${OUTPUT_DIR}/${TIME_STR}_${PID}_${PROCNAME}.txt"

        COUNT=$((COUNT + 1))
        echo -e "${GREEN}[$COUNT/$TOTAL]${NC} 分析: PID=$PID 进程=$PROCNAME"

        # 写入文件头
        {
            echo "=========================================="
            echo "Coredump 分析报告"
            echo "=========================================="
            echo "时间: $TIMESTAMP"
            echo "PID: $PID"
            echo "进程: $PROCNAME"
            echo "分析时间: $(date '+%Y-%m-%d %H:%M:%S')"
            echo "=========================================="
            echo ""
            echo "=== coredumpctl info ==="
            coredumpctl info "$PID" 2>/dev/null || echo "(无法获取详细信息)"
            echo ""
        } > "$OUTFILE"

        # 获取可执行文件路径
        EXEC_PATH=$(coredumpctl info "$PID" 2>/dev/null | grep -E "^\s+Executable:" | awk '{print $2}' | head -1)
        if [[ -z "$EXEC_PATH" ]]; then
            EXEC_PATH=$(coredumpctl info "$PID" 2>/dev/null | grep "Executable:" | awk '{print $2}' | head -1)
        fi

        # 提取 core dump 到临时文件
        CORE_TMP="/tmp/core_${PID}_$$"
        echo "  提取 core dump..."

        if coredumpctl dump "$PID" -o "$CORE_TMP" 2>/dev/null; then
            if [[ -s "$CORE_TMP" ]]; then
                echo "  运行 GDB 分析..."
                {
                    echo "=== 第一部分: GDB 加载信息 ==="
                    echo ""

                    if [[ -n "$EXEC_PATH" && -f "$EXEC_PATH" ]]; then
                        # 第一部分：GDB 加载时的初始信息（不执行任何命令）
                        timeout 60 gdb "$EXEC_PATH" "$CORE_TMP" --batch \
                            -ex "set pagination off" \
                            -ex "quit" 2>&1 || echo "(GDB 加载信息获取失败)"

                        echo ""
                        echo "=== 第二部分: bt 完整堆栈 ==="
                        echo ""

                        # 第二部分：执行 bt 命令
                        timeout 60 gdb "$EXEC_PATH" "$CORE_TMP" --batch \
                            -ex "set pagination off" \
                            -ex "bt" \
                            -ex "quit" 2>&1 || echo "(bt 命令执行失败)"

                        # 详细模式才输出第三、第四部分
                        if [[ "$VERBOSE" -eq 1 ]]; then
                            echo ""
                            echo "=== 第三部分: bt full 详细堆栈 ==="
                            echo ""

                            # 第三部分：执行 bt full 命令
                            timeout 60 gdb "$EXEC_PATH" "$CORE_TMP" --batch \
                                -ex "set pagination off" \
                                -ex "bt full" \
                                -ex "quit" 2>&1 || echo "(bt full 命令执行失败)"

                            echo ""
                            echo "=== 第四部分: 所有线程堆栈 ==="
                            echo ""

                            # 第四部分：所有线程堆栈
                            timeout 120 gdb "$EXEC_PATH" "$CORE_TMP" --batch \
                                -ex "set pagination off" \
                                -ex "thread apply all bt" \
                                -ex "quit" 2>&1 || echo "(thread apply all bt 命令执行失败)"
                        fi
                    else
                        echo "(可执行文件不存在: $EXEC_PATH)"
                    fi
                } >> "$OUTFILE" 2>&1

                rm -f "$CORE_TMP"
            else
                echo "  [警告] core dump 文件为空"
                echo "(core dump 文件为空)" >> "$OUTFILE"
            fi
        else
            echo "  [警告] 无法提取 core dump"
            echo "(无法提取 core dump)" >> "$OUTFILE"
        fi

        echo "  -> 保存到: $OUTFILE"

    done <<< "$DUMPS"

    echo ""
    echo -e "${GREEN}[完成] 分析了 $COUNT 个 coredump，结果保存在: $OUTPUT_DIR${NC}"
}

# 使用 /var/crash 目录分析（备用方案）
analyze_var_crash() {
    echo -e "${BLUE}[信息] 使用 /var/crash 目录分析 coredump...${NC}"

    CRASH_DIR="/var/crash"

    if [[ ! -d "$CRASH_DIR" ]]; then
        echo -e "${YELLOW}[警告] $CRASH_DIR 目录不存在${NC}"
        return
    fi

    # 查找 .crash 文件
    CRASH_FILES=$(find "$CRASH_DIR" -name "*.crash" -type f 2>/dev/null || true)

    if [[ -z "$CRASH_FILES" ]]; then
        echo -e "${YELLOW}[警告] 没有找到 .crash 文件${NC}"
        return
    fi

    # 过滤进程名
    if [[ -n "$FILTER_NAME" ]]; then
        CRASH_FILES=$(echo "$CRASH_FILES" | grep "$FILTER_NAME" || true)
    fi

    # 限制数量
    if [[ "$RECENT_COUNT" -gt 0 ]]; then
        CRASH_FILES=$(echo "$CRASH_FILES" | head -n "$RECENT_COUNT")
    fi

    mkdir -p "$OUTPUT_DIR"

    COUNT=0
    TOTAL=$(echo "$CRASH_FILES" | wc -l)

    echo -e "${BLUE}[信息] 找到 $TOTAL 个 crash 文件，开始分析...${NC}"

    while IFS= read -r crash_file; do
        [[ -z "$crash_file" ]] && continue

        # 从文件名提取信息
        FILENAME=$(basename "$crash_file")
        # 格式类似: _usr_bin_dde-file-manager.1000.crash
        PROCNAME=$(echo "$FILENAME" | sed 's/^_//' | sed 's/\..*$//' | tr '_' '/' | xargs basename 2>/dev/null || echo "unknown")

        # 获取文件修改时间
        MTIME=$(stat -c %Y "$crash_file" 2>/dev/null || echo "0")
        TIME_STR=$(date -d "@$MTIME" '+%Y%m%d_%H%M%S' 2>/dev/null || echo "unknown")

        # 尝试从文件中提取 PID
        PID=$(grep -a "^Pid:" "$crash_file" 2>/dev/null | awk '{print $2}' | head -1 || echo "unknown")

        OUTFILE="${OUTPUT_DIR}/${TIME_STR}_${PID}_${PROCNAME}.txt"

        COUNT=$((COUNT + 1))
        echo -e "${GREEN}[$COUNT/$TOTAL]${NC} 分析: $FILENAME"

        {
            echo "=========================================="
            echo "Crash 文件分析报告"
            echo "=========================================="
            echo "文件: $crash_file"
            echo "进程: $PROCNAME"
            echo "PID: $PID"
            echo "分析时间: $(date '+%Y-%m-%d %H:%M:%S')"
            echo "=========================================="
            echo ""
            echo "=== Crash 文件头信息 ==="
            head -100 "$crash_file" 2>/dev/null | strings || echo "(无法读取)"
            echo ""
            echo "=== 尝试提取 CoreDump 并分析 ==="
        } > "$OUTFILE"

        # 尝试提取和分析 core dump（如果 apport 格式）
        CORE_TMP="/tmp/core_${PID}_$$"
        if grep -q "^CoreDump:" "$crash_file" 2>/dev/null; then
            # 提取 base64 编码的 core dump
            sed -n '/^CoreDump:/,/^[A-Z]/p' "$crash_file" | tail -n +2 | head -n -1 | base64 -d > "$CORE_TMP" 2>/dev/null || true

            if [[ -s "$CORE_TMP" ]]; then
                # 获取可执行文件路径
                EXEC_PATH=$(grep -a "^ExecutablePath:" "$crash_file" 2>/dev/null | cut -d: -f2- | tr -d ' ' || echo "")

                if [[ -x "$EXEC_PATH" ]]; then
                    {
                        gdb "$EXEC_PATH" "$CORE_TMP" --batch \
                            -ex "set pagination off" \
                            -ex "thread apply all bt full" \
                            -ex "quit" 2>/dev/null || echo "(GDB 分析失败)"
                    } >> "$OUTFILE" 2>&1
                else
                    echo "(可执行文件不存在: $EXEC_PATH)" >> "$OUTFILE"
                fi
            fi
            rm -f "$CORE_TMP"
        else
            echo "(非标准 apport 格式，无法提取 core dump)" >> "$OUTFILE"
        fi

        echo "  -> 保存到: $OUTFILE"

    done <<< "$CRASH_FILES"

    echo ""
    echo -e "${GREEN}[完成] 分析了 $COUNT 个 crash 文件，结果保存在: $OUTPUT_DIR${NC}"
}

# 主函数
main() {
    echo "=========================================="
    echo "  Coredump 批量分析工具"
    echo "=========================================="
    echo ""

    check_gdb

    if check_coredumpctl; then
        analyze_with_coredumpctl
    else
        echo -e "${YELLOW}[警告] coredumpctl 不可用，尝试使用 /var/crash 目录${NC}"
        analyze_var_crash
    fi

    # 汇总输出
    echo ""
    echo "=========================================="
    if [[ -d "$OUTPUT_DIR" ]]; then
        FILE_COUNT=$(find "$OUTPUT_DIR" -name "*.txt" -type f 2>/dev/null | wc -l)
        echo -e "输出目录: ${GREEN}$OUTPUT_DIR${NC}"
        echo -e "分析文件数: ${GREEN}$FILE_COUNT${NC}"
        echo ""
        echo "文件列表:"
        ls -lh "$OUTPUT_DIR"/*.txt 2>/dev/null || echo "(无文件)"
    fi
    echo "=========================================="
}

main
