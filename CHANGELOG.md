# Changelog

## [2026-05-10] - 输入系统重组与引擎 API 简化

### AgentSession 封装与数据隐藏
- `AgentSession` 从 struct 重构为 class，所有字段私有化，通过 getter/setter 访问
- 新增 `agent_session.cc` 实现文件（208 行），接管构造函数、move 语义、`SetOutput()`、快照生成
- `SetOutput()` 从 `AgentCore` 静态方法内聚到 `AgentSession` 成员方法，输出回调、流式文本累积统一由 session 管理
- `AgentCore` 移除 `SetSessionOutput()` 静态方法，所有调用点改为 `session.SetOutput()`
- `stop_requested` 原子标志封装为 `RequestStop()` / `IsStopRequested()` / `ClearStopRequested()` 方法

### AgentEngine API 简化
- 移除 `focused_session_id_` 概念，不再持有 "当前会话" 状态
- TUI (`AiCoding`) 改为在 `Run()` 时 `CreateSession()` 并自行持有 `session_id_`
- 旧 API `ProcessUserMessage(text)` / `StopCurrentSession()` 替换为 `SendUserMessage(session_id, text)` / `StopSession(session_id)`
- 新增 `GetFocusedSessionSnapshot()` 返回 `std::optional<RenderSnapshot>`，UI 每帧无锁取一次渲染快照
- `SwitchRole` 签名改为 `(session_id, new_role_id)`，会话可原地切换角色

### 连续输入合并（pending buffer）
- `AgentSessionManager::SendToSessionAsync` 引入生产-消费模式：所有消息追加到 `pending_inputs_` buffer
- 单 task 内循环 swap 消费，buffer 空即结束，连续输入合并为一次 LLM 调用
- 新增 `StartChain()` 方法管理单线程内的积压处理循环
- 输入流时序：`输入 A → 提交 StartChain → 输入 B/C 追加 → Loop(A) → drain [B,C] 合并 → Loop("B\n\nC")`

### 主动触发管理迁移
- `ActiveTriggerManager` 初始化从 `AgentSessionManager` 迁移到 `AgentEngine::InitializeComponents()`
- `AgentSessionManager` 移除 `ActiveTriggerManager` 和 `ActiveInteractionManager` 依赖

### 删除 ActiveInteractionManager
- 移除 `active_interaction_manager.cc/h`（共 392 行）
- 主动交互逻辑（基于会话事件的 LLM 回调）不再维护

### SDL/UI 渲染重构
- `VirtualSprite` 移除 `RegisterAgentOutputCallback()` / `DispatchSessionStates()` / `RegisterMessageSubmitCallback()`，状态路由改为快照驱动
- 静态方法 `RenderHome` / `RenderVirtualHuman` / `RenderGalgame` / `RenderTerminal` 内联到 `Render()` switch 分支，不再作为独立方法
- 移除 `session_states_` 队列和 `session_states_mutex_` 线程安全队列（由 RenderSnapshot 替代）
- `UIRenderer` 精简：移除 `SendToChatPanel / StartAssistantMessage / UpdateLastMessage / SubmitUserMessage / ClearHistory / SetAgentState` 等 7 个方法
- 新增 `SetSnapshotGetter()`，渲染器在 `RenderImGui()` 时通过回调取最新快照，不再持有状态副本
- `StatusBar` 从 `Drawer` 点阵绘制改为 `MediaUtil::DrawTextRect` 字体渲染，移除 `Drawer` 依赖
- 新增 `StateColor` 结构体和 `MakeVisualProps()` 工具函数，状态颜色定义集中到各自的 cpp 文件

### 角色状态可视化增强
- `agent_state_observer.cc` 新增 `GetStateVisualProps()` 本地函数，细化 13 种 AgentRuntimeState → 视觉属性映射（含 scarf/aura 颜色）
- 新增 streaming/deep thinking 状态颜色支持

### 未使用方法清理
- 移除 `galgame_mode.cc` 的 `ClearDialogue()` / `SetSpeakerColor()`
- 移除 `home_screen.h` 的 `DrawTitle()` / `DrawModeButtons()` / `DrawFooter()` 私有方法
- 移除 `banner.cc` 的 `PrintHelp()` 方法（`/help` 命令由 CommandRegistry 处理）
- 移除 `character_state_observer.cc` 中未使用的 `MapToCharacterState` 调用

### 其他
- `agent_types.h` 新增 `RenderSnapshot` 快照结构体（含 session_id / role_id / state / messages / streaming_text / streaming_thinking）
- `agent_core.h` 移除已废弃的静态方法声明
- `media_engine/CMakeLists.txt` 新增 ZLIB 链接依赖
- `layout_config.h` 新增办公区地砖尺寸常量和区域计算方法
- README.md 更新「输入模型与中断策略」章节，文档化 pending buffer 模式

### 文件统计
- 变更文件：31 个
- 新增：+871 行
- 删除：-1,303 行
- 净变化：-432 行（持续精简）

---

## [2026-05-07] - 会话系统重构与多角色渲染

### 引擎多会话 API
- `AgentEngine` 新增 `CreateSession()` / `SendMessage(session_id, text)` / `StopSession(session_id)` 多会话公开接口，为 server/多角色场景准备
- `current_session_id_` 更名为 `focused_session_id_`，明确单会话便捷接口的语义范围
- `OutputCallback` 签名增加 `session_id` 和 `role_id` 参数，下行通道支持多 session 路由

### 会话管理器重构
- `AgentSessionManager` 移除 `MemoryManager` 依赖和 `SwitchMemoryContext()`，职责简化
- `sessions_` 容器从 `unordered_map<string, AgentSession>` 改为 `unordered_map<string, unique_ptr<AgentSession>>`，确保 session 指针地址稳定
- 新增 `session_mutex` per-session 锁，保证同一 session 的 `AgentCore::Loop` 调用串行执行
- `auto_confirm_tools` 改为 per-session 临时提权：创建 session 时包装 tool_executor，调用期间临时切换 PermissionManager 模式后恢复，不再影响全局
- `GetSession()` 增加 `mutex_` 保护，线程安全

### 角色状态可视化多实例
- `AgentStateVisualizer` 新增 `GetOrCreate(role_id)` 工厂方法，每个角色独立实例
- `UpdateAll()` / `RenderAll()` 静态方法，支持多角色并行更新和渲染
- `VirtualSprite` 输出回调改为写入 `session_states_` 队列，`DispatchSessionStates()` 在渲染循环中统一派发到对应角色的 visualizer

### llama.cpp 脚本精简与迁移
- 启动脚本从 `scripts/` 迁移至 `config/.prosophor/scripts/`，随配置目录一同分发
- 启动脚本去掉 jq/python3 配置解析逻辑，改为纯参数传递，脚本职责单一化
- 删除 Windows `.bat` 版本（`start_llamacpp_server.bat`、`stop_llamacpp_server.bat`）和旧版 `start_llamacpp_server.sh`
- `LocalModelManager::Start()` 改为调用外部脚本而非内联构造参数
- 删除 CMake 中脚本安装规则（脚本已随 config 目录安装）

### 平台层清理
- 新增 `platform::NullDevice()` 跨平台空设备路径（POSIX `/dev/null` / Windows `NUL`）
- `CheckPortOpen()` / `WaitForHealth()` 改用 `NullDevice()` 代替硬编码路径
- `LaunchDetachedCommand()` 大幅简化：删除 Windows `CreateProcess` 内联实现（改由 shell 脚本处理后台逻辑），统一为 `RunShellCommand`

### 文件统计
- 变更文件：19 个
- 新增：+281 行
- 删除：-416 行
- 净变化：-135 行（持续精简）

---

## [2026-05-06] - 核心引擎抽象与前后端分离

### 核心架构重构 - prosophor_core
- 所有业务逻辑集中到 `prosophor_core/` 目录，构建为静态库，零 UI/SDL 依赖
- 新增 `AgentEngine` 单例 — 统一核心入口，管理 MemoryManager、ToolRegistry、SessionManager、ProviderRouter、LSP、Config、LocalModelManager
- 新增 `agent_types.h` — `AgentRuntimeState` 从 `ui_types.h` 迁入核心层，消除 UI 对核心状态枚举的依赖
- 通过 `SetOutputCallback()` / `SetPermissionCallback()` 实现前端无关的回调注册，TUI 和 SDL 共用同一引擎

### TUI 前端重构 - AiCoding
- `AiCoding` 替代 `AgentCommander`，全新的终端输入循环（`InputHandler` + `InputEvent`）
- 注册引擎回调：输出流式渲染（thinking/content/tool/complete 各阶段）、权限交互确认
- `banner.cc/h` 从 `common/` 迁至 `ai_coding/`（终端专属）

### SDL 前端重构 - VirtualSprite
- `SdlApp` 更名为 `VirtualSprite`，重新设计为 AgentEngine 的前端消费层
- 模式切换：HOME / VIRTUAL_HUMAN / GALGAME / TERMINAL
- 通过 `RegisterAgentOutputCallback()` + `RegisterMessageSubmitCallback()` 挂载引擎回调
- 状态可视化属性内联（移除 `agent_state_visualizer.h`）

### 构建系统重构
- `main_src/CMakeLists.txt` 完全重写，分为三个目标：`prosophor_core`（静态库）、TUI `prosophor`、SDL `prosophor`
- TUI 构建仅链接 prosophor_core + ai_coding，无 SDL/media/scene 依赖
- SDL 构建链接 prosophor_core + media_engine + scene + virtual_sprite + components
- 平台源文件（input_handler/pipe_handler）按平台正确选择
- `tests/CMakeLists.txt` 新增 prosophor_core include 路径
- 顶层 `CMakeLists.txt`：安装 llama.cpp 启停脚本

### 删除模块
- 移除 `cli/agent_commander.cc/h`（功能由 AiCoding 替代）
- 移除 `tools/tool_registry.cc/h`（功能由 AgentEngine 封装）
- 移除 `tools/command_tools/background_run_tool.cc/h`
- 移除 `core/agent_state_visualizer.h`
- 移除 `input_event.h` 中的 `OutputEvent`、`InputEventCallback`、`OutputEventCallback`（移至前端层）

### llama.cpp 管理脚本
- 新增 `scripts/start_llamacpp_server.sh` / `.bat` — 从 settings.json 读取配置，自动查找二进制与模型文件，后台启动并输出 PID
- 新增 `scripts/stop_llamacpp_server.sh` / `.bat` — 按 PID 停止，支持 `--force`

### 其他
- media_engine UI 组件内部 include 移除 `media/` 前缀（`media/colors.h` → `colors.h`）
- `agent_state_observer.h` 改为引用 `core/agent_types.h`

### 文件统计
- 变更文件：109 个
- 新增：+1,069 行
- 删除：-2,954 行
- 净变化：-1,885 行（大幅精简）

---

## [2026-05-05] - 管道抽象层与跨平台完善

### 管道与进程抽象 (pipe_handler)
- 新增 `platform/pipe_handler.h` + `pipe_handler_posix.cc` + `pipe_handler_win32.cc`
- 统一 pipe/fork/exec/wait 跨平台接口，消除 LSP/MCP 中的 `#ifndef _WIN32` 条件编译
- `ForkAndExec()`：POSIX 用 fork+execvp，Windows 用 CreateProcess + 管道句柄转 fd
- `CreatePipe/ClosePipe/ReadPipe/WritePipe/Dup2Pipe/WaitProcess/GetCurrentPid` 全平台抽象
- `SetPipeNonBlocking/IsPipeWouldBlock/GetPipeErrorString` 统一错误处理

### MCP Client 重构
- `mcp_client.cc` 移除内联 POSIX pipe/fork/kill/wait 代码，全部改用 `platform::ForkAndExec/KillProcess/ClosePipe/ReadPipe/WritePipe`
- 消除全部 `#ifndef _WIN32` / `#else` 块，净减 ~90 行

### LSP Manager 重构
- `lsp_manager.cc` 同样替换为 `platform::ForkAndExec/WritePipe/ReadPipe/ClosePipe`
- 移除 POSIX 头文件依赖 (`unistd.h`, `sys/wait.h`)，净减 ~50 行

### 平台层增强
- `platform.cc/h` 新增：
  - `NormalizePath()` — Windows MinGW 下 POSIX 路径 `/x/...` → `X:\...` 转换
  - `PathExists()` — 带平台路径归一化的文件存在检查
  - `SelectPlatformPath()` — 编译期跨平台路径选择（无需 `#ifdef`）
  - `LaunchDetachedCommand()` — 分离后台进程启动
  - `ExecuteScriptWithTimeout()` — 从 `subprocess_wrapper` 迁移而来
- Windows: `SetConsoleUtf8()` 增加 ANSI/VT 转义序列支持；`GetSelfExePath()` 增加 Windows 实现
- Windows: `LaunchProcess()`/`LaunchDetachedCommand()` 自动搜索 MinGW bin 目录加入 PATH，确保子进程能找到运行时 DLL

### 本地模型管理优化
- `local_model_manager.cc`：`LaunchProcess(args)` → `LaunchDetachedCommand(shell_cmd)`，使用 `ShellEscape` 防止路径注入
- 健康检查改为等待模型真正加载完成（`/health` 返回 200 而非仅端口开放），超时失败自动 `Stop()`
- 新增超时日志节流，每 10s 打印一次等待状态
- `local_model_utils.cc`：移除 `ResolveModelPath()`（路径归一化统一由 `platform::NormalizePath` 处理）
- 新增 `.exe` 搜索路径变体，新增 `build/` 和 `build_win/` 构建目录搜索

### 配置更新
- `LocalModelConfig` 新增 `model_path_for_win` 字段，跨平台双路径支持
- `config.cc` 加载配置时自动调用 `SelectPlatformPath` + `NormalizePath`
- `settings.json`：`model_path` 改为 `../llama_cpp_model/...`，新增 `model_path_for_win`，`auto_start: true`
- 新增默认 OpenAI provider 配置条目

### 构建系统
- 顶层 `CMakeLists.txt`：Windows 编译定义 `_WIN32_WINNT=0x0A00`、`_USE_MATH_DEFINES`、`M_PI`
- `main_src/CMakeLists.txt`：新增 pipe_handler 源文件平台过滤与选择
- `Makefile`：移除 `run_llamacpp_server` 目标

### subprocess_wrapper 移除
- `subprocess_wrapper.cc/h` 删除，功能完整迁移至 `platform::ExecuteScriptWithTimeout`

### 其他
- `cpu_temperature_monitor/trigger.py` 修复退出码语义：正常不触发 → `exit(0)`，触发 → `exit(1)`
- `images/demo.png`、`images/win_demo.png` 从 `docs/` 迁移至 `images/`

### 文件统计
- 变更文件：23 个
- 新增：+782 行
- 删除：-377 行
- 净变化：+405 行

---

## [2026-05-04] - 本地模型支持与平台抽象层

### 本地模型支持 (llama.cpp 集成)
- **LocalModelManager**：llama-server 完整生命周期管理（start/stop/restart/status）
- 新增 `/server` 命令（别名 `/local`），支持 `start|stop|status|restart` 子命令
- 新增 `/setup` 一键配置：自动检测硬件（NVIDIA GPU / Apple Silicon / CPU 线程数）+ 扫描 .gguf 模型 + 生成配置
- 配置新增 `local_models` 数组，支持 model_path、port、auto_start、n_gpu_layers、n_threads、start_timeout_ms 等参数
- CMake 通过 `FetchContent` 可选编译 llama.cpp（`-DPROSOPHOR_BUILD_LLAMA=ON/OFF`）
- `auto_start: true` 时启动自动拉起本地模型服务
- 新增 `local_model_utils`：硬件检测、llama-server 二进制查找、模型路径解析

### 平台抽象层 (platform/)
- 新增 `platform/platform.h` 和 `platform/platform.cc`，统一跨平台 API：编码转换、终端 I/O、进程管理、Shell 执行
- 编译期平台常量：`kIsWindows`、`kIsLinux`、`kIsMacOS`
- `input_handler` 系列从 `cli/` 迁移至 `platform/`（`terminal_input.cc` 同步更新 include 路径）
- 消除 `#ifdef _WIN32` / `#ifdef` 条件编译，统一改为 platform API 调用，涉及：
  - `agent_commander.cc` — `ReadConsoleLine()`、`kIsWindows`
  - `subprocess_wrapper.cc` — 进程启动/管理
  - `tts_speaker.cc` — 音频播放
  - `main.cc` — `SetConsoleUtf8()`
  - `config.cc` — `GetHomeDir()`
  - `file_utils.cc` — `GetHomeDir()`
  - `input_event.h` — NOMINMAX/WIN32_LEAN_AND_MEAN（改为 include platform.h）
  - `time_wrapper.h` — `LocalTime()`
  - `skill_loader.cc` — `IsBinaryAvailable()`、`GetCurrentOs()`
  - `lsp_manager.cc` — `StartServerForFile()`、`InitializeServer()`、`ShutdownAll()`，移除 Windows `io.h` 兼容宏
  - `anthropic_provider.cc`、`ollama_provider.cc` — `ConvertToUtf8` 不再条件编译

### README 重构
- README.md / README_cn.md 大幅精简，移除冗长功能罗列和配置参考表格
- 新增 llama.cpp 本地模型文档
- 更新架构图、工具列表、模块布局、构建说明

### 其他
- `tool_registry.cc` 精简（-259 行）
- `command_registry.cc` 新增 `/server`、`/setup` 命令处理（+255 行）
- `string_utils.cc` 重构（80 行变更）
- `config.cc/h` 新增 `LocalModelConfig` 结构体及 JSON 序列化
- `Makefile`：默认 `-DPROSOPHOR_BUILD_LLAMA=OFF`，`run_llamacpp_server` 使用 build 输出的 llama-server
- `llm_provider.cc`：Token 用量日志级别 INFO → DEBUG

### 文件统计
- 变更文件：35 个
- 新增：+1,719 行
- 删除：-1,604 行
- 净变化：+115 行

---

## [2026-05-04] - Provider 接口统一与状态回调优化

### HttpClient 单例化与接口规范化
- HttpClient 从静态方法改为单例模式 (`HttpClient::Instance()`)
- `HttpRequest::post_data` → `body`，语义更清晰
- `HttpResponse::error` → `error_msg`，新增 `curl_code` 字段精确区分错误来源
- `success()`/`failed()` 判断逻辑增强（同时检查 `curl_code` 与 `status_code`）
- 新增 `StreamPhase` 枚举，用于流式输出的 thinking/content/tool_calls 阶段追踪
- `StreamHandler::OnEvent()` 改为纯虚函数，`OnStreamEnd()` 移除，接口更简洁
- `SseStreamHandler` 字段命名规范化（移除尾缀下划线），新增 `PendingToolCall` 结构

### Provider Thinking 配置重构
- OpenAI Provider: `thinking` 配置从字符串枚举 (`"off"/"low"/"medium"/"high"`) 改为布尔值
- `ShouldEnableThinking()` 参数类型同步更新，移除冗余的映射逻辑
- `ThinkingToReasoningEffort()` 简化，固定返回 `"medium"`
- 响应反序列化新增 `thinking` 类型 content block 识别
- 修复 OpenAI Provider 在 thinking 关闭时未正确设置 `enable_thinking=false` 的问题

### 状态回调分发优化
- `agent_commander.cc` 状态输出从 if-else 链重构为 `switch` 语句，可读性与性能提升
- SDL 模式 UI 回调同样迁移为 `switch` 结构
- 终端 thinking 标签格式调整：前后增加空格（`<thinking> ` / ` </thinking>`）
- 移除工作区初始化时自动创建 `AGENTS.md` 的逻辑

### 构建与配置
- `settings.json` 中所有 provider 的 `thinking` 字段统一为布尔值 (`true`/`false`)
- 修复重复 model list 问题

### 文件统计
- 变更文件：51 个
- 新增：+1,044 行
- 删除：-1,562 行
- 净变化：-518 行（持续精简）

---

## [2026-05-03] - 回调流程优化

### 状态枚举重构
- `AgentRuntimeState::THINKING` → `BEGINNING`
- `AgentRuntimeState::TOOL_MSG` → `TOOL_USE`
- 移除冗余的 `STREAM_MODE_START` 状态
- 影响范围：`ui_types.h`、`agent_core.cc`、`agent_commander.cc`、`status_bar.cc`、`agent_state_visualizer.h`、`agent_state_observer.cc`、`anime_character.cc`、`character_state_observer.cc`、`sdl_app.cc`

### Provider 流式解析重构
- 新增 `providers/detail/anthropic_stream_handler.h` (131 行) — Anthropic SSE 流处理器
- 新增 `providers/detail/ollama_stream_handler.h` (144 行) — Ollama SSE 流处理器
- 新增 `providers/detail/openai_stream_handler.h` (169 行) — OpenAI SSE 流处理器
- `anthropic_provider.cc` 减少 ~205 行，`openai_provider.cc` 减少 ~208 行，`ollama_provider.cc` 减少 ~142 行
- 各 Provider 类职责更单一，流式解析逻辑与请求逻辑分离

### 流式回调逻辑优化
- `agent_core.cc` 中 thinking/content 阶段通过 `content_phase` 字段（"start"/"delta"/"end"）区分
- 消息历史仅在关键状态节点写入（COMPLETE, TOOL_USE, ERROR），流式中间态不再写入
- `agent_commander.cc` 终端输出流格式调整，thinking 标签包裹改进
- `agent_session.h` provider 配置兜底逻辑修复

### 测试清理
- 移除 `tests/agent_core_test.cc`、`tests/compact_service_test.cc`、`tests/tool_registry_test.cc`、`tests/main.cc`
- 移除 `tools/verify.sh`

### 构建与配置
- `CMakeLists.txt`、`Makefile` 适配新文件结构
- `settings.json` 扩展，`.gitignore` 更新

---

## [2026-04-30] - v0.4.0 重大重构

### 新增功能
- **OpenAI Provider**: 新增 OpenAI 兼容接口支持 (`providers/openai_provider.cc/h`)
- **语音合成 (TTS)**: 新增文本转语音播报功能 (`common/tts_speaker.cc/h`)
- **Galgame 模式**: 新增美少女游戏风格界面 (`scene/galgame_mode.cc/h`)
- **Anime 角色系统**: 新增动画角色渲染 (`scene/anime_character.cc/h`)
- **Home Screen**: 新增主页场景 (`scene/home_screen.cc/h`)
- **Media Engine UI 组件**: 从 `components/` 迁移并重构 UI 组件到 `media_engine/ui_component/`
  - `header_bar.cc/h` - 顶部导航栏
  - `input_panel.cc/h` - 输入面板
  - `ui_container.cc/h` - UI 容器
  - `ui_panel.cc/h` - 通用面板
- **配置增强**: `config.cc/h` 大幅扩展配置项支持

### 重构优化
- **架构重构 - UI 模块**:
  - `components/input_panel.cc/h` → `media_engine/ui_component/input_panel.cc/h`
  - `components/ui_panel.cc/h` → `media_engine/ui_component/ui_panel.cc/h`
  - 移除旧版 `components/ui_panel.h`、`components/ui_types.h` 中的废弃接口
- **Provider 体系重构**:
  - 新增 `providers/openai_provider.cc` (479 行)，统一兼容 OpenAI 格式接口
  - 移除 `providers/qwen_provider.cc/h` (Qwen 功能合并至 OpenAI 兼容模式)
  - 重构 `anthropic_provider.cc/h`、`ollama_provider.cc/h`、`llm_provider.cc/h`
- **工具系统大规模精简**: 删除 20 个工具文件
  - 移除 `agent_tool` (subagent 协调工具)
  - 移除 `ask_user_question_tool` (交互工具)
  - 移除 `lsp_tool` (LSP 语言服务器工具)
  - 移除 `glob_tool`、`grep_tool` (搜索工具)
  - 移除 `cron_tool`、`task_tool`、`todo_write_tool` (任务管理工具)
  - 移除 `worktree_tool` (Git worktree 工具)
  - `tool_registry.cc` 从 ~1070 行变更，大幅精简
- **管理器精简**:
  - 移除 `buddy_manager.cc/h`、`buddy_types.cc/h` (伙伴系统)
  - 移除 `worktree_manager.cc/h` (worktree 管理)
- **CLI 精简**: `command_registry.cc` 删除 ~121 行冗余代码

### 性能与质量
- `agent_core.cc` 核心逻辑重构优化 (~194 行变更)
- `agent_session_manager.cc` 会话管理增强 (~135 行变更)
- `agent_role_loader.cc` 角色加载逻辑改进 (~90 行变更)
- `agent_state_observer.cc` 状态同步机制优化 (~205 行变更)
- `sdl_app.cc` SDL 应用框架增强 (~184 行变更)

### 配置更新
- `config/.prosophor/settings.json` 大幅扩展 (126 行变更)
- `config/.prosophor/roles/` 下所有角色配置微调 (architect, coder, default, reviewer, teacher)
- `CMakeLists.txt` 构建系统更新，适配新文件结构
- `.gitignore` 忽略规则更新

### 文件统计
- 变更文件：110 个
- 新增：+4,359 行
- 删除：-6,575 行
- 净变化：-2,216 行（大规模精简）

---

## [2026-04-19] - 重大更新，增加 UI

### 新增功能
- **UI 系统**: 基于 SDL + ImGui 的完整 UI 界面
  - 聊天面板 (chat_panel.cc/h)
  - 输入面板 (input_panel.cc/h)
  - 状态栏 (status_bar.cc/h)
  - 通用 UI 面板 (ui_panel.cc/h)
- **角色系统**: 新增 5 种 AI 角色配置
  - architect.md - 架构师角色
  - coder.md - 程序员角色
  - default.md - 默认角色
  - reviewer.md - 代码审查角色
  - teacher.md - 教学角色
- **场景系统**: 办公室场景渲染
  - 角色精灵 (character_sprite.cc/h)
  - 办公室背景 (office_background.cc/h)
  - 角色管理器 (office_character_manager.cc/h)
  - UI 渲染器 (ui_renderer.cc/h)
- **Agent 状态观察器**: 实时同步 Agent 状态到 UI
- **媒体引擎**: SDL 封装层
  - 音频 (audior.cc/h)
  - 绘图 (drawer.cc/h)
  - 字体 (font.cc/h)
  - 纹理 (texture.cc/h)
  - 颜色系统 (colors.h)
- **输入系统**: Windows 终端输入支持 (terminal_input.cc)
- **输出管理**: 统一输出管理 (output_manager.cc/h)
- **Provider 路由**: 多 LLM Provider 支持 (provider_router.cc/h)
- **Agent 角色加载器**: 动态加载角色配置 (agent_role_loader.cc/h)
- **Agent 会话管理**: 独立会话管理模块 (agent_session_manager.cc/h)
- **线程池**: 通用线程池实现 (thread_pool.h)
- **输入事件系统**: 统一输入事件定义 (input_event.h)
- **内存整合服务**: 自动内存整合 (memory_consolidation_service.cc/h)

### 重构优化
- **架构调整**: 核心模块拆分重组
  - `core/` → `cli/`: agent_commander, command_registry
  - `core/` → `common/`: messages_schema
  - `core/` → `managers/`: session_manager, skill_loader
  - `tools/` → 分类目录: agent_tools, command_tools, lsp_tools, search_tools, task_tools, worktree_tools
- **AgentCommander**: 从 core 移至 cli，代码重构 (462 行新增)
- **CommandRegistry**: 大规模重构 (438 行变更)
- **AgentCore**: 精简优化 (555 行变更)
- **ToolRegistry**: 扩展支持 (1927 行变更)
- **CurlClient**: 增强连接处理 (161 行变更)
- **TimeWrapper**: 时间处理优化 (180 行变更)

### 配置更新
- **CMakeLists.txt**: 构建系统升级
- **Makefile**: 编译配置优化
- **settings.json**: Claude Code 配置更新
- **.gitignore**: 忽略规则更新

### 文件统计
- 新增文件：140 个
- 修改文件：12953 行新增，2948 行删除
- 核心变更：UI 系统、场景渲染、角色系统、媒体引擎

---

## [2026-04-19] - 增加 Provider 请求报错信息，连接超时设置

### 优化
- 改进 LLM Provider 请求错误信息提示
- 增加连接超时配置

---

## [2026-04-19] - 支持多 LLM Provider 切换

### 新增
- 支持 Ollama Provider
- 支持 Qwen Provider
- Provider 动态切换

---

## [2026-04-19] - 适配 Windows 平台

### 兼容性
- Windows 平台适配
- 跨平台支持优化

---

## [2026-04-19] - 初始化版本

### 初始功能
- 基础 Agent 框架
- 工具系统集成
- 会话管理
