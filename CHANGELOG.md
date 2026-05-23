# Changelog

## [Unreleased] — 2026-05-23 TTS 多后端重构 + GPT-SoVITS 流式语音 + Windows 安装发布

本期重点：TTS 从单一服务迁移为 Provider 架构，新增 edge-tts 与 GPT-SoVITS 双后端；接入 GPT-SoVITS 本地 API 生命周期管理和 WAV/PCM 流式播放；VirtualSprite 在回复完成后自动触发语音合成；修复 ImGui Begin/End RAII 配对规则；增强托盘图标、窗口版本号、Release 日志和 Windows NSIS/Gitee 发布流程。净变化 +893 行。

### TTS Provider 架构
- 新增 `TtsProvider` 抽象基类，统一同步合成与流式合成接口
- 新增 `EdgeTtsProvider`，保留 edge-tts 子进程合成能力并作为默认后端
- 新增 `GptSoVitsProvider`，通过本地 HTTP `/tts` 接口支持普通 WAV 输出和 streaming_mode=2 流式输出
- `TtsSpeaker` 从 `services/` 迁移到 `providers/tts/`，改为多后端门面，支持运行时切换后端、语音选择、合成完成回调和音频 chunk 回调
- 非流式后端通过临时 WAV 文件读取方式提供统一的流式回退路径

### GPT-SoVITS 集成
- 新增 `GptSoVitsManager`，负责启动/停止 GPT-SoVITS `api_v2.py` 子进程、端口等待和进程存活检测
- `TtsConfig` 新增 `tts` 配置段，支持 `backend`、`gs_url`、`gs_install_path`、`gs_auto_start`、`gs_port`、参考音频路径/文本/语言和输入文本语言
- `GptSoVitsProvider` 支持参考音频参数、文本语言配置、WAV header 解析，以及从首包中剥离 PCM 数据后转发给播放器

### 语音播放接入
- `VirtualSprite::GlobalInit()` 绑定 `TtsSpeaker` 流式回调，创建 `AudioStreamer` 并推送 PCM chunk 播放
- Agent 回复进入 `COMPLETE` / `STREAM_MODE_COMPLETE` 后，自动对最终回复文本执行 `SpeakStream()`
- `media_engine.h` 导出 `audio_streamer.h`，便于上层窗口系统直接接入流式音频播放

### UI / ImGui 修复与增强
- 修复 `ScopedGuard` 对 ImGui `Begin()/End()`、`BeginChild()/EndChild()` 的配对规则：普通窗口和 Child 即使 Begin 返回 false 也会调用 End，Popup/Menu/Tab 仍按条件调用
- ChatPanel、SpeechBubble、NavBar、ChatWindow 等窗口/Child 使用新的 always-call RAII 规则，避免 ImGui 栈不匹配
- `DrawList` 新增 `CircleFilled`、`CircleOutline`、`Line` 绘制接口，颜色表新增 Cyan/Pink 科技色系
- ChatWindow 标题显示 `PROSOPHOR_VERSION`，语言切换后同步更新带版本号标题
- 托盘窗口改为预加载 `robot_icon.png` 纹理并绘制真实图标，不再绘制文字 P 占位
- Debug 构建重新启用精灵窗口轮廓和 hitbox 边框叠加

### 配置、路径与日志
- `ProsophorConfig::BaseDir()` 明确使用用户目录 `~/.prosophor` 作为可写配置目录，新增 `InstallConfigDir()` 表示 exe 同级只读配置目录
- 多处路径存在性判断改用 `FileExists()` / `DirExists()`，统一平台兼容行为
- Release 构建增加文件日志输出到 `~/.prosophor/log/log-YYYYMMDD.txt`，Debug 构建保持 stdout 日志

### 构建与发布
- Makefile 版本升级到 `0.6.3`，默认构建类型改为 `RelWithDebInfo`
- 新增 `package` 目标，使用 NSIS 生成 Windows 安装包 `Prosophor-<version>-win64-setup.exe`
- `deploy` 改为发布 NSIS 安装包到 GitHub Releases；新增 `deploy_gitee` 和 `deploy_all` 支持发布到 Gitee/双平台
- CMake 为 GUI/TUI 目标加入 `app.rc` 和 `app.ico`，Release/RelWithDebInfo/MinSizeRel 下设置 `WIN32_EXECUTABLE`
- 移除根 CMake 中旧的 assets 与 bin/.prosophor 额外安装步骤

### 资源清理
- 删除旧 ayaka 资产配置与 demo 截图资源
- 新增 Windows 应用图标资源 `main_src/resources/app.ico` / `app.rc`

### 文件统计
- 变更文件：46 个
- 新增：+1,202 行
- 删除：-309 行
- 净变化：+893 行

---

## [Unreleased] — 2026-05-20 托盘窗口独立字体 + 精灵背景纹理清理

本期重点：Tray 窗口独立字体配置（`use_shared_font=false`），避免共享 CJK 字体上下文冲突；移除 Sprite 中废弃的 `LoadBackground()` 方法和 `bg_texture_` 成员（精灵窗口不再使用静态背景纹理）。

### Tray 窗口
- `ChatWindow::CreateTrayWindow()` 设置 `cfg.use_shared_font = false`，托盘窗口不再共享 MediaCore 的 CJK 字体集

### 精灵系统清理
- 移除 `Sprite::LoadBackground()` 方法及 `bg_texture_` 成员
- 精灵窗口创建时不再加载 `solitude.jpg` 背景纹理（纹理层已由 pet spritesheet + Widget UI 替代）

### 文件统计
- 变更文件：3 个
- 新增：+1 行
- 删除：-13 行
- 净变化：-12 行

---

## [Unreleased] — 2026-05-19 ImGui API 命名空间重构 + i18n 国际化 + 精灵系统增强

本期重点：ImGui 工具函数全面命名空间化（`ImGuiFoo` → `ImGuiWindow::`/`Style::`/`Layout::`/`DrawList::`）；新增 i18n 国际化系统（JSON 翻译 + 运行时切换）；精灵系统增强（名字标签、显示名绑定、调试边框、悬浮感知）；ChatPanel 智能自动滚动；OpenAI thinking 格式更新；场景残余清理。净变化 +2,607 行。

### ImGui Widget 命名空间重构
- 所有扁平工具函数（`SetImGuiNextWindowPos`、`ImGuiBegin`、`PushStyleColor` 等）归入命名空间类：`ImGuiWindow::`、`Style::`、`Layout::`、`Scroll::`、`Child::`、`DrawList::`
- 新增 `DrawList` 类：`RoundRect`、`OverlayRectOutline`、`Text`、`Panel`、`ResizeGrip` 等静态绘图方法
- `ImGuiCond_*` 常量从 0..3 改为 power-of-two 位掩码（匹配 ImGui 原生枚举）
- 新增 `ImGuiWindowFlags_MenuBar` 标志常量
- 移除冗余自由函数：`IsItemHovered`、`IsItemActive`、`GetMouseDragDelta`、`ResetMouseDragDelta`、`SetMouseCursor`

### 国际化 (i18n) 系统
- 新增 `common/i18n.{h,cc}` — I18n 单例，JSON 翻译文件懒加载 + LRU 缓存
- 新增 `config/.prosophor/lang/en.json`、`zh-CN.json` 双语言翻译文件
- `VirtualSprite::GlobalInit()` 自动初始化 `zh-CN` 翻译
- `I18n::SetLanguage(lang)` 运行时切换语言，缓存已加载的翻译表

### 精灵系统增强
- **名字标签 Widget**: `Sprite` 从 `DrawTextRect` 裸绘改为 `Label` widget（`name_label_`），奶油底色 + 橙色文字
- **DisplayName 绑定**: `LoadSpriteBindingFromRole()` 返回 `display_name` 字段，role JSON 可配置精灵显示名
- **轮廓调试**: 非 Debug 构建下窗口轮廓 + 精灵 hitbox 叠加半透明边框（`#ifndef NDEBUG`）
- **悬浮感知**: 新增 `SetHovering()` 和 `LEAVE` 鼠标事件处理
- **气泡切换**: 新增 `ToggleSpeechBubble()` 公开方法，右键菜单切换到按精灵逐个体控制气泡
- **气泡设置**: `SpeechBubble` 标题栏和 assistant 显示名使用精灵名称；输入框圆角可配置（`bubble_radius`）
- **访问器增强**: 新增 `GetName()`、`GetCurrentPetSlug()`、`GetCurrentPetName()`、`GetSpritesheetPath()`
- **Spritesheet 增强**: 新增 `GetActionFps()` 逐动作帧率；`SpritesheetAction::COUNT` 枚举总行数；`file_path_` 纹理路径追踪

### 聊天面板智能滚动
- ChatPanel 自动滚动改为基于消息计数变化触发（`last_msg_count_`），流式过程中用户可自由拖拽滚动条
- ChatPanel 滚动条宽度收窄至 3.0f
- 新增 `SetSnapshot()` 方法，session 切换时自动清理暂存消息
- SpeechBubble 内 ChatPanel 用户/助理消息背景设为透明

### OpenAI Provider 更新
- `enable_thinking` 字段替换为 `thinking.type`（`"enabled"`/`"disabled"`），对齐最新 OpenAI API
- `OpenAIProvider` 构造函数移除 `enable_thinking_` 参数，handler 级别不再控制 thinking
- `OpenAIStreamHandler` 不再需要 `enable_thinking_` 标志
- `reasoning_content` 解析不再受 `enable_thinking_` 门控
- 默认 `thinking: false`

### 场景残余清理
- 移除 `scene/ui_renderer.cc/h`（功能迁至 `virtual_sprite/ui_renderer`）
- `asset_define.h`、`layout_config.h` 从 `scene/` 移至 `virtual_sprite/`
- CMake 构建移除 `scene/` 目录扫描和 include 路径
- `UIRenderer` 重构：新增 `SetOnToggleChat(WindowCallback)`（按窗口切换气泡）、`SetOnShowMainWindow`、`SetOnOpenSettings`、`SetOnNewSprite` 回调；`SetOnToggleChat` 签名从无参改为 `Window*` 参数
- ESC 键仅隐藏中央窗口（不再切换上下文菜单）

### 颜色扩展
- 奶油色系: `Cream70`、`CreamTranslucent`、`CreamOpaque90`
- 橙色系: `OrangeLightest`、`OrangeWarm`、`OrangeDeep`
- 聊天气泡色: `BluePale`、`GreenPale`、`Gray20a`
- 白色系: `White80`

### 工具函数新增
- `time_wrapper.h`: `FormatCurrentTime(format)` 自定义格式时间戳；`GetCurrentEpochSeconds()` 秒级时间戳
- `sprite_manager`: `FindByWindow()` 窗口指针查找精灵；`GetFocusedSpriteName()` 焦点精灵显示名
- `agent_role_loader`: role JSON 解析新增 `display_name` 字段

### 构建 & 配置
- CMake: MinGW 安装器包含 SSL ca-bundle.crt 证书包
- Makefile: 包名正式改为 `Prosophor`，版本 `0.6.0`；移除 `release` 目标；`deploy` 简化从 install/bin 直接打包；`run_win` 自动传递 `SSL_CERT_FILE=ca-bundle.crt`
- settings.json: 默认 `thinking` 改为 `false`

### 文件统计
- 变更文件：54 个
- 新增：+3,486 行
- 删除：-879 行
- 净变化：+2,607 行

---

## [Unreleased] — 2026-05-17 角色配置全面 JSON 化 + 新增 6 个角色精灵

本期重点：角色配置全面 JSON 化 + 新增 6 个角色精灵；对话摘要策略重构（贝尔曼衰减）；SpeechBubble/ChatPanel 接入 Widget 渲染树；多精灵管理器动态注册；ImGui 抽象层扩展；Provider 流处理优化；CPack 打包与一键部署。净变化 +328 行。

### 角色系统 JSON 化 & 多角色精灵
- 角色配置从 Markdown (YAML front-matter) 迁移为纯 JSON 格式，结构更清晰、解析更可靠
- 新增 6 个角色：ayaka、kazuha、keqing、sayu、skirk-2、linnea-2，每个绑定独立精灵图 (.webp)
- 默认角色改为多角色组合 `["ayaka", "sayu", "keqing"]`，支持多角色轮换
- 新增精灵图资产目录 `config/.prosophor/assets/`
- Providers agents 字段从 object 改为 array，支持同名 agent 多配置

### 对话系统重构
- `compact_service.{h,cc}` 重命名为 `dialog_strategy.{h,cc}`，统一对话摘要策略抽象
- `ApplyDialogStrategy()` 逻辑重构：支持贝尔曼衰减摘要 + 角色摘要开关
- `AgentCore` 接入新 `DialogStrategy` 接口，移除内联摘要逻辑

### SpeechBubble / ChatPanel 重写
- `SpeechBubble` 继承 `Widget` 渲染树，接入坐标级联系统
- InputPanel / ChatPanel 作为 Widget 子节点，通过 `SetPixelRect()` 直接定位
- 双模式渲染：compact 不透明气泡 + maximized 半透明全屏
- 自动滚动分离：聊天面板智能跟随（在底部时自动滚），气泡内强制滚到底
- 消息间距缩紧、文本颜色统一为 `Gray40`

### 精灵系统增强
- `Sprite` 渲染循环重构，支持多精灵管理器注册
- `SpriteManager` 迭代器注册机制，支持动态添加/移除精灵
- `ChatWindow` 集成新 SpeechBubble，双击切换显示

### 媒体引擎扩展
- **共享字体**: MediaCore 级共享 CJK TTF 数据，多窗口只读一次磁盘
- **ImGui 抽象层**: 新增弹出窗口 (`OpenPopup`/`BeginPopupModal`)、标签栏 (`TabBar`/`TabItem`)、控件 (`Checkbox`/`Combo`/`InputText`/`Button`)、窗口查询 (`GetWindowPos`/`GetScrollMaxY`/`GetScrollY`)
- `Window::SetTitle()` 接口、`SetImGuiNextWindowSize` 条件参数、`ImGuiCond_*` 常量
- `Widget::SetPixelRect()` 直接像素坐标设置，跳过百分比解算

### Provider / Network
- **OpenAI StreamHandler**: `enable_thinking_` 在 handler 级别控制，`Deserialize` 非 thinking 时跳过 `reasoning_content`
- **Anthropic StreamHandler**: 移除冗余 `stop_reason`/`usage` 成员变量，直接写入 `accumulated_response`
- **curl SSL**: 支持 `SSL_CERT_FILE` 环境变量，未设置时关闭对端证书验证
- 修复 `HttpClient::Post` 调试日志输出原始 `res_body` 而非已移动的 `response.body`

### 构建 & 打包
- **CPack 打包**: 集成 CPack，Windows 下优先 NSIS 安装器，降级 ZIP
- **MinGW DLL 捆绑**: 自动复制 libcurl、OpenSSL、zlib 等 20+ 运行时 DLL
- **便携部署**: config 同时安装到 `bin/.prosophor/` 作为备选查找路径
- **一键发布**: Makefile 新增 `deploy` 目标 (cmake --build → cpack)
- 编译定义 `PROSOPHOR_SOURCE_DIR` 暴露源码路径

### 杂项
- `/help roles` 改用 `.json` 文件自动补全
- `ProviderRouter` 默认角色从 string 改为 vector 取第一个
- 重构后 `dialog_strategy.h` 新增 121 行，`dialog_strategy.cc` 净变更 235 行

---

## [2026-05-16] - 桌面宠物精灵窗口系统与多窗口架构

### 新增
- **桌面宠物精灵窗口系统**：Sprite 独立窗口，替代旧式场景渲染
  - `Sprite` 类：独立 SDL 窗口 + ImGui 上下文，拥有自己的 session、pet spritesheet 渲染、拖拽、动画循环
  - `Spritesheet` 精灵表渲染：9 种动作（IDLE/RUN/WAVE/JUMP/FAILED/WAIT/SPRINT/REVIEW），帧动画播放
  - `SpeechBubble` 漫画式气对话框：气泡主体 + 三角尾巴 + 标题栏 + 输入区 + 缩放手柄，支持 compact/maximized 模式
  - `SpriteManager` 多精灵实例管理：`CreateSprite`/`FindBySessionId`/`UpdateAll`
  - 支持拖拽移动、双击切换中央窗口、自动漫游（IDLE 时左右走）
  - `agent_state_observer`/`anime_character` 替换为精灵系统的状态 → 动作映射
- **聊天窗口独立化**：`ChatWindow` 独立 SDL 窗口，与精灵窗口分离
  - 使用 `ChatPanel` + `InputPanel` 构建完整聊天 UI
  - 支持显示/隐藏、托盘图标切换
- **新命令**：
  - `/workspace`（别名 `/ws`）— 设置/查看当前工作区路径
  - `/skills` — 列出和查看可用的技能文件（`/skills list` / `skills show <name>`）
  - 动态技能命令注册（从 `~/.prosophor/skills/` 自动加载）

### 重构
- **多窗口架构**：`media_engine::Window` 完整窗口抽象
  - SDL_Window + Renderer + ImGui 上下文三合一封装
  - `WindowConfig` 支持透明背景、无边框、跳过任务栏、置顶等模式
  - `MediaCore` 升级为多窗口管理器，支持独立帧循环
  - 消除旧版单窗口假设，Window 通过 `BeginFrame`/`EndFrame`/`RenderFrame` 全包/分步渲染
  - 新增 `sdl_interface/window.cc` 实现（225 行）
- **VirtualSprite 精简**：从 monolithic 单体重塑为 `SpriteManager` + `ChatWindow` 协调者
  - 移除 `scene/agent_state_observer.cc/h`（由精灵状态映射替代）
  - 移除 `scene/anime_character.cc/h`（由 Spritesheet 精灵表替代）
  - 移除 `scene/character_sprite.cc/h`、`scene/galgame_mode.cc/h`、`scene/home_screen.cc/h`、`scene/office_*.cc/h`、`scene/pixel_character.cc/h`
  - 移除 `scene/character_state_observer.cc/h`
- **Widget 百分比布局树**：新增 `media_engine::Widget` 基类替代 `UIContainer`
  - 全百分比坐标系统：`SetRoot(px,py)` / `SetPosition(x%,y%,w%,h%)`
  - 父尺寸变化自动级联 ResolveSelf 至整棵布局树
  - 新增 `Label`（像素文本标签）、`NavBar`（底部导航栏）子类
  - 移除 `ui_container.cc/h`（由 Widget 替代）
- **ImGui Widget 大幅精简**：`imgui_widget` 从全组件库精简为仅保留 Button
  - 命名空间 `imgui_widget::` → `media_engine::`
  - 移除：`IconButton`、`Slider`、`Checkbox`、`ColorPicker`、`ComboBox`、`ProgressBar`、`Image`、`InputText`
  - `imgui_widget.cc` 从 ~1052 行减至约 ~400 行，去除 ~650 行冗余 ImGui 包装
- **`imgui_widget.h` 平台无关化**：所有实现移至 `sdl_interface/`，media 层仅保留 `VoidCallback`/`StringCallback` 类型别名和有限的前置声明
- **LayoutConfig 重构**：移除废弃的办公区/场景布局方法，新增精灵窗口尺寸、宠物配置、托盘配置字段
- **`ui_renderer.cc/h` 精简**：适配新架构，移除场景渲染残余

### 改进
- **Colors 增强**：新增 `Color::Slot` ImGui 样式颜色槽索引枚举；扩展灰度色板（GrayBlack/GrayNearBlack/GrayDarkest/Gray20 等）；所有颜色纳入 `media_engine` 命名空间
- **ChatPanel 增强**：新增 `RenderContentInRect()` 方法，支持在指定矩形区域内渲染消息（供 SpeechBubble 使用）
- **HeaderBar/InputPanel/UIPanel 重构**：接入 Widget 布局树，移除硬编码坐标
- **AgentEngine 增强**：多会话 continue 支持，session 状态管理优化
- **配置**：`enable_summary` 默认开启 (`true`)
- **构建**：`Makefile` 新增 `release` 打包目标、`run_llamacpp_server`/`stop_llamacpp_server` 目标；`CMakeLists.txt` 适配新文件结构
- **命令注册**：`command_registry.cc` 新增 `/workspace`、`/skills` 命令及动态技能命令加载

### 清理
- 移除约 3000+ 行废弃代码（旧场景系统、旧角色渲染器、旧布局系统、冗余 ImGui 包装）
- 移除 `time_wrapper.h` 中废弃的函数声明
- 移除 `ui_container.cc/h`

### 文件统计
- 变更文件：76 个
- 新增：+3,708 行
- 删除：-3,380 行
- 净变化：+328 行

---

## [2026-05-11] - 对话摘要系统与上下文压缩重构

### 新增
- **对话摘要系统**: 零额外 API 调用的贝尔曼衰减摘要循环
  - `ApplyDialogStrategy()` 在每轮请求前注入时间戳 + 历史摘要 + 摘要生成指令
  - `ExtractDialogSummary()` 在响应后提取 `[摘要]` 标签内容存入 `MessageSchema::summary`
  - 递推公式：`每轮摘要 = 本轮内容 + γ × 上轮摘要`，关键决策不衰减
  - 新增 `enable_summary` 配置开关（settings.json 根字段 + role 元数据），可在 settings.json 全局关闭
  - system prompt 新增摘要生成指令模板

### 重构
- **上下文压缩策略替换**: 从 LLM 调用压缩（`MaybeCompact`）改为对话摘要 + 简单截断
  - 移除 `AgentCore::MaybeCompact()`（曾额外调用 LLM 生成摘要）
  - 新增 `AgentCore::ApplyDialogStrategy()` / `ExtractDialogSummary()` 替代 LLM 压缩
  - 上下文过大时仅保留最近 2 条消息（1 轮完整 user+assistant），不再调用 LLM 压缩

### 改进
- **API 错误处理优化**:
  - `agent_core.cc`: API 错误时调用 `CleanupInterruptedLoop()` 移除孤立 user 消息
  - `ai_coding.cc`: STATE_ERROR 状态输出具体错误内容到终端
  - `curl_client.cc`: 移除重复的 `LOG_ERROR`，错误信息由调用方统一处理
  - Provider 错误信息格式统一（`AnthropicProvider::ChatStream` / `OpenAIProvider::ChatStream`）
- **配置**: `settings.json` 新增 `enable_summary: false`，`auto_start` 默认关闭

### 文件统计
- 变更文件：14 个
- 新增：+104 行
- 删除：-47 行
- 净变化：+57 行

---

## [2026-05-10] - 清理 scene 界面

### 重构
- **角色渲染系统重构**: `AnimeCharacterRenderer` 从纯程序化绘制（DrawHair/DrawBody/DrawEyes 等多方法）重构为 PNG 立绘优先 + Q 版胸像回退的双层架构
  - 新增 `Texture` 纹理缓存和 `LoadPortraitTexture` 懒加载机制
  - 新增 `GetPalette` 统一调色板、`RenderWithTexture` 纹理渲染、`DrawFallback*` 回退绘制子方法
  - PNG 立绘支持呼吸浮动、脉冲缩放、状态色调覆盖、Error 红 X 覆盖、眨眼覆盖
  - Q 版回退从全身像精简为胸像（大眼可爱风格），移除了腿部/手臂/配件等全身绘制逻辑
  - 移除了 ~1500 行程序化绘制代码（5 角色 x 全身各部位）
- **场景模式精简**: 移除多模式 UI 系统（HOME/GALGAME/VIRTUAL_HUMAN/TERMINAL），固定为虚拟人交互模式
  - 删除 `HomeScreen`, `GalgameScene`, `OfficeBackground`, `OfficeCharacterManager`, `CharacterSpriteRenderer`, `CharacterStateObserver`, `PixelCharacterRenderer` 及其头文件（7 对文件）
  - 删除模式切换逻辑 `SwitchMode()`、ESC 快捷键切换、`saved_callback_` 保存/恢复
  - 删除 GALGAME 模式的像素风格角色绘制、教室背景、对话系统和 WASD 操作
  - 删除办公室 2D 俯视场景（地板/墙壁/门/窗/桌子/电脑/椅子/盆栽绘制）
  - 删除 BFS 寻路、角色状态机（IDLE/WALK/TYPE/READ）、瓷砖地图、精灵图 PNG 加载与帧裁剪
  - 删除 `CharacterStateObserver` 的 AgentRuntimeState→CharacterState 映射和 SetCurrentTool
  - 删除 `UIMode` 枚举及相关回调
- **聊天面板采用快照驱动**: `ChatPanel` 从内部消息存储（`messages_`、`AddMessage`/`UpdateLastMessage`/`ClearMessages`/`StartAssistantMessage`）重构为外部 `RenderSnapshot` 驱动
  - 支持 thinking 块渲染（先显示 thinking 再显示 text）
  - 支持流式 thinking/text 实时渲染
  - `RenderContent()` 签名改为 `RenderContent(const RenderSnapshot&)`
- **UI 渲染器解耦**: `UIRenderer` 移除对 `ChatPanel` 的直接消息操作转发，改为 `SnapshotGetter` 回调获取渲染快照
  - 移除了 `SubmitUserMessage()`, `SendToChatPanel()`, `UpdateLastMessage()`, `StartAssistantMessage()`, `ClearHistory()`
  - 移除了 `SetAgentState()`，改为从 snapshot 获取 state
- **场景背景重构**: `AgentStateVisualizer::DrawBlackboard()` 从程序化黑板绘制（木框+深绿渐变+粉笔字+粉笔槽）改为 JPG 壁纸渲染（支持拉伸/平铺/纯色 fallback）
  - 新增 `LoadBackground()` 从 `BackwallDir()` 加载 `solitude.jpg`
  - 新增 `bg_texture_` 纹理成员
- **自动创建会话**: VirtualSprite 初始化时自动创建默认会话，确保 UI 启动即就绪

### 改进
- **ImGui 标志更新**: 窗口/子窗口标志常量更新为 ImGui 1.92.8 标准值，新增完整标志位定义
- **ScrollWindow 嵌套修复**: 从 `ImGuiBegin()` 改为 `BeginChild()`，正确处理在父窗口内的嵌套滚动
- **窗口最大化**: `SDL_CreateWindowAndRenderer` 增加 `SDL_WINDOW_MAXIMIZED` 标志，启动时窗口最大化
- **窗口缩放响应**: 新增 `SDL_EVENT_WINDOW_RESIZED` 事件处理，自动更新逻辑尺寸和 Letterbox
- **字体路径平台化**: `platform.h` 新增跨平台 `kDefaultFontPath` 常量（Windows `msyh.ttc` / Linux `DroidSansFallbackFull.ttf`）
  - 所有硬编码 `"C:/Windows/Fonts/msyh.ttc"` 引用替换为 `platform::kDefaultFontPath`
- **Color 结构体传参**: `MediaUtil::DrawTextRect` 及相关函数从分离的 r/g/b/a 参数改为 `Color` 结构体传参
- **资源路径统一**: `asset_define.h` 从 `AssetDefine` 单例模式简化为内联辅助函数，使用 `PROSOPHOR_SOURCE_DIR` CMake 编译定义
  - 移除了 EMSCRIPTEN 平台条件编译
  - 新增 `AssetBase()`, `PortraitDir()`, `BackwallDir()`, `ImageDir()`, `SoundDir()`, `MusicDir()`, `FontDir()`, `EffectDir()` 函数
- **字符类型从角色 ID 映射**: 新增 `AnimeCharacterTypeFromRoleId()` 内联函数，按 role_id 字符串选择立绘类型
- **agent_state_observer**: 在 `GetOrCreate` 时根据 role_id 设置角色类型

### 清理
- 移除约 3000+ 行废弃代码（旧场景系统、旧渲染器、模式切换、精灵图系统）
- 移除中文注释噪音（不必要的块注释）

---

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
