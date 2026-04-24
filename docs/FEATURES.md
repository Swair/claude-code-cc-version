# Prosophor 核心功能清单

## 已实现功能总览

本文档列出 Prosophor 项目已实现的核心功能模块，基于对源代码的分析。

---

## 一、核心 Agent 系统

### 1.1 AgentCommander
**文件**: `core/agent_commander.h/cc`

- [x] 单例模式入口点
- [x] 主交互循环 (REPL)
- [x] 斜杠命令处理
- [x] 组件初始化编排
- [x] 系统提示构建
- [x] Tab 自动补全支持
- [x] 提供者切换 (SwitchProvider)

### 1.2 AgentCore
**文件**: `core/agent_core.h/cc`

- [x] 消息处理主循环 (CloseLoop)
- [x] 流式响应处理 (LoopStream)
- [x] 工具调用解析和执行
- [x] 多轮对话管理
- [x] 停止控制 (Stop)
- [x] 历史管理 (ClearHistory, GetHistory)
- [x] 动态配置更新 (SetConfig)
- [x] 系统提示管理 (SetSystemPrompt)
- [x] 上下文压缩集成

---

## 二、LLM 提供者

### 2.1 抽象接口
**文件**: `providers/llm_provider.h`

- [x] Chat() 同步调用
- [x] ChatStream() 流式调用
- [x] 请求/响应序列化
- [x] 模型列表查询

### 2.2 Anthropic 提供者
**文件**: `providers/anthropic_provider.h`

- [x] Claude API 集成
- [x] 流式响应解析
- [x] 工具调用格式转换
- [x] Token 使用追踪

### 2.3 Qwen 提供者
**文件**: `providers/qwen_provider.h`

- [x] 通义千问 API 集成
- [x] Thinking 模式支持
- [x] 工具调用格式转换

### 2.4 Ollama 提供者
**文件**: `providers/ollama_provider.h`

- [x] Ollama 本地模型集成
- [x] OpenAI 兼容格式
- [x] 工具调用格式转换

---

## 三、工具系统

### 3.1 工具注册表
**文件**: `core/tool_registry.h`

- [x] 工具注册机制
- [x] 工具执行器路由
- [x] Schema 生成 (供 LLM 调用)
- [x] MCP 工具动态注册

### 3.2 内置工具

#### 文件操作
- [x] `read_file` - 读取文件内容
- [x] `write_file` - 写入文件
- [x] `edit_file` - 编辑文件

#### Shell 执行
- [x] `exec` - 执行命令
- [x] `bash` - Bash shell 执行

#### 搜索
- [x] `glob` - 文件模式匹配
- [x] `grep` - 内容搜索

#### Git 操作
- [x] `git_status` - 查看状态
- [x] `git_diff` - 查看差异
- [x] `git_log` - 查看提交历史
- [x] `git_commit` - 提交更改
- [x] `git_add` - 添加文件
- [x] `git_branch` - 分支管理

#### 交互工具
- [x] `ask_user_question` - 向用户提问
- [x] `todo_write` - 任务列表管理

#### LSP 工具
- [x] `lsp_diagnostics` - 获取诊断
- [x] `lsp_go_to_definition` - 跳转定义
- [x] `lsp_find_references` - 查找引用
- [x] `lsp_get_hover` - 悬停信息
- [x] `lsp_document_symbols` - 文档符号
- [x] `lsp_workspace_symbols` - 工作区符号
- [x] `lsp_format_document` - 文档格式化
- [x] `lsp_list_servers` - 列出 LSP 服务器
- [x] `lsp_all_diagnostics` - 全部诊断

#### Web 工具
- [x] `web_search` - 网络搜索
- [x] `web_fetch` - 获取网页内容

#### Token 工具
- [x] `token_count` - 计算 Token 数
- [x] `token_usage` - 查看 Token 使用

#### Agent 工具
- [x] `agent` - 启动子 Agent
  - [x] launch - 启动子任务
  - [x] skill - 执行技能
  - [x] tool_search - 搜索工具

#### 计划工具
- [x] `plan_mode` - 进入/退出计划模式
- [x] `todo_write` - 任务管理

#### 定时任务工具
- [x] `cron_scheduler` - 定时任务调度
  - [x] 创建定时任务
  - [x] 列出任务
  - [x] 删除任务

#### 工作树工具
- [x] `worktree_tool` - Git worktree 管理
  - [x] 创建工作树
  - [x] 切换工作树
  - [x] 清理工作树

#### 后台运行工具
- [x] `background_run_tool` - 后台任务执行
- [x] `task_tool` - 任务管理

---

## 四、权限管理

### 4.1 PermissionManager
**文件**: `core/permission_manager.h`

- [x] 单例模式
- [x] 三种权限模式：auto/default/bypass
- [x] 规则匹配（工具名、命令模式、路径模式）
- [x] 允许/拒绝/询问规则
- [x] 用户确认回调
- [x] 失败回退逻辑（3 次拒绝后自动批准）

---

## 五、记忆和上下文管理

### 5.1 MemoryManager
**文件**: `core/memory_manager.h`

- [x] 工作空间文件加载
- [x] 身份文件读取 (SOUL.md, USER.md, MEMORY.md)
- [x] AGENTS.md 读取
- [x] TOOLS.md 读取
- [x] 文件搜索
- [x] 每日记忆保存
- [x] 文件变化监听

### 5.2 CompactService
**文件**: `core/compact_service.h/cc`

- [x] 单例模式
- [x] 自动压缩检测
- [x] 三种压缩策略：Summary/Truncate/Hybrid
- [x] LLM 摘要生成
- [x] Token 估算
- [x] 保留最近消息

### 5.3 SessionManager
**文件**: `core/session_manager.h`

- [x] 单例模式
- [x] 会话创建
- [x] 会话保存（JSON 格式）
- [x] 会话加载
- [x] 会话列表
- [x] 会话删除
- [x] 自动保存
- [x] Token 计数
- [x] 成本追踪

---

## 六、技能系统

### 6.1 SkillLoader
**文件**: `core/skill_loader.h/cc`

- [x] SKILL.md 解析
- [x] YAML frontmatter 解析
- [x] 技能门控检查（二进制、环境变量、OS 限制）
- [x] 自动安装依赖
- [x] 命令注册
- [x] 多目录加载

### 6.2 技能元数据
```cpp
struct SkillMetadata {
    std::string name;
    std::string description;
    std::vector<std::string> required_bins;
    std::vector<std::string> required_envs;
    std::vector<std::string> any_bins;
    std::vector<std::string> config_files;
    std::vector<std::string> os_restrict;
    bool always;
    std::string primary_env;
    std::string emoji;
    std::string homepage;
    std::string skill_key;
    std::string content;
    std::vector<SkillInstallInfo> installs;
    std::vector<SkillCommand> commands;
};
```

---

## 七、计划模式

### 7.1 PlanModeManager
**文件**: `core/plan_mode.h`

- [x] 单例模式
- [x] 计划模式进入/退出
- [x] 计划创建
- [x] 步骤添加
- [x] 计划批准/拒绝
- [x] 步骤状态管理（完成/失败/跳过）
- [x] 进度百分比计算
- [x] Markdown 格式输出

### 7.2 步骤状态
- [x] Pending (待处理)
- [x] InProgress (进行中)
- [x] Completed (已完成)
- [x] Skipped (已跳过)
- [x] Failed (失败)

### 7.2 TaskManager
**文件**: `agents/task_manager.h`

- [x] 任务创建
- [x] 任务列表
- [x] 任务更新
- [x] 任务依赖管理

---

## 八、LSP 语言服务

### 8.1 LspManager
**文件**: `services/lsp_manager.h`

- [x] 单例模式
- [x] 服务器配置注册
- [x] 按文件自动启动服务器
- [x] JSON-RPC 通信
- [x] 诊断管理
- [x] 文档打开/关闭通知

### 8.2 LSP 功能
- [x] 诊断（错误/警告/信息）
- [x] 跳转到定义
- [x] 查找引用
- [x] 悬停信息
- [x] 文档符号
- [x] 工作区符号
- [x] 文档格式化
- [x] 多服务器管理

---

## 九、MCP 协议

### 9.1 McpClient
**文件**: `mcp/mcp_client.h`

- [x] 单例模式
- [x] 多服务器连接
- [x] stdio/SSE/WebSocket 传输
- [x] JSON-RPC 请求/响应
- [x] 工具发现
- [x] 资源读取
- [x] 提示获取

### 9.2 MCP 管理
- [x] 服务器添加
- [x] 服务器移除
- [x] 配置文件持久化
- [x] 工具动态注册

---

## 十、配置系统

### 10.1 ProsophorConfig
**文件**: `common/config.h`

- [x] 单例模式
- [x] JSON 配置加载
- [x] 默认配置生成
- [x] 路径展开 (~)
- [x] 多提供者配置
- [x] 多 Agent 配置

### 10.2 配置项
```cpp
struct ProsophorConfig {
    std::string log_level;
    std::string default_provider;
    std::string default_agent;
    SecurityConfig security;
    std::unordered_map<std::string, ProviderConfig> providers;
    ToolConfig tools;
    SkillsConfig skills;
};
```

### 10.3 安全配置
- [x] 权限级别设置
- [x] 本地执行允许/拒绝
- [x] 路径白名单/黑名单
- [x] 命令白名单/黑名单

---

## 十一、辅助功能

### 11.1 Token 追踪
**文件**: `managers/token_tracker.h`

- [x] 按模型记录 Token 使用
- [x] 成本计算
- [x] 会话成本追踪

### 11.2 参考解析
**文件**: `core/reference_parser.h`

- [x] 文件引用解析 (@filename)
- [x] 行号解析 (@file:10)
- [x] 范围解析 (@file:10-20)

### 11.3 Buddy 系统 (伴生宠物)
**文件**: `managers/buddy_manager.h`

- [x] 稀有度系统 (Common/Uncommon/Rare/Epic/Legendary)
- [x] 物种系统 (18 种)
- [x] 外观自定义（眼睛、帽子）
- [x] 属性系统 (Debugging/Patience/Chaos/Wisdom/Snark)
- [x] ASCII 艺术渲染

### 11.4 工作树管理
**文件**: `managers/worktree_manager.h`

- [x] Git worktree 创建
- [x] worktree 切换
- [x] worktree 清理

### 11.5 后台任务管理
**文件**: `managers/background_task_manager.h`

- [x] 后台任务启动
- [x] 任务状态跟踪
- [x] 任务输出捕获

### 11.6 定时任务调度
**文件**: `services/cron_scheduler.h`

- [x] Cron 表达式解析
- [x] 定时任务注册
- [x] 任务触发执行
- [x] 任务列表管理

---

## 十二、CLI 交互

### 12.1 CommandRegistry
**文件**: `cli/command_registry.h`

- [x] 斜杠命令注册
- [x] 命令执行路由
- [x] Tab 补全支持
- [x] 帮助信息

### 12.2 InputHandler
**文件**: `cli/input_handler.h`

- [x] 行输入读取
- [x] Tab 补全
- [x] 历史导航
- [x] Ctrl+C 处理

---

## 十三、子 Agent 系统

### 13.1 SubagentCoordinator
**文件**: `agents/subagent_coordinator.h`

- [x] 子 Agent 启动
- [x] 任务委派
- [x] 结果聚合
- [x] 技能执行
- [x] 工具搜索

---

## 功能状态总结

| 模块 | 完成度 |
|------|--------|
| 核心 Agent 系统 | 100% |
| LLM 提供者 | 100% (Anthropic, Qwen, Ollama) |
| 工具系统 | 100% (40+ 工具) |
| 权限管理 | 100% |
| 记忆和上下文 | 100% |
| 技能系统 | 100% |
| 计划模式 | 100% |
| LSP 服务 | 100% |
| MCP 协议 | 100% |
| 配置系统 | 100% |
| 会话管理 | 100% |
| CLI 交互 | 100% |
| 子 Agent 系统 | 100% |
| 定时任务 | 100% |
| 工作树管理 | 100% |

---

## 待扩展功能

基于代码分析，以下功能已预留接口但可能需要进一步扩展：

1. **更多 LLM 提供者**: 继承 `LLMProvider` 即可添加
2. **自定义工具**: 通过 `ToolRegistry.RegisterTool()` 注册
3. **远程技能**: 通过 URL 下载安装
4. **媒体引擎**: SDL/ImGui 集成（PRODUCT.md 中提及）
