#!/bin/bash
# ================================================================================
# DDE文件管理器 - 崩溃测试总控脚本
# ================================================================================
#
# 【功能说明】
#     自动按顺序执行 case 目录下的所有测试脚本（Python 和 Shell）
#     每个测试脚本执行完成后检查退出码，成功才继续执行下一个
#
# 【使用方法】
#     # 执行所有测试
#     ./run_case.sh
#
#     # 指定测试目录（默认为 ./case）
#     ./run_case.sh --dir /path/to/tests
#
#     # 只执行 Python 脚本
#     ./run_case.sh --type python
#
#     # 只执行 Shell 脚本
#     ./run_case.sh --type shell
#
#     # 遇到失败时继续执行（默认遇到失败停止）
#     ./run_case.sh --continue-on-error
#
#     # 显示详细信息
#     ./run_case.sh --verbose
#
# 【命令行参数】
#     --dir DIR              测试脚本目录，默认 ./case
#     --type TYPE            脚本类型过滤，可选: python|shell|all (默认: all)
#     --continue-on-error    遇到测试失败时继续执行后续测试
#     --verbose, -v          显示详细输出
#     --dry-run              仅显示将要执行的测试，不实际运行
#     -h, --help             显示帮助信息
#
# 【执行顺序】
#     按文件名字母顺序执行 case 目录下的测试脚本：
#     1. Shell 脚本 (*.sh)
#     2. Python 脚本 (*.py)
#
# 【退出码】
#     0  - 所有测试都通过
#     1  - 部分或全部测试失败
#     2  - 参数错误
#
# 【注意事项】
#     1. 确保所有测试脚本具有可执行权限
#     2. 每个测试脚本应该正确设置退出码（0=成功，非0=失败）
#     3. 建议在专用测试机器上运行
#     4. 需要图形界面环境
# ================================================================================

set -e

# ==================== 配置区 ====================
TEST_DIR="./case"
SCRIPT_TYPE="all"
CONTINUE_ON_ERROR=false
VERBOSE=false
DRY_RUN=false

# 统计变量
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
FAILED_TEST_NAMES=()

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# ==================== 工具函数 ====================

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[PASS]${NC} $1"
}

log_error() {
    echo -e "${RED}[FAIL]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_verbose() {
    if [[ "$VERBOSE" == "true" ]]; then
        echo -e "${CYAN}[VERBOSE]${NC} $1"
    fi
}

# 打印分隔线
print_separator() {
    local width="${1:-80}"
    local char="${2:-=}"
    printf "${CYAN}%*s${NC}\n" "$width" | tr ' ' "$char"
}

# 显示帮助
show_help() {
    cat << EOF
用法: $0 [选项]

选项:
  --dir DIR              测试脚本目录 (默认: ./case)
  --type TYPE            脚本类型过滤: python|shell|all (默认: all)
  --continue-on-error    遇到测试失败时继续执行后续测试
  --verbose, -v          显示详细输出
  --dry-run              仅显示将要执行的测试，不实际运行
  -h, --help             显示帮助信息

示例:
  $0                                    # 执行所有测试
  $0 --dir ./case --verbose             # 显示详细信息
  $0 --type python                      # 只执行 Python 脚本
  $0 --type shell --continue-on-error   # 只执行 Shell 脚本，失败继续

测试执行顺序:
  按文件名字母顺序执行指定目录下的测试脚本
EOF
}

# 解析参数
parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --dir)
                TEST_DIR="$2"
                shift 2
                ;;
            --type)
                SCRIPT_TYPE="$2"
                shift 2
                ;;
            --continue-on-error)
                CONTINUE_ON_ERROR=true
                shift
                ;;
            --verbose|-v)
                VERBOSE=true
                shift
                ;;
            --dry-run)
                DRY_RUN=true
                shift
                ;;
            -h|--help)
                show_help
                exit 0
                ;;
            *)
                echo -e "${RED}错误: 未知参数 '$1'${NC}"
                echo "使用 -h 或 --help 查看帮助信息"
                exit 2
                ;;
        esac
    done
}

# 检查目录是否存在
check_test_dir() {
    if [[ ! -d "$TEST_DIR" ]]; then
        log_error "测试目录不存在: $TEST_DIR"
        exit 2
    fi
}

# 获取测试脚本列表
get_test_scripts() {
    local scripts=()

    # Shell 脚本
    if [[ "$SCRIPT_TYPE" == "all" || "$SCRIPT_TYPE" == "shell" ]]; then
        while IFS= read -r -d '' script; do
            scripts+=("$script")
        done < <(find "$TEST_DIR" -maxdepth 1 -type f -name "*.sh" -print0 | sort -z)
    fi

    # Python 脚本
    if [[ "$SCRIPT_TYPE" == "all" || "$SCRIPT_TYPE" == "python" ]]; then
        while IFS= read -r -d '' script; do
            scripts+=("$script")
        done < <(find "$TEST_DIR" -maxdepth 1 -type f -name "*.py" -print0 | sort -z)
    fi

    echo "${scripts[@]}"
}

# 执行单个测试
run_test() {
    local script="$1"
    local script_name=$(basename "$script")
    local script_ext="${script_name##*.}"
    local exit_code

    log_info "========================================"
    log_info "执行测试: $script_name"
    log_info "========================================"

    if [[ "$DRY_RUN" == "true" ]]; then
        log_verbose "[DRY-RUN] 将执行: $script"
        echo ""
        return 0
    fi

    # 记录开始时间
    local start_time=$(date +%s)

    # 根据脚本类型执行（直接显示输出）
    if [[ "$script_ext" == "sh" ]]; then
        bash "$script"
        exit_code=$?
    elif [[ "$script_ext" == "py" ]]; then
        python3 "$script"
        exit_code=$?
    else
        log_warn "未知的脚本类型: $script_ext"
        return 1
    fi

    # 计算耗时
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))

    # 检查退出码
    if [[ $exit_code -eq 0 ]]; then
        log_success "$script_name 测试通过 (耗时: ${duration}秒)"
        ((PASSED_TESTS++)) || true
        echo ""
        return 0
    else
        log_error "$script_name 测试失败 (退出码: $exit_code, 耗时: ${duration}秒)"
        ((FAILED_TESTS++)) || true
        FAILED_TEST_NAMES+=("$script_name")
        echo ""

        if [[ "$CONTINUE_ON_ERROR" == "false" ]]; then
            return 1
        fi
        return 0
    fi
}

# 打印测试摘要
print_summary() {
    echo ""
    print_separator 80 "="
    echo -e "${CYAN}  测试执行摘要${NC}"
    print_separator 80 "="
    echo "  总测试数: $TOTAL_TESTS"
    echo -e "  ${GREEN}通过: $PASSED_TESTS${NC}"
    echo -e "  ${RED}失败: $FAILED_TESTS${NC}"

    if [[ ${#FAILED_TEST_NAMES[@]} -gt 0 ]]; then
        echo ""
        echo -e "${RED}失败的测试:${NC}"
        for name in "${FAILED_TEST_NAMES[@]}"; do
            echo "  - $name"
        done
    fi

    print_separator 80 "="

    # 返回适当的退出码
    if [[ $FAILED_TESTS -gt 0 ]]; then
        return 1
    fi
    return 0
}

# ==================== 主函数 ====================
main() {
    # 解析参数
    parse_args "$@"

    # 打印标题
    print_separator 80 "="
    echo -e "${CYAN}  DDE文件管理器 - 崩溃测试总控脚本${NC}"
    print_separator 80 "="
    echo ""
    log_info "测试目录: $TEST_DIR"
    log_info "脚本类型: $SCRIPT_TYPE"
    log_info "遇到失败继续: $CONTINUE_ON_ERROR"
    log_info "详细输出: $VERBOSE"
    log_info "仅模拟运行: $DRY_RUN"
    echo ""

    # 检查目录
    check_test_dir

    # 获取测试脚本列表
    log_info "正在扫描测试脚本..."
    local scripts=($(get_test_scripts))
    TOTAL_TESTS=${#scripts[@]}

    if [[ $TOTAL_TESTS -eq 0 ]]; then
        log_warn "没有找到任何测试脚本"
        exit 0
    fi

    log_info "找到 $TOTAL_TESTS 个测试脚本"
    echo ""

    # 显示将要执行的测试
    log_info "测试执行顺序:"
    for i in "${!scripts[@]}"; do
        local script_name=$(basename "${scripts[$i]}")
        echo "  $((i+1)). $script_name"
    done
    echo ""

    # 如果是 dry-run 模式，直接退出
    if [[ "$DRY_RUN" == "true" ]]; then
        log_info "DRY-RUN 模式，不实际执行测试"
        print_summary
        exit $?
    fi

    # 确认是否执行
    if [[ "$CONTINUE_ON_ERROR" == "false" ]]; then
        log_warn "注意: 遇到测试失败将立即停止（使用 --continue-on-error 可继续执行）"
    fi

    log_info "开始执行测试..."
    echo ""

    # 执行所有测试
    for script in "${scripts[@]}"; do
        if ! run_test "$script"; then
            # 遇到失败，如果不继续执行则退出
            if [[ "$CONTINUE_ON_ERROR" == "false" ]]; then
                log_error "测试失败，停止执行后续测试"
                print_summary
                exit 1
            fi
        fi
    done

    # 打印摘要
    print_summary
    exit $?
}

# 运行主函数
main "$@"
