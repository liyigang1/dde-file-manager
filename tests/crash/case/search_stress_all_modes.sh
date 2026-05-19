#!/usr/bin/env bash
# search_stress_all_modes.sh
# 循环执行 search_stress_test.py 所有 7 种测试模式, 指定轮次
# 通过 coredumpctl 检测隐藏的崩溃 (halt_on_error=0 时进程不中止但会产生 coredump)
#
# 用法:
#   ./search_stress_all_modes.sh [每模式循环次数] [外层总循环次数]
#   ./search_stress_all_modes.sh          # 默认: 每模式 10 次, 外层 100 次
#   ./search_stress_all_modes.sh 5 50     # 每模式 5 次, 外层 50 次
#
# 日志: /tmp/search_stress_all_modes/<mode>_<outer>_<inner>.log
# 崩溃汇总: /tmp/search_stress_all_modes/crash_reports/
# 汇总: /tmp/search_stress_all_modes/summary.log

set -euo pipefail

MODES=("gui" "url" "multi" "loop" "mixed" "inprocess" "windows")

INNER_LOOPS="${1:-10}"
OUTER_LOOPS="${2:-100}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TEST_SCRIPT="${SCRIPT_DIR}/search_stress_test.py"
LOG_DIR="/tmp/search_stress_all_modes"
CRASH_DIR="${LOG_DIR}/crash_reports"
SUMMARY_LOG="${LOG_DIR}/summary.log"

mkdir -p "${LOG_DIR}" "${CRASH_DIR}"
: > "${SUMMARY_LOG}"

if ! command -v python3 &>/dev/null; then
    echo "[ERROR] python3 未找到" >&2
    exit 1
fi

if [[ ! -f "${TEST_SCRIPT}" ]]; then
    echo "[ERROR] search_stress_test.py 未找到: ${TEST_SCRIPT}" >&2
    exit 1
fi

total=$((OUTER_LOOPS * ${#MODES[@]}))
passed=0
crashed=0
env_fail=0
skipped=0
current=0
has_coredumpctl=false

if command -v coredumpctl &>/dev/null; then
    has_coredumpctl=true
fi

ts() { date '+%Y-%m-%d %H:%M:%S'; }

# 记录当前 coredump 数量 (从 systemd 读取)
get_coredump_count() {
    if [[ "${has_coredumpctl}" == "true" ]]; then
        coredumpctl list --no-pager 2>/dev/null | wc -l
    else
        echo 0
    fi
}

# 检查是否有新的 dde-file-manager coredump 产生, 导出崩溃信息
check_new_coredumps() {
    local mode="$1" outer="$2"
    local prev_count="$3"
    local new_count
    new_count=$(get_coredump_count)

    if [[ "${new_count}" -gt "${prev_count}" ]]; then
        local diff=$((new_count - prev_count))
        echo "  [!] 检测到 ${diff} 个新 coredump"

        # 导出最新的崩溃信息
        local crash_file="${CRASH_DIR}/${mode}_outer${outer}_$(date +%s).txt"
        {
            echo "=== coredump report: ${mode} outer=${outer} ==="
            echo "时间: $(ts)"
            echo "新增 coredump 数: ${diff}"
            echo ""
            # 导出最新的几个崩溃的简要信息
            coredumpctl list --no-pager 2>/dev/null | grep "dde-file-manager" | tail -"${diff}" | while read -r line; do
                pid=$(echo "${line}" | awk '{print $5}')
                if [[ -n "${pid}" ]]; then
                    echo "--- PID ${pid} ---"
                    coredumpctl info "${pid}" --no-pager 2>/dev/null | head -50
                    echo ""
                fi
            done
        } > "${crash_file}"
        echo "  [!] 崩溃详情: ${crash_file}"
        return 1
    fi
    return 0
}

# 判断日志是否包含真正的崩溃 (ASan 错误 / signal / 堆损坏等)
is_real_crash() {
    local log_file="$1"
    grep -qiE "AddressSanitizer|DEADLYSIGNAL|heap-buffer-overflow|heap-use-after-free|double-free|stack-buffer-overflow|Segmentation fault|Signal [0-9]+ \(SIG|glibc detected|pure virtual method|std::terminate" "${log_file}" 2>/dev/null
}

run_mode() {
    local mode="$1" outer="$2" inner="$3"
    local log_file="${LOG_DIR}/${mode}_${outer}_${inner}.log"

    current=$((current + 1))
    printf "[%.3d/%d] %s outer=%d inner=%d ... " \
           "${current}" "${total}" "${mode}" "${outer}" "${inner}"

    case "${mode}" in
        gui)
            python3 "${TEST_SCRIPT}" --mode gui --loops "${INNER_LOOPS}" \
                --search-wait 10 --dfm-bin /bin/dde-file-manager >> "${log_file}" 2>&1
            ;;
        url)
            python3 "${TEST_SCRIPT}" --mode url --loops "${INNER_LOOPS}" \
                --search-wait 10 --dfm-bin /bin/dde-file-manager >> "${log_file}" 2>&1
            ;;
        multi)
            python3 "${TEST_SCRIPT}" --mode multi --instances 3 \
                --duration 30 --search-interval 2 --dfm-bin /bin/dde-file-manager >> "${log_file}" 2>&1
            ;;
        loop)
            python3 "${TEST_SCRIPT}" --mode loop --loops "${INNER_LOOPS}" \
                --search-wait 10 --dfm-bin /bin/dde-file-manager >> "${log_file}" 2>&1
            ;;
        mixed)
            python3 "${TEST_SCRIPT}" --mode mixed --loops "${INNER_LOOPS}" \
                --duration 30 --dfm-bin /bin/dde-file-manager >> "${log_file}" 2>&1
            ;;
        inprocess)
            python3 "${TEST_SCRIPT}" --mode inprocess --loops "${INNER_LOOPS}" \
                --plugin-duration  --dfm-bin /bin/dde-file-manager >> "${log_file}" 2>&1
            ;;
        windows)
            python3 "${TEST_SCRIPT}" --mode windows --windows 3 \
                --loops "${INNER_LOOPS}" --search-wait 10  --dfm-bin /bin/dde-file-manager >> "${log_file}" 2>&1
            ;;
        *)
            echo "SKIP (unknown mode)"
            skipped=$((skipped + 1))
            return 0
            ;;
    esac
}

record_result() {
    local mode="$1" outer="$2" inner="$3" rc="$4" log_file="$5" has_coredump="$6"

    if [[ ${rc} -eq 0 && "${has_coredump}" == "false" ]]; then
        echo "PASS"
        passed=$((passed + 1))
        echo "$(ts) PASS  mode=${mode} outer=${outer} inner=${inner}" >> "${SUMMARY_LOG}"
        rm -f "${log_file}"
    elif is_real_crash "${log_file}" || [[ "${has_coredump}" == "true" ]]; then
        local reason=""
        if is_real_crash "${log_file}"; then
            reason="日志检出崩溃"
        elif [[ "${has_coredump}" == "true" ]]; then
            reason="coredumpctl 检出新 coredump"
        fi
        echo "CRASH (${reason}, rc=${rc})"
        crashed=$((crashed + 1))
        echo "$(ts) CRASH mode=${mode} outer=${outer} inner=${inner} ${reason} log=${log_file}" >> "${SUMMARY_LOG}"
    else
        echo "ENV-FAIL (rc=${rc}, no crash signature) -> ${log_file}"
        env_fail=$((env_fail + 1))
        echo "$(ts) ENV-FAIL mode=${mode} outer=${outer} inner=${inner} log=${log_file}" >> "${SUMMARY_LOG}"
        rm -f "${log_file}"
    fi
}

echo "=============================================="
echo " search_stress_test.py 全模式循环压测"
echo "=============================================="
echo "  模式数量:     ${#MODES[@]}"
echo "  外层循环:     ${OUTER_LOOPS}"
echo "  内层循环:     ${INNER_LOOPS}"
echo "  总任务数:     ${total}"
echo "  日志目录:     ${LOG_DIR}"
echo "  崩溃报告:     ${CRASH_DIR}"
echo "  coredumpctl:  ${has_coredumpctl}"
echo "  开始时间:     $(ts)"
echo "=============================================="
echo ""

# 记录起始 coredump 数量
start_coredump_count=$(get_coredump_count)

trap 'echo -e "\n[中断] Ctrl+C, 打印汇总:"; tail -20 "${SUMMARY_LOG}" 2>/dev/null; exit 130' INT

for outer in $(seq 1 "${OUTER_LOOPS}"); do
    echo "====== Outer ${outer}/${OUTER_LOOPS} ======"

    for mode in "${MODES[@]}"; do
        log_file="${LOG_DIR}/${mode}_${outer}_${INNER_LOOPS}.log"

        # 记录运行前的 coredump 数量
        prev_coredump_count=$(get_coredump_count)

        set +e
        run_mode "${mode}" "${outer}" "${INNER_LOOPS}" > "${log_file}" 2>&1
        rc=$?
        set -e

        # 检查是否产生了新的 coredump
        new_coredump=false
        if [[ "${has_coredumpctl}" == "true" ]]; then
            set +e
            check_new_coredumps "${mode}" "${outer}" "${prev_coredump_count}"
            has_new=$?
            set -e
            if [[ ${has_new} -ne 0 ]]; then
                new_coredump=true
            fi
        fi

        record_result "${mode}" "${outer}" "${INNER_LOOPS}" "${rc}" "${log_file}" "${new_coredump}"
    done

    echo ""
done

echo "=============================================="
echo " 压测结束"
echo "=============================================="
echo "  结束时间:   $(ts)"
echo "  总计:       ${total}"
echo "  通过:       ${passed}"
echo "  崩溃:       ${crashed}"
echo "  环境异常:   ${env_fail} (wmctrl/xdotool 超时等, 非崩溃)"
echo "  跳过:       ${skipped}"
echo ""
echo "  汇总日志:   ${SUMMARY_LOG}"
if [[ "${has_coredumpctl}" == "true" ]]; then
    end_coredump_count=$(get_coredump_count)
    new_total=$((end_coredump_count - start_coredump_count))
    echo "  新增 coredump: ${new_total}"
fi
if [[ -d "${CRASH_DIR}" ]] && [[ -n "$(ls -A "${CRASH_DIR}" 2>/dev/null)" ]]; then
    echo "  崩溃报告:     ${CRASH_DIR}/"
    ls -1 "${CRASH_DIR}" | while read -r f; do echo "    - ${f}"; done
fi
echo "=============================================="
