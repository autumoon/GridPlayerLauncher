# GridPlayer Launcher

为 [GridPlayer](https://github.com/vhanla/GridPlayer) 定制的 Windows 右键菜单启动器，让你能一键播放文件夹内的所有视频文件。

## ✨ 功能特性

- 📁 **文件夹右键菜单**：在任意文件夹上右键 → “用 GridPlayer 播放此文件夹”，自动加载该目录下所有视频文件。
- 🎯 **智能启动方式**：
  - 文件数 ≤ 10 个：通过命令行直接传参启动，响应迅速。
  - 文件数 > 10 个：自动生成 `.gpls` 播放列表文件，放在该文件夹内，以文件夹名称命名，避免命令行长度限制。
- 🛡️ **文件数限制保护**：可配置最大文件数阈值（默认 50），超过后弹窗确认是否继续，防止大量文件导致 GridPlayer 卡顿或异常。
- ⚙️ **自动配置**：首次运行自动生成 `GridPlayerLauncher.ini` 配置文件，可自定义支持的视频扩展名。
- 🖥️ **一键生成右键菜单**：双击程序（无参数）自动生成 UTF-16 编码的注册表文件（中文不乱码），导入即可使用。
- 🔍 **路径记忆**：首次运行弹窗选择 `GridPlayer.exe`，自动保存到配置文件，下次直接使用。
- 🌐 **支持中文路径**：播放列表采用 UTF-8 编码，完美支持中文文件夹/文件名。
- 📝 **详细日志**：记录每次启动、扫描和启动过程，便于排查问题（日志位于 `%TEMP%\GridPlayerLauncher.log`）。

---

## 📦 安装与使用

### 1. 下载与放置
将 `GridPlayerLauncher.exe` 和 `GridPlayer.exe` 放在**同一个目录**下（例如 `D:\Tools\VideoTools\GridPlayer\`）。

### 2. 生成右键菜单注册表
双击 `GridPlayerLauncher.exe`（不带任何参数），程序会自动：
- 检测当前目录下的 `GridPlayer.exe`（若不存在则弹窗让你选择）。
- 生成两个注册表文件：`GridPlayerLauncher添加右键菜单.reg` 和 `GridPlayerLauncher删除右键菜单.reg`。

以**管理员身份**双击 `GridPlayerLauncher添加右键菜单.reg`，确认导入。之后在任意文件夹上右键，即可看到“用 GridPlayer 播放此文件夹”。

### 3. 卸载右键菜单
以管理员身份双击 `GridPlayerLauncher删除右键菜单.reg` 即可移除。

### 4. 自定义视频扩展名（可选）
程序首次运行会自动生成 `GridPlayerLauncher.ini` 配置文件，位置与 exe 相同。打开编辑，每行一个扩展名（以点开头），支持 `#` 或 `;` 注释。

**示例 `GridPlayerLauncher.ini`**：
```ini
; 支持的文件扩展名列表
.mp4
.mkv
.avi
.mov
.wmv
.flv
.webm
; 自定义格式
.m4v
.ts
```

### 5. 自定义最大文件数阈值（可选）
`GridPlayerLauncher.ini` 中支持 `MaxFileCount` 配置项，用于设置视频文件数上限（默认 50）。当文件夹内视频文件数超过该值时，程序会弹窗询问你是否继续加载。

```ini
; 最大文件数阈值（超过后需确认是否继续）
MaxFileCount=100
```

- 阈值必须为正整数，非法值会被忽略并使用默认值 50。
- 若未配置该行，程序使用默认阈值 50。
- 建议保持默认值，过高的阈值可能导致 GridPlayer 加载大量文件时卡顿。

如果配置文件不存在或为空，程序将使用内置默认扩展名（`.mp4`, `.mkv`, `.avi`, `.mov`, `.wmv`, `.flv`, `.webm`, `.m4v`, `.mpg`, `.mpeg`, `.ts`, `.m2ts`, `.3gp`, `.rmvb`）。

---

## 🛠 编译指南

### 环境要求
- Windows SDK（10 或更高）
- Visual Studio 2015+ 或 MinGW-w64

### 使用 Visual Studio
1. 新建 **Win32 空项目**（或 C++ 空项目）。
2. 将 `main.cpp` 添加到项目。
3. 项目属性：
   - **配置属性 → 链接器 → 系统 → 子系统**：选择 **Windows (/SUBSYSTEM:WINDOWS)**。
   - 关闭预编译头（或设置为“不使用预编译头”）。
4. 编译生成 `GridPlayerLauncher.exe`。

### 使用 MinGW
```bash
g++ -mwindows -o GridPlayerLauncher.exe main.cpp -lrpcrt4 -lshlwapi
```

---

## 🧠 工作原理

| 场景 | 行为 |
|------|------|
| **双击 exe（无参数）** | 生成注册表文件（UTF-16 编码），并创建默认配置文件。 |
| **右键文件夹调用** | 程序接收文件夹路径 → 扫描该目录下的视频文件（不递归子目录）→ 若文件数超过 `MaxFileCount`（默认 50），弹窗确认是否继续，取消则退出 → 若文件数 ≤ 10 个，直接命令行传参启动 GridPlayer；若 > 10 个，生成 `.gpls` 播放列表到该文件夹，并加载。 |
| **播放列表文件** | 文件名与文件夹名相同，例如 `E:\Videos\动画\动画.gpls`，内容为标准 GridPlayer 播放列表格式（含元数据头，每个视频有唯一 UUID）。 |

---

## 📂 项目结构

```
GridPlayerLauncher/
├── main.cpp                 # 主程序源码
├── GridPlayerLauncher.exe   # 编译后的可执行文件（需与 GridPlayer.exe 同目录）
├── GridPlayerLauncher.ini   # 配置文件（自动生成，可手动编辑）
├── 添加右键菜单.reg         # 导入此文件添加右键菜单（自动生成）
└── 删除右键菜单.reg         # 导入此文件移除右键菜单（自动生成）
```

---

## ❓ 常见问题

### Q: 右键菜单没有出现？
- 确保以**管理员身份**运行 `.reg` 文件。
- 如果仍不出现，尝试重启资源管理器（任务管理器重启 `Windows 资源管理器` 进程）或重启电脑。

### Q: 播放列表文件中的中文路径乱码？
程序已使用 UTF-8 编码，只要你的系统区域设置正确（简体中文），应无乱码。若仍有问题，请检查 `GridPlayerLauncher.ini` 的保存编码（推荐 ANSI 或 UTF-8 without BOM）。

### Q: 超过 10 个文件播放失败？
- 检查该文件夹是否有写入权限（需要创建 `.gpls` 文件）。
- 确认 `GridPlayer.exe` 版本支持 `.gpls` 播放列表（最新版均支持）。
- 查看日志 `%TEMP%\GridPlayerLauncher.log` 获取详细错误信息。

### Q: 如何更改文件数阈值？
修改源码中的 `MAX_CMDLINE_FILES` 常量（当前为 10），重新编译即可。

### Q: 文件夹里文件很多时会卡顿或异常？
程序内置了文件数限制保护：当视频文件数超过 `MaxFileCount`（默认 50）时，会弹窗提示并询问是否继续。如需调整，在 `GridPlayerLauncher.ini` 中设置 `MaxFileCount=100` 即可（数值必须为正整数）。

---

## 📄 许可证

本工具采用 [MIT License](LICENSE)，自由使用、修改和分发。

---

## 🤝 贡献

欢迎提交 Issue 或 Pull Request。如果你有更好的想法或发现了 bug，请随时告知。

---

## 🙏 致谢

- [GridPlayer](https://github.com/vhanla/GridPlayer) 团队，提供了如此优秀的播放器。
- 所有参与测试和反馈的朋友。

---

**Happy watching! 🎬**