# ai-web-os

AI 原生操作系统的核心框架，验证"核心逻辑可移植、宿主可替换、浏览器可视化"的分层架构。

## 分层原则

```text
abi/             ← C ABI 头文件，稳定接口（跨语言调用边界）
core/            ← Rust 核心实现，编译为 staticlib / wasm
hal/{native}/    ← 宿主能力实现（C，通过函数指针注入 core）
apps/*           ← 示例程序
```

- **ABI 用 C 定义**：C 是操作系统的"lingua franca"，所有语言都能调
- **核心用 Rust 实现**：内存安全、Wasm 一等公民、开发效率更高
- **宿主可替换**：core 通过 `aiwos_host_api_t` 函数指针调用宿主，不直接依赖任何平台 API

## 目录说明

- `abi/`：稳定的 C ABI 接口（纯头文件，`aiwos_abi.h`）
- `core/`：Rust 核心实现（调度器、状态机），编译为：
  - `staticlib`（本机 x64，供 C 程序链接）
  - `cdylib`（wasm32，供浏览器加载）
- `hal/native/`：Windows 本机宿主（日志、时间、内存分配）
- `apps/native-hello/`：本机示例，C 程序链接 Rust 核心
- `apps/browser-digital-twin/`：浏览器数字孪生，加载 Wasm 核心

## 依赖与版本

### 语言

- Rust: `2021 edition`（编译核心）
- C: `C11`（ABI 定义 + 宿主 HAL）
- HTML/CSS/JavaScript: 浏览器示例使用原生前端技术

### 构建工具

- CMake: `4.1.2`（构建本机 C 程序）
- Cargo: Rust 构建工具（构建核心）

### 编译器

- MSVC: `19.44.35219.0`（本机 C/Rust 链接）
- wasm32-unknown-unknown: Rust Wasm 目标

### 平台

- 当前验证环境: Windows
- 本机 HAL 实现: Windows API + 标准 C 库

## 构建与运行

### 1. 本机示例

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\apps\native-hello\Release\aiwos_native_hello.exe
```

预期输出包含两个宿主的运行结果：

- Native HAL：真实系统时钟、Windows API 日志
- Simple HAL：模拟时间、printf 日志

核心行为在两个宿主上一致：init → tick → event → query → shutdown，shutdown 后调用被正确拒绝。

### 2. 浏览器数字孪生

Wasm 需要 HTTP 协议加载，不能用 `file://` 直接打开：

```bash
cd apps/browser-digital-twin
python -m http.server 8080
```

浏览器打开 `http://localhost:8080`。

按钮说明：

- **Tick**：调 `aiwos_tick(now_ns)`，推进调度器 vruntime、检查阻塞超时
- **Trigger Timer IRQ**：注入定时器中断（type=2, data=42）
- **Trigger Interrupt**：注入普通中断（type=1）
- **Reset**：`shutdown → re-init` 完整生命周期

浏览器页面加载 `core/` 编译出的真实 Wasm 核心（非 JS 模拟），通过 `host_log` / `host_now_ns` 宿主回调与浏览器环境交互。

### 3. 单独构建 Rust 核心（不经过 CMake）

```bash
# 本机 staticlib
cd core && cargo build --release

# Wasm
cd core && cargo build --target wasm32-unknown-unknown --release
```

## 当前状态

- ABI v3：7 个错误码、2 种查询（SNAPSHOT / SCHEDULER_STATS）、3 种事件类型（NONE / INTERRUPT / TIMER）
- Rust 核心实现 CFS 调度器（vruntime、权重、阻塞超时）
- 本机宿主替换演示：同一个 `demo_host()` 驱动两个不同 HAL
- 浏览器数字孪生：硬件模拟（CPU、内存、IRQ）在 JS，核心状态机在 Wasm
