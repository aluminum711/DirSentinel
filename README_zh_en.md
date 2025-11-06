# DirSentinel | 目录哨兵

## 概览 | Overview
- `DirSentinel` 是一个在后台运行的 Windows 服务，用于自动管理指定文件夹的磁盘空间。当文件夹大小超过策略阈值时，服务会删除最旧的文件直至合规。
- `DirSentinel` is a Windows service that runs in the background to automatically manage disk space of specified folders. When a folder exceeds policy thresholds, it deletes the oldest files until compliance.

## 打包方式（编译） | Build
- 使用 `MinGW-w64` 在 macOS/Linux 上交叉编译生成 Windows 可执行文件。
- Use `MinGW-w64` to cross-compile on macOS/Linux and produce a Windows executable.

```
x86_64-w64-mingw32-gcc -o dirsentinel.exe main.c monitor.c config.c logger.c utils.c cJSON.c service.c -lws2_32 -lole32 -static
```
- `-static` 建议启用，便于在没有额外依赖的 Windows 上运行。
- Enabling `-static` is recommended to run on Windows without extra dependencies.

## 运行方式（服务管理） | Running (Service Management)
- 将 `dirsentinel.exe` 与 `config.json` 复制到 Windows 机器，使用管理员权限在 CMD/PowerShell 中执行以下命令。
- Copy `dirsentinel.exe` and `config.json` to a Windows machine, then run the following commands in CMD/PowerShell as Administrator.

- 安装服务 | Install service: `dirsentinel.exe install`
- 启动服务 | Start service: `sc start DirSentinelService` 或 `net start DirSentinelService`
- 停止服务 | Stop service: `sc stop DirSentinelService` 或 `net stop DirSentinelService`
- 卸载服务 | Uninstall service: `dirsentinel.exe uninstall`

## 配置文件（config.json）| Configuration
- 服务通过 `config.json` 控制行为，可在 `policies` 数组中定义一条或多条策略。
- The service is configured via `config.json`, with one or more policies defined in the `policies` array.

示例 | Example:
```
{
    "check_interval_seconds": 10,
    "policies": [
        {
            "path": "D:\\Temp",
            "policy": { "type": "size_gb", "value": 10 },
            "allowed_extensions": [".log", ".tmp", ".bak"]
        },
        {
            "path": "C:\\Users\\YourUser\\Downloads",
            "policy": { "type": "percentage", "value": 5 },
            "allowed_extensions": [".zip", ".iso"]
        }
    ]
}
```

- `check_interval_seconds`（可选）：检查频率（秒）。未提供时默认 `60` 秒。
- `check_interval_seconds` (optional): Check interval in seconds. Defaults to `60` if omitted.
- `path`：要监控的文件夹绝对路径（JSON 中需使用 `\\`）。
- `path`: Absolute path of the monitored folder (use `\\` in JSON).
- `policy.type`：策略类型，`"size_gb"` 或 `"percentage"`。
- `policy.type`: Policy type, either `"size_gb"` or `"percentage"`.
- `policy.value`：阈值，单位随类型变化（GB 或百分比）。
- `policy.value`: Threshold; unit depends on type (GB or percentage).
- `allowed_extensions`：删除白名单，仅这些扩展名的文件会被删除。
- `allowed_extensions`: Deletion whitelist; only files with these extensions are eligible for deletion.

## 策略判断方式 | Policy Evaluation

### 策略一：`size_gb` | Policy: `size_gb`
- 逻辑：递归计算文件夹总大小；若总大小大于 `value`（GB），策略触发。
- Logic: Recursively compute folder total size; if it exceeds `value` (GB), the policy is triggered.
- 日志示例 | Log sample: `Path: D:\\Temp, Size: 12.50 GB, Policy: > 10.00 GB`

场景 | Scenario:
```
path = "D:\\DatabaseBackups"
policy = { type = "size_gb", value = 20 }
```
- 情景：文件夹大小为 `21.7 GB`；结果：触发，因为 `21.7 > 20`。
- Situation: Folder size is `21.7 GB`; Result: Triggered because `21.7 > 20`.

### 策略二：`percentage` | Policy: `percentage`
- 逻辑：计算文件夹大小占所在磁盘总容量的百分比；若百分比大于 `value`，策略触发。
- Logic: Compute folder size as a percentage of the drive’s total capacity; if percentage exceeds `value`, the policy is triggered.
- 日志示例 | Log sample: `Path: C:\\Downloads, Usage: 28.50 GB / 200.00 GB (14.25%), Policy: > 10.00% (20.00 GB)`

场景 | Scenario:
```
path = "C:\\Users\\Admin\\Downloads"
policy = { type = "percentage", value = 10 }
drive total = 500 GB -> threshold = 10% = 50 GB
```
- 情景：文件夹大小为 `52 GB`；结果：触发，因为 `52/500*100 = 10.4% > 10%`。
- Situation: Folder size is `52 GB`; Result: Triggered because `52/500*100 = 10.4% > 10%`.

## 删除判断方式 | Deletion Logic
- 触发后进入循环：扫描 → 筛选 → 找最旧 → 删除 → 复查，直到合规。
- Once triggered, loop: scan → filter → find oldest → delete → recheck, until compliant.
- 扫描范围：递归遍历监控目录及所有子目录。
- Scope: Recursively traverse the monitored directory and all subdirectories.
- 筛选规则：仅考虑扩展名在 `allowed_extensions` 列表中的文件。
- Filter: Consider only files whose extension appears in `allowed_extensions`.
- 最旧判定：比较文件 `LastWriteTime` 找到最旧文件。
- Oldest selection: Compare file `LastWriteTime` to find the oldest file.
- 删除失败：如删除失败则退出当前循环并记录日志。
- On delete failure: Exit the loop and log the error.

示例 | Example:
```
path = "E:\\Archives"
policy = { type = "size_gb", value = 5 }
allowed_extensions = [".zip"]
```
- 初始大小：`5.2 GB`；最旧文件为 `dec.zip (2022-12-31)` → 删除后大小降至 `4.9 GB` → 满足策略。
- Initial size: `5.2 GB`; oldest file is `dec.zip (2022-12-31)` → after deletion size drops to `4.9 GB` → policy satisfied.

## 日志 | Logging
- 服务会在与可执行文件同目录下生成 `dirsentinel.log`，记录策略评估与删除行为。
- The service writes `dirsentinel.log` alongside the executable, recording policy checks and deletions.

## 说明 | Notes
- 检查频率由 `check_interval_seconds` 控制，默认 `60` 秒；示例中为 `10` 秒。
- The check frequency is controlled by `check_interval_seconds` (default `60` seconds); the example uses `10` seconds.
- 服务名称为 `DirSentinelService`，可执行文件名为 `dirsentinel.exe`。
- The service name is `DirSentinelService`, and the executable is `dirsentinel.exe`.