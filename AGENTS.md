# AGENTS.md

## Project Intent

This project explores an AI/Web OS style architecture where visualization and early iteration use Web technologies, while algorithms and core runtime modules are implemented in portable systems languages and can later be moved into native kernels, RTOS environments, or bare-metal targets.

The central design principle is:

- Use Web UI and browser simulation for fast feedback.
- Keep core algorithms independent from browser, OS, and hardware APIs.
- Expose stable ABI/API contracts between UI, simulation, core modules, and future kernel/HAL integrations.
- Build the same core logic for Wasm during early visualization and for native machine code when performance or kernel integration is required.
- Treat AI model capabilities as first-class operating system infrastructure, not as an application-layer add-on.

## Language Policy

This repository currently uses only the following main languages:

- `C`
- `JavaScript`
- `TypeScript`
- `Rust`
- `Python`

Language responsibilities:

- `C`: ABI interfaces, fixed-width data contracts, and a limited amount of HAL interaction glue.
- `Rust`: core modules that need to be compiled to Wasm and portable runtime components.
- `JavaScript` and `TypeScript`: browser frontend pages, simulation UI, and later React components.
- `Python`: large model, machine learning, AI orchestration, evaluation, and automation tooling.

Rules:

- Prefer `C` for native ABI boundaries and host/HAL interop.
- Prefer `Rust` for Wasm-targeted logic and environment-portable core modules.
- Prefer `JavaScript` and `TypeScript` for frontend and browser simulation work.
- Prefer `Python` for AI and ML related work outside the core runtime path.

## AI-Native OS Direction

This OS should explore what an operating system becomes when AI models, agents, embeddings, vector memory, and tool execution are treated as foundational capabilities.

The working hypothesis is:

- Traditional operating systems manage CPU, memory, storage, devices, processes, users, and permissions.
- An AI-era operating system must also manage models, prompts, context, agent execution, tools, semantic memory, inference resources, and trust boundaries.
- AI should participate in policy, orchestration, search, automation, diagnosis, and human-computer interaction.
- Deterministic OS mechanisms must remain explicit, inspectable, and testable. AI may advise or choose policy, but core safety-critical mechanisms must not depend on opaque model behavior.

AI is therefore a system capability layer with stable APIs, resource accounting, and security controls.

### AI As A System Service

The OS should eventually provide AI services in the same way that conventional systems provide filesystems, networking, process management, and device access.

Candidate system services:

- Model runtime service: load, unload, route, and execute local or remote models.
- Agent runtime service: execute long-running AI agents with explicit state, permissions, budgets, and audit logs.
- Context service: manage prompts, conversation state, task state, working memory, and context-window packing.
- Semantic memory service: manage embeddings, vector indexes, retrieval, summarization, and provenance.
- Tool service: expose OS capabilities to agents through permissioned, typed, auditable APIs.
- Policy service: allow AI-assisted decisions for scheduling, placement, caching, prefetching, fault diagnosis, and resource tuning.
- Model registry: track model identity, version, capabilities, quantization, hardware requirements, trust level, and license constraints.
- Inference resource manager: schedule CPU, GPU, NPU, memory bandwidth, KV cache, and power budgets.

### AI Layering Rules

Keep AI-specific code separated from deterministic kernel and core algorithm mechanisms.

Recommended layering:

- Core OS mechanisms: scheduling primitives, memory mapping, IPC, filesystem metadata, device queues, and isolation.
- Policy layer: pluggable policy modules that may use heuristics, rules, or AI model outputs.
- AI runtime layer: model execution, embedding generation, retrieval, context management, and agent orchestration.
- AI HAL/provider layer: adapters for local runtimes, browser-hosted models, cloud APIs, GPU/NPU backends, and future kernel or bare-metal accelerators.
- UI/observability layer: visualizes agent decisions, model calls, context flow, resource use, and permission boundaries.

Hard requirements:

- Do not let model output directly mutate kernel-critical state without validation.
- Do not hide nondeterministic model decisions inside core algorithms.
- Every AI-initiated action must have an explicit capability, budget, owner, and audit record.
- AI policies must be replaceable with deterministic policies for tests, simulation replay, and safety-critical builds.
- Model providers must be replaceable behind stable interfaces.

### AI ABI And API Direction

AI capabilities should be exposed through explicit system APIs rather than ad hoc SDK calls scattered across the codebase.

Recommended AI-facing capability groups:

- `model`: inference, streaming tokens, embeddings, model metadata.
- `context`: append, compact, retrieve, snapshot, restore.
- `memory`: semantic search, vector insert, vector delete, provenance lookup.
- `agent`: spawn, suspend, resume, cancel, inspect, checkpoint.
- `tool`: enumerate, authorize, invoke, revoke.
- `policy`: propose, score, explain, validate.
- `audit`: record model calls, tool calls, permission checks, and user approvals.

Design rules:

- Prefer typed request/response structs with explicit versioning.
- Keep provider-specific details out of core modules.
- Represent model calls as resource-consuming operations with deadlines and cancellation.
- Track token, memory, compute, power, and latency budgets.
- Keep prompt templates, policy rules, and model routing inspectable.
- Preserve enough metadata to replay or explain an AI-assisted decision when feasible.

### AI Security And Trust Model

AI integration expands the OS attack surface. Treat model output, tool calls, retrieved documents, and remote model responses as untrusted inputs.

Required controls:

- Capability-based permissions for every agent and tool.
- Sandboxing for generated code, scripts, and tool execution.
- Audit logs for model calls, retrieved context, tool invocations, and state changes.
- Explicit user or policy approval for privileged operations.
- Data classification for private, local-only, exportable, and remote-model-eligible data.
- Prompt injection and data exfiltration defenses at tool and retrieval boundaries.
- Quotas for tokens, runtime, memory, storage, network, and accelerator usage.

### AI Resource Scheduling

AI workloads introduce resources that traditional OS schedulers do not model well.

The system should eventually model and schedule:

- CPU, GPU, NPU, and accelerator time.
- Model memory and KV cache.
- Context window budget.
- Token generation throughput.
- Latency-sensitive interactive inference.
- Batch inference and background summarization.
- Power and thermal budgets on edge or embedded devices.

Early browser simulations should visualize these resources the same way they visualize processes, pages, interrupts, and devices.

### AI Digital Twin Scenarios

The browser digital twin should support AI-aware OS experiments before real kernel integration.

Useful simulation scenarios:

- AI-assisted scheduler choosing between latency, throughput, energy, and fairness policies.
- Agent task execution with explicit capabilities and audit trails.
- Context memory allocation, compaction, eviction, and retrieval.
- Model routing between local, remote, small, and large models.
- Prompt injection attempts against tool-using agents.
- GPU/NPU contention between interactive inference and background jobs.
- AI-assisted fault diagnosis using simulated kernel traces and event logs.

The visualization should show both system state and AI reasoning artifacts where safe: selected policy, confidence, constraints, rejected actions, tool calls, and resource budgets.

### AI Kernel Integration Strategy

Initial AI integration should live outside the privileged kernel path.

Preferred progression:

1. Browser simulation: AI services are mocked or hosted by JavaScript/TypeScript adapters.
2. User-space runtime: AI services run as native processes or Wasm components with explicit OS-facing APIs.
3. Kernel-adjacent services: AI policy modules advise kernel mechanisms through constrained interfaces.
4. Kernel or bare-metal integration: only small, deterministic, validated AI-adjacent components move closer to privileged execution.

Avoid placing large model runtimes directly in a kernel unless there is a specific, defensible reason. For most designs, the kernel should expose telemetry, control points, and resource primitives while AI services run in isolated user-space, RTOS tasks, or dedicated accelerator firmware.

## Development Phases

### Phase 1: Browser Digital Twin

Use the browser as a fast development and visualization environment.

Target architecture:

- Frontend: JavaScript or TypeScript, with HTML/CSS for presentation.
- Core modules: Rust compiled to WebAssembly.
- Hardware simulation: JavaScript or TypeScript simulated memory, CPU registers, interrupts, timers, disks, and device events.
- Runtime boundary: pass data and host capabilities into Wasm through explicit ABI/API interfaces.

Expected outcomes:

- Visualize scheduler behavior, time-slice allocation, task states, interrupts, and queue transitions.
- Visualize paging, virtual-to-physical address mapping, page faults, and memory allocation.
- Visualize AI agent execution, model calls, context memory, tool permissions, and inference resource scheduling.
- Iterate without depending on real device drivers or privileged CPU modes.

Implementation notes:

- v86 may be used as a high-performance WebAssembly x86 emulator when full x86 hardware or OS emulation is useful.
- Do not couple core algorithm modules directly to v86, browser APIs, or DOM APIs.
- Treat the browser simulation as one HAL implementation, not as the core system design.

### Phase 2: Standardized Core Module Extraction

Core modules must become portable Wasm components.

Rules:

- Core modules must not import browser-only APIs such as DOM, `window`, `document`, or `console`.
- Core modules must not depend on OS-specific APIs such as Linux pthreads, raw syscalls, or platform allocators unless routed through HAL interfaces.
- Prefer WASI-compatible interfaces where Wasm runtime portability is required.
- Keep exported functions, memory layout, structs, enums, and error codes versioned and documented.
- Build test harnesses that run the same core logic under browser Wasm, standalone Wasm runtimes, and native binaries where possible.

Expected outcomes:

- The core algorithm module is deployable to any compatible Wasm runtime.
- Browser visualization becomes only one host environment.
- ABI/API contracts become explicit enough to support native kernel integration later.
- AI model providers, agent runtimes, and semantic memory implementations become replaceable behind stable service interfaces.

### Phase 3: Native Kernel, RTOS, And Bare-Metal Transition

There are two supported migration routes.

Route A: Wasm runtime route

- Deploy portable Wasm modules into environments that can host Wasm applications, such as embedded runtimes, RTOS environments, or edge systems.
- Zephyr, RT-Thread, or similar systems may be evaluated as host environments when Wasm support is available.
- Keep runtime capabilities explicit: memory, clock, logging, storage, networking, interrupt events, and scheduling hooks must be provided by the host.

Route B: native static library route

- Reuse Rust or C source code directly.
- Compile core logic into static libraries or kernel objects.
- Replace browser or test HAL implementations with real HAL and kernel APIs.
- Examples: replace simulated allocation with `kmalloc` or kernel allocator wrappers, replace simulated timers with real timer/interrupt integration, replace simulated storage with real block device access.

The native route is required when maximum performance, privileged execution, or direct hardware control matters more than Wasm portability.

## Layering Rules

### Core Algorithm Layer

Responsibilities:

- Scheduler algorithms.
- Memory management and paging algorithms.
- Filesystem metadata algorithms.
- Resource accounting and policy logic.
- Deterministic state transitions.

Hard requirements:

- No browser APIs.
- No direct OS APIs.
- No direct hardware access.
- No logging, allocation, clock, IO, or threading except through explicit interfaces.
- Deterministic behavior should be preferred for simulation and replay.

Preferred languages:

- C for ABI surfaces and HAL interop.
- Rust for stronger safety boundaries, `no_std` readiness, and Wasm-targeted modules.
- JavaScript and TypeScript for browser and frontend work.
- Python for AI, ML, and model-development tooling.

### HAL Layer

Responsibilities:

- Logging.
- Time source.
- Memory allocation or physical page provider.
- Interrupt and event delivery.
- Disk, block device, or file-backed storage.
- Platform-specific synchronization.
- Host runtime adaptation.

Required implementations over time:

- Browser HAL: JavaScript or TypeScript simulation host.
- WASI HAL: standalone Wasm runtime host.
- Native user-space HAL: local testing and benchmarking.
- Kernel HAL: Linux kernel or custom kernel integration.
- Bare-metal HAL: board-specific memory map, interrupts, timers, and device drivers.
- AI provider HAL: adapters for browser models, local runtimes, remote APIs, GPU/NPU backends, and test doubles.

Rust implementations should use conditional compilation with `cfg` features such as:

- `target_arch = "wasm32"`
- `target_os = "wasi"`
- feature flags like `hal_browser`, `hal_wasi`, `hal_native`, `hal_kernel`, and `hal_baremetal`

C implementations should use CMake options and preprocessor definitions such as:

- `AI_WEB_OS_HAL_BROWSER`
- `AI_WEB_OS_HAL_WASI`
- `AI_WEB_OS_HAL_NATIVE`
- `AI_WEB_OS_HAL_KERNEL`
- `AI_WEB_OS_HAL_BAREMETAL`

## ABI And API Contract Rules

Prefer a stable C-compatible ABI at module boundaries.

Rules:

- Use fixed-width integer types.
- Avoid exposing language-specific containers, exceptions, trait objects, templates, or Rust-specific layouts across ABI boundaries.
- Use explicit pointer and length pairs for buffers.
- Use explicit status/error codes.
- Keep ownership rules documented for every pointer.
- Version the ABI and reject incompatible versions at initialization.
- Keep host imports minimal and capability-oriented.

Recommended exported module shape:

```c
uint32_t aiwos_abi_version(void);
int32_t aiwos_init(const struct aiwos_host_api* host);
int32_t aiwos_tick(uint64_t now_ns);
int32_t aiwos_handle_event(const struct aiwos_event* event);
int32_t aiwos_query_state(uint32_t query_id, void* out_buf, uintptr_t out_len);
void aiwos_shutdown(void);
```

Recommended host capability groups:

- `log`
- `clock`
- `memory`
- `event`
- `storage`
- `interrupt`
- `metrics`
- `ai_model`
- `ai_context`
- `ai_agent`
- `ai_tool`
- `ai_audit`

## Build System Direction

Use CMake as the cross-language orchestration layer when C or Rust modules, or multi-target native builds, are required.

Expected build targets:

- Browser Wasm target for visualization.
- WASI Wasm target for portable runtime testing.
- Native user-space target for fast tests and benchmarks.
- Static library target for future kernel or bare-metal integration.
- AI runtime adapter targets for local, browser, remote, mock, and accelerator-backed model providers.
- Python-based AI experiment, training, evaluation, and automation targets where needed.

Guidelines:

- Keep target-specific flags in CMake presets or toolchain files.
- Avoid baking browser assumptions into core modules.
- Keep debug symbols and tracing available for browser visualization builds.
- Keep release, LTO, and architecture-specific optimization paths available for native builds.
- Treat Wasm JIT/AOT performance as useful but not a substitute for native machine code where absolute performance is required.

## Suggested Repository Layout

```text
.
├── apps/
│   └── browser-digital-twin/
├── core/
│   ├── include/
│   ├── src/
│   └── tests/
├── hal/
│   ├── browser/
│   ├── wasi/
│   ├── native/
│   ├── kernel/
│   └── baremetal/
├── ai/
│   ├── include/
│   ├── runtime/
│   ├── agents/
│   ├── context/
│   ├── memory/
│   ├── policy/
│   └── providers/
├── abi/
│   ├── aiwos_abi.h
│   └── version.h
├── cmake/
│   └── toolchains/
├── examples/
├── docs/
└── CMakeLists.txt
```

This layout is a starting point. Follow existing repository structure if one is introduced later.

## Development Priorities

When implementing new features:

1. Define or update the ABI/API contract first.
2. Implement pure core logic without platform dependencies.
3. Add or update HAL bindings for the current target.
4. Expose state snapshots for visualization and tests.
5. Add deterministic tests for core behavior.
6. Add AI service integration through explicit capability APIs, not direct provider SDK calls.
7. Add browser visualization only after the underlying state model is stable.

## Testing Expectations

Core logic should be testable without the browser.

Minimum expected test layers:

- Unit tests for pure algorithms.
- ABI compatibility tests for struct sizes, enum values, and version checks.
- Simulation tests using deterministic clocks and event streams.
- AI policy tests with deterministic mock models and fixed responses.
- Permission and audit tests for agent tool usage.
- Replay tests for AI-assisted decisions where decisions affect system behavior.
- Browser smoke tests for visualization builds when UI code exists.
- Native benchmark or profiling targets for performance-sensitive paths.

## Agent Instructions

When working in this repository:

- Preserve the separation between core algorithm code and environment-specific HAL code.
- Do not introduce browser, Node.js, operating system, or kernel dependencies into core modules.
- Prefer explicit ABI/API boundaries over direct cross-layer calls.
- Keep Wasm and native builds in mind for every core change.
- Use CMake for cross-target build definitions unless an existing build system supersedes it.
- Document ownership, lifetime, alignment, and error behavior for ABI-facing functions.
- If a design choice makes future kernel or bare-metal migration harder, call it out before implementing it.
- Keep early UI work focused on observability and simulation control, not on hiding architectural boundaries.
- Treat AI model outputs as untrusted inputs unless a narrower trust model is explicitly documented.
- Keep AI providers, prompts, retrieval, and agent tool calls behind typed, auditable service interfaces.
- Preserve deterministic fallbacks for AI-assisted policies whenever system behavior must be tested or replayed.
- Do not add new languages without a specific, documented need.
