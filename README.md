# DirSentinel

> 语言切换 / Language: 若需英文版文档，请查看 [README_en.md](README_en.md)。
> For English documentation, see [README_en.md](README_en.md).

## 1. 项目概述

`DirSentinel` 是一个在后台运行的 Windows 服务，旨在自动管理指定文件夹的磁盘空间。

它会周期性地监控一个或多个文件夹。当文件夹的大小超出了您在 `config.json` 文件中设定的策略阈值时，服务会自动删除该文件夹（及其所有子文件夹）中最旧的文件，直到文件夹大小符合策略要求为止。

所有操作都会被详细地记录在与服务程序位于同一目录下的 `dirsentinel.log` 文件中。

## 2. 打包方式 (编译)

本项目使用 `MinGW-w64` 进行交叉编译，以便在 macOS 或 Linux 环境下生成 Windows 可执行文件。

**编译命令:**

```bash
x86_64-w64-mingw32-gcc -o dirsentinel.exe main.c monitor.c config.c logger.c utils.c cJSON.c service.c -lws2_32 -lole32 -lgnurx -static
```

*   `-o dirsentinel.exe`: 指定输出的可执行文件名。
*   `main.c monitor.c ... service.c`: 列出所有需要编译的 `.c` 源文件。
*   `-lws2_32 -lole32`: 链接 Windows 必需的库。
*   `-static`: （推荐）静态链接所有库，使得 `dirsentinel.exe` 可以在没有安装特定依赖的 Windows 系统上独立运行。

注意：为支持正则表达式，需要在 MinGW-w64 环境安装并链接 `libgnurx`（有的环境库名为 `libregex`，可用 `-lregex`）。如果遇到链接错误，请安装相应的正则库并更换链接参数。

编译成功后，您会得到一个 `dirsentinel.exe` 文件。

## 3. 运行方式 (服务管理)

请将整个项目文件夹（特别是 `dirsentinel.exe` 和 `config.json`）复制到您的 Windows 机器上。然后在 Windows 的**命令提示符 (CMD)** 或 **PowerShell** 中以**管理员身份**运行以下命令。

*   **安装服务**:
    ```shell
    dirsentinel.exe install
    ```

*   **启动服务**:
    ```shell
    sc start DirSentinelService
    ```
    or
    ```shell
    net start DirSentinelService
    ```

*   **停止服务**:
    ```shell
    sc stop DirSentinelService
    ```
    or
    ```shell
    net stop DirSentinelService
    ```

*   **卸载服务**:
    ```shell
    dirsentinel.exe uninstall
    ```

## 4. 配置文件 (`config.json`)

服务的行为完全由 `config.json` 文件控制。您可以在 `policies` 数组中定义一条或多条监控策略。

```json
{
    "check_interval_seconds": 10,
    "policies": [
        {
            "path": "D:\\Temp",
            "policy": {
                "type": "size_gb",
                "value": 10
            },
            "allowed_extensions": [".log", ".tmp", ".bak"],
            "recursive": true,
            "excluded_subdirs": ["cache", "tmp"],
            "included_subdirs": []
        },
        {
            "path": "C:\\Users\\YourUser\\Downloads",
            "policy": {
                "type": "percentage",
                "value": 5
            },
            "allowed_extensions": [".zip", ".iso"],
            "recursive": false
        }
    ]
}
```

*   `check_interval_seconds`: （可选）服务检查策略的频率，单位为秒。如果未提供，则默认为 60 秒。
*   `path`: 要监控的文件夹的**绝对路径**。请注意 JSON 中需要使用双反斜杠 `\\` 来表示路径分隔符。
*   `policy`: 定义策略规则。
    *   `type`: 策略类型，可以是 `"size_gb"` 或 `"percentage"`。
    *   `value`: 策略的阈值。
*   `allowed_extensions`: 一个字符串数组，定义了哪些文件扩展名是可以被删除的。服务**只会**删除这些类型的文件。
*   `recursive`: （可选，默认 `true`）是否递归扫描子目录。设为 `false` 时仅扫描主目录中的文件。
*   `excluded_subdirs`: （可选）需要**跳过**的子目录名称列表（仅匹配一级子目录名）。当 `recursive=true` 时有效。
*   `included_subdirs`: （可选）需要**限定**递归进入的子目录名称列表（仅匹配一级子目录名）。当该列表非空时，只有列出的子目录会被递归扫描；其优先级高于 `excluded_subdirs`。

### `allowed_extensions` 支持正则表达式

为兼容旧配置，数组项如果以 `.` 开头（如 `.log`、`.zip`），仍然按“扩展名完全匹配”处理。
除此之外，您可以提供**正则表达式**，将对“完整文件名”进行匹配（POSIX 扩展正则）。示例：

```json
{
  "allowed_extensions": [
    ".zip",                          // 旧行为：只匹配扩展名 .zip
    ".bak",                          // 旧行为：只匹配扩展名 .bak
    ".tmp",                          // 旧行为：只匹配扩展名 .tmp
    ".*\\.(zip|7z)$",               // 正则：匹配 .zip 或 .7z 结尾的文件名
    "^backup_\\d{8}\\.log$"       // 正则：匹配如 backup_20240101.log 的文件名
  ]
}
```

说明：
- 当条目以 `.` 开头时，按扩展名比较（与旧版本行为一致）。
- 其他条目按正则表达式处理，匹配**完整文件名**（不包含路径）。
- 正则遵循 POSIX 扩展语法（`regcomp/REG_EXTENDED`）。
- JSON 中需要对反斜杠进行转义，例如 `\\d`、`\\.`。

## 5. 策略判断方式

服务每隔 10 秒检查一次所有策略。

### 策略 1: `size_gb`

这是基于文件夹**绝对大小**的策略。

*   **判断逻辑**: 程序会递归计算 `path` 所指定文件夹的总大小（包含所有子文件夹）。如果这个总大小**大于** `value` 中指定的 GB 数，则策略被触发。
*   **日志示例**: `Path: D:\Temp, Size: 12.50 GB, Policy: > 10.00 GB`
*   **场景举例**:
    *   **配置**:
        ```json
        "path": "D:\\DatabaseBackups",
        "policy": { "type": "size_gb", "value": 20 }
        ```
    *   **情景**: `D:\DatabaseBackups` 文件夹的总大小增长到了 21.7 GB。
    *   **结果**: 策略被触发，因为 `21.7 GB > 20 GB`。服务将开始执行删除操作。

### 策略 2: `percentage`

这是基于文件夹大小**占其所在磁盘总容量百分比**的策略。

*   **判断逻辑**: 程序首先计算 `path` 文件夹的总大小，然后获取该文件夹所在的磁盘驱动器（如 `C:` 盘）的总容量。最后，用 `(文件夹大小 / 磁盘总容量) * 100` 计算出百分比。如果这个百分比**大于** `value` 中指定的数值，则策略被触发。
*   **日志示例**: `Path: C:\\Downloads, Usage: 28.50 GB / 200.00 GB (14.25%), Policy: > 10.00% (20.00 GB)`
*   **场景举例**:
    *   **配置**:
        ```json
        "path": "C:\\Users\\Admin\\Downloads",
        "policy": { "type": "percentage", "value": 10 }
        ```
    *   **情景**: `C:` 盘的总容量为 500 GB。因此，策略的阈值是 `10%`，即 `50 GB`。现在，`C:\\Users\\Admin\\Downloads` 文件夹的总大小达到了 52 GB。
    *   **结果**: 策略被触发，因为 `(52 GB / 500 GB) * 100 = 10.4%`，而 `10.4% > 10%`。服务将开始执行删除操作。

## 6. 删除判断方式

当任一策略被触发后，服务会启动一个循环删除流程，直到文件夹大小重新满足策略要求。

*   **扫描范围**: 服务会递归地扫描整个被监控的文件夹及其所有子文件夹。
*   **筛选文件**: 在扫描过程中，它只关注文件名后缀在 `allowed_extensions` 列表中的文件。
    - 若条目以 `.` 开头，则按扩展名精确匹配（如 `.zip`）。
    - 其他条目按正则表达式匹配整个文件名（如 `^backup_\\d{8}\\.log$`）。
*   **子目录控制**:
    - 若 `recursive=false`，不进入任何子目录。
    - 若 `included_subdirs` 非空，仅进入列出的子目录。
    - 若 `excluded_subdirs` 包含某子目录名，则跳过该子目录的递归扫描。
*   **排序方式**: 它会比较所有符合条件的文件（来自所有子目录）的**最后修改日期**，并找到那个**最旧**的文件。
*   **执行删除**: 服务会删除那个最旧的文件，并记录一条日志。
*   **循环检查**: 删除一个文件后，服务会**立即重新计算**文件夹的总大小并再次检查策略。如果大小仍然超标，它会重复上述“扫描->筛选->排序->删除”的步骤，删除下一个最旧的文件。这个过程会一直持续，直到文件夹大小降到策略阈值以下。

### 场景举例：

*   **配置**:
    ```json
    "path": "E:\\Archives",
    "policy": { "type": "size_gb", "value": 5 },
    "allowed_extensions": [".zip"]
    ```
*   **情景**: `E:\\Archives` 文件夹大小为 5.2 GB。其中包含以下文件：
    *   `E:\\Archives\\2023\\jan.zip` (修改于 2023-01-31)
    *   `E:\\Archives\\2023\\feb.zip` (修改于 2023-02-28)
    *   `E:\\Archives\\2022\\dec.zip` (修改于 2022-12-31)
    *   `E:\\Archives\\config.xml` (不符合扩展名)
*   **删除流程**:
    1.  **第一次检查**: `5.2 GB > 5 GB`，策略触发。
    2.  **第一次删除**: 服务扫描所有子目录，找到三个 `.zip` 文件。其中 `dec.zip` (2022-12-31) 是最旧的。服务删除 `E:\\Archives\\2022\\dec.zip`。
    3.  **第二次检查**: 假设 `dec.zip` 大小为 0.3 GB。现在文件夹总大小为 4.9 GB。
    4.  **结束**: `4.9 GB < 5 GB`，策略满足。删除循环结束。服务将等待下一个 10 秒检查周期。