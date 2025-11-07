# DirSentinel

> Language: English. For Chinese documentation, see [README.md](README.md).

## 1. Overview

DirSentinel is a Windows service that runs in the background to automatically manage disk space for specified folders.

It periodically monitors one or more folders. When a folder exceeds the thresholds defined in `config.json`, the service deletes the oldest eligible files (including in subfolders) until the folder complies with the policy.

All actions are logged to `dirsentinel.log` located alongside the executable.

## 2. Build

This project uses `MinGW-w64` for cross-compiling to generate a Windows executable from macOS or Linux.

Command:

```bash
x86_64-w64-mingw32-gcc -o dirsentinel.exe main.c monitor.c config.c logger.c utils.c cJSON.c service.c -lws2_32 -lole32 -lgnurx -static
```

- `-static` is recommended so that `dirsentinel.exe` runs on Windows without extra dependencies.

Note: Regex support requires linking a regex library in MinGW-w64. Typically `libgnurx` (use `-lgnurx`), or in some toolchains `libregex` (use `-lregex`). Install the appropriate package if you hit linker errors.

## 3. Running (Service Management)

Copy the entire folder (especially `dirsentinel.exe` and `config.json`) to your Windows machine. In an Administrator CMD/PowerShell, run:

- Install: `dirsentinel.exe install`
- Start: `sc start DirSentinelService` or `net start DirSentinelService`
- Stop: `sc stop DirSentinelService` or `net stop DirSentinelService`
- Uninstall: `dirsentinel.exe uninstall`

## 4. Configuration (`config.json`)

The service behavior is controlled entirely via `config.json`. Define one or more policies in the `policies` array.

Example:

```json
{
    "check_interval_seconds": 10,
    "policies": [
        {
            "path": "D:\\Temp",
            "policy": { "type": "size_gb", "value": 10 },
            "allowed_extensions": [".log", ".tmp", ".bak"],
            "recursive": true,
            "excluded_subdirs": ["cache", "tmp"],
            "included_subdirs": []
        },
        {
            "path": "C:\\Users\\YourUser\\Downloads",
            "policy": { "type": "percentage", "value": 5 },
            "allowed_extensions": [".zip", ".iso"],
            "recursive": false
        }
    ]
}
```

- `check_interval_seconds` (optional): Check frequency in seconds. Defaults to `60` if omitted.
- `path`: Absolute folder path to monitor (use `\\` in JSON strings).
- `policy`:
  - `type`: Either `"size_gb"` or `"percentage"`.
  - `value`: Threshold value (GB or percentage depending on type).
- `allowed_extensions`: Whitelist of file extensions. Only files with these extensions are eligible for deletion.
- `recursive` (optional, default `true`): Whether to recursively scan subdirectories. Set to `false` to scan only the top-level directory.
- `excluded_subdirs` (optional): Names of immediate subdirectories to skip when `recursive=true`.
- `included_subdirs` (optional): If non-empty, only subdirectories listed here are descended into (takes precedence over `excluded_subdirs`).

### Regex support in `allowed_extensions`

For backward compatibility, entries starting with `.` (e.g., `.log`, `.zip`) are treated as literal extension matches.
Additionally, you can provide POSIX Extended regular expressions that match the full filename (not the path). Examples:

```json
{
  "allowed_extensions": [
    ".zip",                        // legacy behavior: extension equals ".zip"
    ".bak",                        // legacy behavior: extension equals ".bak"
    ".tmp",                        // legacy behavior: extension equals ".tmp"
    ".*\\.(zip|7z)$",             // regex: filenames ending with .zip or .7z
    "^backup_\\d{8}\\.log$"     // regex: e.g., backup_20240101.log
  ]
}
```

Notes:
- Entries starting with `.` use literal extension matching.
- Other entries are compiled as POSIX Extended regex and matched against the filename.
- Escape backslashes in JSON strings (e.g., `\\d`, `\\.`).

## 5. Policy Evaluation

The service checks policies every `check_interval_seconds`.

### Policy: `size_gb`

- Logic: Recursively compute total folder size. If the size exceeds `value` (GB), the policy is triggered.
- Log sample: `Path: D:\Temp, Size: 12.50 GB, Policy: > 10.00 GB`
- Scenario:
  ```
  path = "D:\\DatabaseBackups"
  policy = { type = "size_gb", value = 20 }
  ```
  Folder size grows to `21.7 GB` → Triggered because `21.7 > 20`.

### Policy: `percentage`

- Logic: Compute percentage as `(folder_size / drive_total_capacity) * 100`. If it exceeds `value`, the policy is triggered.
- Log sample: `Path: C:\Downloads, Usage: 28.50 GB / 200.00 GB (14.25%), Policy: > 10.00% (20.00 GB)`
- Scenario:
  ```
  path = "C:\\Users\\Admin\\Downloads"
  policy = { type = "percentage", value = 10 }
  drive total = 500 GB → threshold = 10% = 50 GB
  ```
  Folder size is `52 GB` → Triggered because `52/500*100 = 10.4% > 10%`.

## 6. Deletion Logic

When any policy is triggered, the service starts a loop until compliance:

- Scan: Recursively traverse the monitored folder and subfolders.
- Filter: Consider only files with extensions listed in `allowed_extensions`.
  - If entry starts with `.`, match by literal extension (e.g., `.zip`).
  - Otherwise, match by regex against the filename (e.g., `^backup_\\d{8}\\.log$`).
- Subdirectory control:
  - If `recursive=false`, do not descend into any subdirectories.
  - If `included_subdirs` is non-empty, only descend into those listed.
  - If `excluded_subdirs` includes a subdir name, skip descending into it.
- Select oldest: Compare `LastWriteTime` to find the oldest eligible file.
- Delete: Remove the oldest file and log the action.
- Recheck: Recompute size and re-evaluate policy; repeat until the folder meets the policy.

Example:

```
path = "E:\\Archives"
policy = { type = "size_gb", value = 5 }
allowed_extensions = [".zip"]
```
Initial size `5.2 GB`; oldest file `dec.zip (2022-12-31)` → delete; new size `4.9 GB` → compliant.

## 7. Logging

The service writes `dirsentinel.log` next to the executable, logging policy checks and deletions.

## 8. Notes

- The service name is `DirSentinelService`; the executable is `dirsentinel.exe`.
- `check_interval_seconds` defaults to `60` seconds if not set; example shows `10` seconds.