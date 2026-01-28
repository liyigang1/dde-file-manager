# DDE文件管理器自动化压测工具

## 项目简介

本工具用于对 `dde-file-manager`（深度文件管理器）进行自动化压力测试，支持多种测试场景。`case/` 目录下包含不同场景的压测脚本（支持 Python 和 Shell 脚本），可根据需要选择运行。

## 目录结构

```
tests/crash/
├── README.md                 # 本说明文档
├── run_case.sh               # 总控脚本（按顺序执行所有测试）
├── setup.sh                  # 一键环境配置脚本
├── case/                     # 测试用例目录
│   └── *.py                  # Python测试脚本
└── tool/                     # 工具脚本目录
    └── export_coredumps.sh   # coredump分析工具
```

## 系统要求

- **操作系统**: Deepin/UOS 或其他基于Debian的Linux发行版
- **桌面环境**: DDE (Deepin Desktop Environment)
- **目标应用**: dde-file-manager

## 快速开始

### 1. 一键配置环境

```bash
cd dfm-auto-test
chmod +x setup.sh
./setup.sh
```

### 2. 查看可用场景

```bash
ls case/
```

### 3. 运行压测

#### 方式一：使用总控脚本（推荐）

```bash
# 执行所有测试（按顺序，一个通过后才执行下一个）
./run_case.sh
```

#### 方式二：单独运行测试脚本

```bash
# 运行 Python 脚本
python3 case/<脚本名>.py

# 运行 Shell 脚本
./case/<脚本名>.sh
```

## 通用功能模块

压测脚本通常包含以下模块（具体实现因场景而异）：

### 数据构造

根据测试场景生成所需的测试数据（文件、目录等）。

### 结果统计

测试完成后输出：
- 测试时长
- 崩溃次数
- 资源使用情况
- JSON格式详细报告

## 输出文件

### 监控报告 (JSON)

位置：`~/压测日志/<场景名>_YYYYMMDD_HHMMSS.json`

### 崩溃现场

位置：`~/压测日志/crash_YYYY-MM-DDTHH-MM-SS/`

包含：
- `screenshot.png` - 崩溃时屏幕截图
- `syslog_tail.txt` - 系统日志最后100行
- `*.crash` - core dump文件（如有）

## 扩展开发

### 添加新的压测场景

在 `case/` 目录下创建新的 Python 或 Shell 脚本即可，run_case.sh 会自动识别并执行。

**注意**：脚本必须正确设置退出码（0=成功，非0=失败），以便总控脚本判断测试结果。


## 许可证

内部测试工具，仅供团队使用。
