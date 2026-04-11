#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
================================================================================
DDE文件管理器 - 双窗口双击崩溃测试脚本（2次循环，步骤2-6）
================================================================================

【场景描述】
    测试dde-file-manager的键盘快捷键和双窗口连续双击功能。
    创建大量测试文件，在每次循环中打开两个窗口，分别在窗口的不同位置
    执行双击操作，监控崩溃情况。

【测试内容】
    1. 在~/Documents目录下创建_crash_sort目录，再在_crash_sort下创建_crash_sort目录
    2. 在最新创建的嵌套目录下创建10000个10KB以下的随机文件
    3. 将~/Documents/_crash_sort/_crash_sort转换为绝对路径的url并使用dde-file-manager -n打开
    4. 找到打开的窗口，记录当前窗口的rect，使用快捷键ctrl+2
    5. 移动鼠标到rect右上角的坐标 + (-60, 70)的坐标点左键点击
    6. 等待0.2秒，移动鼠标到rect左上角的坐标 + (100, 110)的坐标点左键点击
    7. 关闭当前窗口
    8. 将~/Documents/_crash_sort/_crash_sort转换为绝对路径的url并使用dde-file-manager -n打开
    9. 找到打开的窗口，记录当前窗口的rect，使用快捷键ctrl+2
    10. 移动鼠标到rect左上角的坐标 + (250, 70)的坐标点左键点击
    11. 等待0.2秒，移动鼠标到rect左上角的坐标 + (100, 110)的坐标点左键点击
    12. 关闭当前窗口
    13. 检查/var/lib/systemd/coredump目录下是否有文件名称包含"dde-file-m"的文件
    14. 循环执行步骤2-13共1000次
    15. 删除~/Documents/_crash_sort并退出

【使用方法】
    python3 test_dual_window_crash.py

【输出文件】
    日志文件：~/压测日志/dual_window_crash_test_YYYYMMDD_HHMMSS.log

【依赖项】
    系统工具：xdotool, wmctrl
    Python库：标准库

【注意事项】
    1. 需要图形界面环境
    2. 需要xdotool、wmctrl工具
    3. 脚本执行期间不要操作鼠标键盘
    4. 脚本会删除测试目录，请确保数据安全
    5. 建议在测试机上运行
    6. 需要systemd-coredump服务运行以捕获崩溃
================================================================================
"""

import os
import subprocess
import time
import random
import datetime
import logging
import shutil
from urllib.parse import quote
from pathlib import Path
import sys

# ==================== 配置区 ====================
CONFIG = {
    "dde_file_manager": "/usr/bin/dde-file-manager",
    "test_dir_name": "_crash_sort",
    "num_files": 10000,
    "max_file_size": 10240 - 1,   # 小于10KB (10KB - 1 byte)
    "min_file_size": 512,         # 最小512B
    "documents_dir": os.path.expanduser("~/Documents"),
    "log_dir": os.path.expanduser("~/压测日志"),
    "loop_count": 2,           # 循环次数
	"loop_delay": 3,           # 循环次数
    "fm_open_delay": 1,           # 等待文件管理器打开
    "window_activate_delay": 1,   # 等待窗口激活
    "mouse_move_delay": 0.1,      # 鼠标移动延迟
    "click_delay": 0.2,           # 鼠标点击延迟
    "action_delay": 0.5,          # 动作间隔
    "click_wait_delay": 0.2,      # 两次点击之间的等待时间（0.5秒）
    # 第一个窗口的点击偏移（相对于右上角）
    "window1_first_click_offset_x": -60,
    "window1_first_click_offset_y": 70,
    "window1_second_click_offset_x": 100,  # 相对于左上角
    "window1_second_click_offset_y": 110,
    # 第二个窗口的点击偏移（相对于左上角）
    "window2_first_click_offset_x": 250,
    "window2_first_click_offset_y": 70,
    "window2_second_click_offset_x": 100,
    "window2_second_click_offset_y": 110,
    "coredump_dir": "/var/lib/systemd/coredump",  # coredump目录
    "script_dir": os.path.dirname(os.path.abspath(__file__)),  # 脚本所在目录
}

# 全局变量：跟踪脚本打开的窗口
script_opened_windows = []

# ==================== 日志配置 ====================
def setup_logging():
    """
    设置日志系统
    """
    log_dir = CONFIG["log_dir"]
    os.makedirs(log_dir, exist_ok=True)

    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    log_file = os.path.join(log_dir, f"dual_window_crash_test_{timestamp}.log")

    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s - %(levelname)s - %(message)s',
        handlers=[
            logging.FileHandler(log_file, encoding='utf-8'),
            logging.StreamHandler(sys.stdout)
        ]
    )

    logging.info(f"日志文件: {log_file}")
    return log_file

# ==================== 文件创建 ====================
def create_test_files_with_nested_dir(base_dir, dir_name, num_files, max_size):
    """
    创建嵌套测试目录和文件

    目录结构：
    base_dir/dir_name/dir_name/ (嵌套)
    """
    logging.info("=" * 70)
    logging.info("步骤1: 创建测试数据")
    logging.info("=" * 70)

    # 创建第一层目录
    first_level_dir = os.path.join(base_dir, dir_name)
    try:
        os.makedirs(first_level_dir, exist_ok=True)
        logging.info(f"✓ 创建第一层目录: {first_level_dir}")
    except Exception as e:
        logging.error(f"✗ 创建第一层目录失败: {e}")
        return None

    # 创建第二层嵌套目录
    nested_dir = os.path.join(first_level_dir, dir_name)
    try:
        os.makedirs(nested_dir, exist_ok=True)
        logging.info(f"✓ 创建第二层嵌套目录: {nested_dir}")
    except Exception as e:
        logging.error(f"✗ 创建第二层嵌套目录失败: {e}")
        return None

    # 在嵌套目录下创建文件
    logging.info(f"创建 {num_files} 个随机文件...")
    created_count = 0

    for i in range(num_files):
        try:
            file_size = random.randint(CONFIG["min_file_size"], max_size)
            filename = f"test_file_{i:05d}.dat"
            file_path = os.path.join(nested_dir, filename)

            with open(file_path, 'wb') as f:
                random_data = os.urandom(file_size)
                f.write(random_data)

            created_count += 1

            if (i + 1) % 1000 == 0:
                logging.info(f"已创建 {i + 1}/{num_files} 个文件")

        except Exception as e:
            logging.error(f"创建文件 {filename} 失败: {e}")

    logging.info(f"✓ 成功创建 {created_count}/{num_files} 个文件")
    logging.info(f"✓ 测试目录创建完成: {nested_dir}")

    return nested_dir

# ==================== 窗口管理 ====================
def find_all_windows():
    """
    查找所有窗口
    """
    windows = []

    try:
        result = subprocess.run(['wmctrl', '-l'], capture_output=True, text=True)

        if result.returncode != 0:
            logging.warning("wmctrl命令执行失败")
            return windows

        lines = result.stdout.strip().split('\n')

        for line in lines:
            parts = line.split(None, 5)
            if len(parts) < 5:
                continue

            window_id = parts[0]
            title = parts[4]

            if window_id.startswith('0x'):
                desktop = parts[1]
                window_class = parts[3] if len(parts) > 3 else ""

                windows.append({
                    "window_id": window_id,
                    "title": title,
                    "class": window_class,
                    "desktop": desktop
                })

    except subprocess.CalledProcessError as e:
        logging.error(f"执行wmctrl失败: {e}")
    except Exception as e:
        logging.error(f"查找窗口时出错: {e}")

    return windows

def get_window_geometry(window_id):
    """
    获取窗口几何信息（位置和大小）

    返回格式: {"x": x, "y": y, "width": width, "height": height}
    """
    geometry_info = {"x": 0, "y": 0, "width": 0, "height": 0}

    try:
        result = subprocess.run(
            ['xdotool', 'getwindowgeometry', window_id],
            capture_output=True,
            text=True
        )

        if result.returncode == 0:
            # 解析输出格式：
            # Window 12345678
            #   Position: 100,200 (screen: 0)
            #   Geometry: 800x600
            for line in result.stdout.strip().split('\n'):
                if 'Geometry:' in line:
                    geometry = line.split('Geometry:')[1].strip()
                    width, height = map(int, geometry.split('x'))
                    geometry_info["width"] = width
                    geometry_info["height"] = height
                elif 'Position:' in line:
                    position = line.split('Position:')[1].split()[0].strip()
                    x, y = map(int, position.split(','))
                    geometry_info["x"] = x
                    geometry_info["y"] = y

            logging.info(f"窗口几何信息: x={geometry_info['x']}, y={geometry_info['y']}, "
                        f"width={geometry_info['width']}, height={geometry_info['height']}")
    except Exception as e:
        logging.error(f"获取窗口几何信息失败: {e}")

    return geometry_info

def activate_window(window_id):
    """
    激活指定窗口
    """
    logging.info(f"激活窗口: {window_id}")

    try:
        subprocess.run(['wmctrl', '-ia', window_id], check=True)
        time.sleep(CONFIG["window_activate_delay"])
        return True
    except subprocess.CalledProcessError as e:
        logging.error(f"激活窗口失败: {e}")
        return False

def send_key_combo(key):
    """
    发送键盘组合键
    """
    logging.info(f"发送按键: {key}")
    time.sleep(0.1)

    try:
        subprocess.run(['xdotool', 'key', key], check=True)
        time.sleep(CONFIG["action_delay"])
        return True
    except subprocess.CalledProcessError as e:
        logging.error(f"发送按键失败: {e}")
        return False

def move_mouse(x, y):
    """
    移动鼠标到指定坐标
    """
    logging.info(f"移动鼠标到坐标: ({x}, {y})")

    try:
        subprocess.run(['xdotool', 'mousemove', str(x), str(y)], check=True)
        time.sleep(CONFIG["mouse_move_delay"])
        return True
    except subprocess.CalledProcessError as e:
        logging.error(f"移动鼠标失败: {e}")
        return False

def mouse_click(button=1):
    """
    鼠标点击（button: 1=左键, 2=中键, 3=右键）
    """
    button_name = "左键" if button == 1 else ("中键" if button == 2 else "右键")
    logging.info(f"执行鼠标{button_name}点击")

    try:
        subprocess.run(['xdotool', 'click', str(button)], check=True)
        time.sleep(CONFIG["click_delay"])
        return True
    except subprocess.CalledProcessError as e:
        logging.error(f"鼠标点击失败: {e}")
        return False

def close_window(window_id):
    """
    关闭指定的窗口
    """
    logging.info(f"关闭窗口: {window_id}")

    try:
        subprocess.run(['wmctrl', '-ic', window_id], check=True)
        logging.info(f"✓ 已关闭窗口: {window_id}")
        return True
    except subprocess.CalledProcessError as e:
        logging.error(f"✗ 关闭窗口失败: {window_id} - {e}")
        return False

def open_directory_with_fm(url, wait_time=None):
    """
    使用dde-file-manager -n在新窗口中打开目录URL，并返回窗口对象
    """
    global script_opened_windows

    if wait_time is None:
        wait_time = CONFIG["fm_open_delay"]

    logging.info(f"使用dde-file-manager -n打开URL: {url}")

    try:
        proc = subprocess.Popen(
            [CONFIG["dde_file_manager"], '-n', url],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )

        time.sleep(wait_time)

        if proc.poll() is None:
            logging.info("dde-file-manager 已在新窗口启动")
        else:
            logging.info(f"dde-file-manager 进程 (PID: {proc.pid}) 已退出")

        # 等待窗口出现并获取窗口信息
        time.sleep(CONFIG["fm_open_delay"])

        # 查找最新打开的窗口
        windows = find_all_windows()
        if windows:
            new_window = windows[-1]  # 最新窗口
            # 检查是否已跟踪过
            if new_window['window_id'] not in [w['window_id'] for w in script_opened_windows]:
                script_opened_windows.append(new_window)
                logging.info(f"✓ 已跟踪窗口: {new_window['window_id']} - {new_window['title']}")

        return proc

    except Exception as e:
        logging.error(f"打开dde-file-manager失败: {e}")
        return None

def perform_window_test(nested_dir, window_num, script_start_time):
    """
    执行单个窗口的测试步骤

    window_num: 1 或 2，表示是第1个窗口还是第2个窗口
    """
    logging.info("=" * 70)
    logging.info(f"执行第 {window_num} 个窗口的测试")
    logging.info("=" * 70)

    # 步骤: 打开目录
    nested_url = f"file://{quote(nested_dir)}"
    logging.info(f"步骤: 打开第 {window_num} 个目录")

    open_directory_with_fm(nested_url)

    time.sleep(CONFIG["fm_open_delay"])

    # 找到打开的窗口
    windows = find_all_windows()
    if not windows:
        logging.error("未找到任何窗口")
        return False

    window = windows[-1]  # 最新窗口
    window_id = window['window_id']
    logging.info(f"找到窗口: {window_id} - {window['title']}")

    # 激活窗口
    activate_window(window_id)

    # 记录当前窗口的rect
    logging.info("步骤: 记录当前窗口的rect（矩形坐标）")

    geometry = get_window_geometry(window_id)

    if geometry["width"] == 0 or geometry["height"] == 0:
        logging.error("无法获取有效的窗口几何信息")
        return False

    # 使用快捷键ctrl+2
    logging.info("步骤: 使用快捷键ctrl+2")

    send_key_combo('ctrl+2')

    time.sleep(CONFIG["action_delay"])

    # 根据窗口类型选择不同的点击策略
    if window_num == 1:
        # 第一个窗口：第一次点击在右上角，第二次点击在左上角
        top_right_x = geometry["x"] + geometry["width"]
        top_right_y = geometry["y"]
        top_left_x = geometry["x"]
        top_left_y = geometry["y"]

        # 第一次点击：右上角 + (-60, 70)
        first_click_x = top_right_x + CONFIG["window1_first_click_offset_x"]
        first_click_y = top_right_y + CONFIG["window1_first_click_offset_y"]

        logging.info(f"步骤: 移动鼠标到右上角 + ({CONFIG['window1_first_click_offset_x']}, "
                    f"{CONFIG['window1_first_click_offset_y']}) = ({first_click_x}, {first_click_y})")

        move_mouse(first_click_x, first_click_y)
        mouse_click(button=1)

        # 等待0.5秒
        logging.info(f"步骤: 等待 {CONFIG['click_wait_delay']} 秒")
        time.sleep(CONFIG["click_wait_delay"])

        # 第二次点击：左上角 + (100, 110)
        second_click_x = top_left_x + CONFIG["window1_second_click_offset_x"]
        second_click_y = top_left_y + CONFIG["window1_second_click_offset_y"]

        logging.info(f"步骤: 移动鼠标到左上角 + ({CONFIG['window1_second_click_offset_x']}, "
                    f"{CONFIG['window1_second_click_offset_y']}) = ({second_click_x}, {second_click_y})")

        move_mouse(second_click_x, second_click_y)
        mouse_click(button=1)

    else:  # window_num == 2
        # 第二个窗口：两次点击都在左上角
        top_left_x = geometry["x"]
        top_left_y = geometry["y"]

        # 第一次点击：左上角 + (250, 70)
        first_click_x = top_left_x + CONFIG["window2_first_click_offset_x"]
        first_click_y = top_left_y + CONFIG["window2_first_click_offset_y"]

        logging.info(f"步骤: 移动鼠标到左上角 + ({CONFIG['window2_first_click_offset_x']}, "
                    f"{CONFIG['window2_first_click_offset_y']}) = ({first_click_x}, {first_click_y})")

        move_mouse(first_click_x, first_click_y)
        mouse_click(button=1)

        # 等待0.5秒
        logging.info(f"步骤: 等待 {CONFIG['click_wait_delay']} 秒")
        time.sleep(CONFIG["click_wait_delay"])

        # 第二次点击：左上角 + (100, 110)
        second_click_x = top_left_x + CONFIG["window2_second_click_offset_x"]
        second_click_y = top_left_y + CONFIG["window2_second_click_offset_y"]

        logging.info(f"步骤: 移动鼠标到左上角 + ({CONFIG['window2_second_click_offset_x']}, "
                    f"{CONFIG['window2_second_click_offset_y']}) = ({second_click_x}, {second_click_y})")

        move_mouse(second_click_x, second_click_y)
        mouse_click(button=1)

    time.sleep(CONFIG["action_delay"])

    # 关闭当前窗口
    logging.info("步骤: 关闭当前窗口")

    close_window(window_id)

    logging.info("=" * 70)
    logging.info(f"第 {window_num} 个窗口测试完成")
    logging.info("=" * 70)

    return True

def check_coredumps_and_analyze(start_time):
    """
    检查/var/lib/systemd/coredump目录下是否有包含"dde-file-m"的文件，
    且文件创建时间在脚本运行期间（即修改时间晚于start_time）
    如果有就调用export_coredumps.sh分析

    参数:
        start_time: 脚本开始运行时间（datetime对象）
    """
    coredump_dir = CONFIG["coredump_dir"]

    # 检查coredump目录是否存在
    if not os.path.exists(coredump_dir):
        return True  # 目录不存在，无崩溃

    # 查找包含dde-file-m的文件，且修改时间在脚本开始时间之后
    coredump_files = []
    try:
        for filename in os.listdir(coredump_dir):
            if 'dde-file-m' in filename:
                filepath = os.path.join(coredump_dir, filename)
                if os.path.isfile(filepath):
                    # 检查文件修改时间是否在脚本开始时间之后
                    file_mtime = os.path.getmtime(filepath)
                    file_time = datetime.datetime.fromtimestamp(file_mtime)

                    if file_time >= start_time:
                        coredump_files.append(filepath)
                        logging.error(f"找到运行期间的coredump: {filename} (时间: {file_time})")

    except Exception as e:
        logging.error(f"扫描coredump目录失败: {e}")
        return False

    if not coredump_files:
        return True  # 无崩溃
    else:
        logging.info("=" * 70)
        logging.error(f"检测到 {len(coredump_files)} 个dde-file-manager的coredump文件！")
        logging.info("=" * 70)

        # 调用export_coredumps.sh脚本
        logging.info("调用export_coredumps.sh分析coredump...")

        script_dir = CONFIG["script_dir"]
        export_script = os.path.join(script_dir, "..", "tool", "export_coredumps.sh")

        if not os.path.exists(export_script):
            logging.error(f"export_coredumps.sh脚本不存在: {export_script}")
            return False

        logging.info(f"脚本路径: {export_script}")

        try:
            # 使用--name参数只分析dde-file-manager的coredump
            result = subprocess.run(
                ['bash', export_script, '--name', 'dde-file-manager'],
                capture_output=True,
                text=True,
                timeout=120  # 2分钟超时
            )

            # 记录输出
            logging.info("export_coredumps.sh 输出:")
            if result.stdout:
                for line in result.stdout.split('\n'):
                    logging.info(line)

            if result.stderr:
                logging.error("export_coredumps.sh 错误输出:")
                for line in result.stderr.split('\n'):
                    logging.error(line)

            if result.returncode == 0:
                logging.info("✓ export_coredumps.sh 执行完成")
            else:
                logging.error(f"✗ export_coredumps.sh 执行失败，返回码: {result.returncode}")

        except subprocess.TimeoutExpired:
            logging.error("export_coredumps.sh 执行超时")
        except Exception as e:
            logging.error(f"执行export_coredumps.sh失败: {e}")

        logging.info("=" * 70)
        logging.info("检测到dde-file-manager崩溃，已调用export_coredumps.sh分析")
        logging.info("=" * 70)

        return False

def delete_test_directory():
    """
    删除测试目录
    """
    logging.info("=" * 70)
    logging.info("删除测试目录")
    logging.info("=" * 70)

    test_dir = os.path.join(CONFIG["documents_dir"], CONFIG["test_dir_name"])

    if os.path.exists(test_dir):
        try:
            shutil.rmtree(test_dir)
            logging.info(f"✓ 成功删除测试目录: {test_dir}")
            return True
        except Exception as e:
            logging.error(f"✗ 删除测试目录失败: {e}")
            return False
    else:
        logging.warning(f"测试目录不存在: {test_dir}")
        return True

# ==================== 主函数 ====================
def main():
    """主函数"""
    global script_opened_windows

    log_file = setup_logging()
    script_start_time = datetime.datetime.now()

    logging.info("=" * 70)
    logging.info("双窗口双击崩溃测试开始（2次循环）")
    logging.info("=" * 70)
    logging.info(f"脚本开始时间: {script_start_time}")
    logging.info(f"循环次数: {CONFIG['loop_count']}")
    logging.info(f"测试文件数: {CONFIG['num_files']}")

    logging.info("第1个窗口点击策略:")
    logging.info(f"  第一次点击（右上角偏移）: ({CONFIG['window1_first_click_offset_x']}, {CONFIG['window1_first_click_offset_y']})")
    logging.info(f"  第二次点击（左上角偏移）: ({CONFIG['window1_second_click_offset_x']}, {CONFIG['window1_second_click_offset_y']})")

    logging.info("第2个窗口点击策略:")
    logging.info(f"  第一次点击（左上角偏移）: ({CONFIG['window2_first_click_offset_x']}, {CONFIG['window2_first_click_offset_y']})")
    logging.info(f"  第二次点击（左上角偏移）: ({CONFIG['window2_second_click_offset_x']}, {CONFIG['window2_second_click_offset_y']})")

    # 清空窗口跟踪列表
    script_opened_windows = []

    # 步骤1: 创建测试文件（仅执行一次）
    nested_dir = create_test_files_with_nested_dir(
        CONFIG["documents_dir"],
        CONFIG["test_dir_name"],
        CONFIG["num_files"],
        CONFIG["max_file_size"]
    )

    if nested_dir is None:
        logging.error("创建测试目录失败")
        return 1

    # 循环执行步骤2-6（共2次）
    logging.info("=" * 70)
    logging.info(f"开始循环执行步骤2-6（共 {CONFIG['loop_count']} 次，每次2个窗口）")
    logging.info("=" * 70)

    for i in range(1, CONFIG["loop_count"] + 1):
        logging.info("=" * 70)
        logging.info(f"第 {i}/{CONFIG['loop_count']} 次循环开始")
        logging.info("=" * 70)
        time.sleep(CONFIG["loop_delay"])

        # 步骤2-5: 测试第一个窗口
        if not perform_window_test(nested_dir, 1, script_start_time):
            logging.error(f"第 {i} 次循环中的第1个窗口测试失败")
            # 删除测试目录
            delete_test_directory()
            return 1

        # 步骤6: 检查coredump
        logging.info("步骤6: 检查coredump")
        success = check_coredumps_and_analyze(script_start_time)

        if not success:
            logging.error(f"在第 {i} 次循环中检测到崩溃")
            # 删除测试目录
            delete_test_directory()
            return 1

        time.sleep(CONFIG["loop_delay"])
        # 步骤2-5: 测试第二个窗口
        if not perform_window_test(nested_dir, 2, script_start_time):
            logging.error(f"第 {i} 次循环中的第2个窗口测试失败")
            # 删除测试目录
            delete_test_directory()
            return 1

        # 第6步中的第二次检查coredump（可选，为了更严格的崩溃检测）
        logging.info("步骤6: 检查coredump（第2个窗口后）")
        success = check_coredumps_and_analyze(script_start_time)

        if not success:
            logging.error(f"在第 {i} 次循环的第2个窗口后检测到崩溃")
            # 删除测试目录
            delete_test_directory()
            return 1

		

    logging.info("=" * 70)
    logging.info(f"完成所有 {CONFIG['loop_count']} 次循环")
    logging.info("=" * 70)

    # 步骤8: 删除测试目录
    delete_test_directory()

    logging.info("=" * 70)
    logging.info("测试完成，未检测到崩溃")
    logging.info(f"日志文件: {log_file}")
    logging.info("=" * 70)

    return 0

if __name__ == "__main__":
    sys.exit(main())
