#!/bin/bash

kill_service() {
       local SERVER_NAME="$1"
	PID=$(pidof "$SERVER_NAME")

	if [ -z "$PID" ]; then
	    echo "Process $SERVER_NAME is not running."
	fi

	echo "Process $SERVER_NAME (PID: $PID) found. Attempting to terminate..."
	kill -TERM "$PID"
	sleep 2  # 等待进程响应 SIGTERM

	# 再次检查进程是否仍在运行
	if pgrep -f "$SERVER_NAME" > /dev/null; then
	    echo "Process did not terminate. Sending SIGKILL..."
	    # 发送 SIGKILL 信号
	    kill -KILL "$PID"
	    echo "Process $SERVER_NAME has been killed."
	else
	    echo "Process $SERVER_NAME terminated successfully."
	fi
}

# Main function
main() {
    # Upgrade
    echo "Starting postinst script execution"

    kill_service "dde-file-manager-server"
    kill_service "dde-file-manager-daemon"

    echo "Finished postinst script execution"
}

# Execute main function
main
