# ai-web-os Hello World

这是 `ai-web-os` 的最小可运行框架，用来验证当前的分层思路：

- `abi/`：稳定的 C ABI 接口
- `core/`：与环境无关的核心逻辑
- `hal/native/`：本机宿主层，负责日志和时间
- `apps/native-hello/`：把 ABI、core、HAL 串起来的本机示例
- `apps/browser-digital-twin/`：浏览器里的数字孪生示例

这个最小版本的目标不是完整操作系统，而是先把“核心逻辑可移植、宿主可替换、浏览器可视化”这条链路跑通。

## 当前支持的 hello world

1. 本机 hello world
2. 浏览器数字孪生 hello world

## 目录说明

- `AGENTS.md`：项目约束、分层原则和后续开发规则
- `abi/`：跨模块稳定接口
- `core/`：纯逻辑，不直接依赖浏览器或操作系统 API
- `hal/native/`：Windows 下的本机宿主实现
- `apps/native-hello/`：本机示例程序
- `apps/browser-digital-twin/`：浏览器可视化示例

## 依赖与版本

### 语言

- C: `C11`
- HTML/CSS/JavaScript: 浏览器示例使用原生前端技术

### 构建工具

- CMake: `4.1.2`

### 编译器

- MSVC: `19.44.35219.0`
- clang: `19.1.6`
- GCC: `14.2.0`

### 平台

- 当前验证环境: Windows
- 本机 HAL 实现: Windows API + 标准 C 库

## 构建与运行

### 1. 配置和构建本机示例

```powershell
cmake -S . -B build
cmake --build build --config Release
```

### 2. 运行本机 hello world

```powershell
Start-Process -FilePath "C:\code\ai\ai-web-os\build\apps\native-hello\Release\aiwos_native_hello.exe" -NoNewWindow -Wait
```

预期输出包含：

- `hello ai-web-os`
- `tick_count=1`
- `last_event_type=2`
- `last_event_data=42`

### 3. 打开浏览器数字孪生

直接在浏览器中打开：

- `C:\code\ai\ai-web-os\apps\browser-digital-twin\index.html`

按钮说明：

- `Tick`：推进一次模拟核心 tick
- `Trigger Timer IRQ`：注入一个定时器中断事件
- `Reset`：清空模拟硬件状态和核心快照

## 当前实现说明

这个 hello world 版本已经具备以下边界：

- 核心逻辑只处理状态变化，不直接操作浏览器 DOM
- ABI 层使用固定结构体和错误码
- HAL 层负责宿主相关能力，如日志和时间
- 浏览器页面负责模拟硬件和展示状态

后续如果加入 Wasm、Rust、AI 运行时、更多 HAL 或内核集成，这个 README 会继续补充对应的依赖和版本记录。

