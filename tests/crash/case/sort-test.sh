#!/bin/bash
set -euo pipefail

# ===== 配置区（请按实际校准）=====
START_FM_COORD="123 1062"      # 文件管理器图标
SYS_DISK_COORD="677 536"       # 系统盘图标（双击）
MOD_TIME_HEADER="1117 237"     # “修改时间”列标题坐标（用于排序）
CLOSE_BTN_COORD="1483 196"     # 关闭按钮

DEFAULT_RUNS=3000
# ======================================

# 安装 xdotool（静默）
if ! command -v xdotool &> /dev/null; then
  sudo apt-get update -qq >/dev/null
  sudo apt-get install -y -qq xdotool >/dev/null
fi

# 检查 X11
if [ "${XDG_SESSION_TYPE:-}" != "x11" ]; then
  echo "❌ 仅支持 X11 会话"
  exit 1
fi

# 参数解析
RUNS=$DEFAULT_RUNS
while getopts "n:h" opt; do
  case $opt in
    n) RUNS=$OPTARG ;;
    h) echo "用法: $0 [-n 次数]"; exit 0 ;;
    *) exit 1 ;;
  esac
done

run_navigation_test() {
  local i=$1
  echo "▶ 第 $i 次导航与排序测试"

  # 1. 点击图标打开文件管理器
  echo "   • 点击桌面文件管理器图标"
  xdotool mousemove $START_FM_COORD click 1
  sleep 3

  # 2. 双击系统盘
  echo "   • 双击系统盘"
  xdotool mousemove $SYS_DISK_COORD click 1 click 1
  sleep 1.0

  # 3. 输入 u + Enter, b + Enter
  echo "   • 输入 'u' 并回车"
  xdotool type "u"
  sleep 0.2
  xdotool key Return
  sleep 0.8

  echo "   • 输入 'b' 并回车"
  xdotool type "b"
  sleep 0.2
  xdotool key Return
  sleep 0.8

  # 4. Ctrl+2 切换到列表视图
  echo "   • 切换到列表视图 (Ctrl+2)"
  xdotool key Ctrl+2
  sleep 1

  # 5. 点击“修改时间”列进行排序
  echo "   • 点击“修改时间”列标题进行排序"
  xdotool mousemove $MOD_TIME_HEADER click 1
  sleep 1.2

  # 6. Alt+Left 返回上一级
  echo "   • 返回上一级 (Alt+←)"
  xdotool key Alt+Left
  sleep 1.0

  # 7. 输入 l + Enter, → + Enter
  echo "   • 输入 's' 并回车"
  xdotool type "s"
  sleep 0.2
  xdotool key Return
  sleep 0.8

  # 8. 再次 Ctrl+2（确保列表视图）
  echo "   • 再次切换到列表视图 (Ctrl+2)"
  xdotool key Ctrl+2
  sleep 1

  # 9. 再次点击“修改时间”排序
  echo "   • 再次点击“修改时间”列标题"
  xdotool mousemove $MOD_TIME_HEADER click 1
  sleep 1

  # 10. 关闭窗口
  echo "   • 点击关闭按钮"
  xdotool mousemove $CLOSE_BTN_COORD click 1
  sleep 1.0

  # 清理残留
  #pkill -f dde-file-manager 2>/dev/null || true
}

# ===== 主流程 =====
echo "🚀 开始文件导航与排序测试"
echo "   执行 $RUNS 次完整流程"

for ((i=1; i<=RUNS; i++)); do
  run_navigation_test $i
done

echo "✅ 执行完毕"