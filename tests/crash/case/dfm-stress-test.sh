#!/bin/bash

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
KILL_SCRIPT="$SCRIPT_DIR/dfm-kill-service.sh"
LOG_FILE="$SCRIPT_DIR/stress-test.log"
TOTAL=1000

if [ -z "$1" ]; then
    echo "Usage: sudo $0 <username>"
    exit 1
fi

USER="$1"

# 获取目标用户的环境变量
get_user_env() {
    USER_PID=$(pgrep -u "$USER" -x "dde-desktop" | head -1)
    if [ -z "$USER_PID" ]; then
        USER_PID=$(pgrep -u "$USER" -x "dde-file-manager" | head -1)
    fi
    if [ -z "$USER_PID" ]; then
        echo "[ERROR] No dde process found for user $USER" | tee -a "$LOG_FILE"
        return 1
    fi

    export DISPLAY=$(cat /proc/$USER_PID/environ 2>/dev/null | tr '\0' '\n' | grep "^DISPLAY=" | cut -d= -f2)
    export DBUS_SESSION_BUS_ADDRESS=$(cat /proc/$USER_PID/environ 2>/dev/null | tr '\0' '\n' | grep "^DBUS_SESSION_BUS_ADDRESS=" | cut -d= -f2)
    export XAUTHORITY=$(cat /proc/$USER_PID/environ 2>/dev/null | tr '\0' '\n' | grep "^XAUTHORITY=" | cut -d= -f2)
    return 0
}

echo "=== Stress test started at $(date) ===" | tee "$LOG_FILE"
echo "Total iterations: $TOTAL" | tee -a "$LOG_FILE"
echo "Target user: $USER" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

for i in $(seq 1 $TOTAL); do
    echo "[Round $i/$TOTAL] $(date '+%H:%M:%S')" | tee -a "$LOG_FILE"

    # 1. 调用 kill 脚本
    echo "  [1] Calling kill script..." | tee -a "$LOG_FILE"
    bash "$KILL_SCRIPT" >> "$LOG_FILE" 2>&1

    # 2. 杀死 dde-file-manager
    echo "  [2] Killing dde-file-manager..." | tee -a "$LOG_FILE"
    kill -KILL $(pidof dde-file-manager) 2>/dev/null

    # 3. 以用户身份启动 dde-file-manager
    echo "  [3] Starting dde-file-manager as $USER..." | tee -a "$LOG_FILE"
    get_user_env
    if [ $? -eq 0 ]; then
        sudo -u "$USER" DISPLAY="$DISPLAY" DBUS_SESSION_BUS_ADDRESS="$DBUS_SESSION_BUS_ADDRESS" XAUTHORITY="$XAUTHORITY" \
            nohup /usr/bin/dde-file-manager > /dev/null 2>&1 &
    fi

    sleep 1

    echo "  [Done] Round $i completed" | tee -a "$LOG_FILE"
    echo "" | tee -a "$LOG_FILE"
done

echo "=== Stress test finished at $(date) ===" | tee -a "$LOG_FILE"
echo "Log: $LOG_FILE" | tee -a "$LOG_FILE"
