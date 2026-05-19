# 模块: virtual_sprite — 桌面宠物应用

## 用途
完整的 SDL 窗口应用层——桌面宠物模式入口。拥有 sprite 窗口（动画像素宠物 + 语音气泡叠加）和共享的中央聊天窗口。

## 关键文件

| 文件 | 职责 | 实例化模式 |
|------|------|-----------|
| `virtual_sprite.h` / `virtual_sprite.cc` | `VirtualSprite` 单例（顶层应用，拥有中央窗口 + 注册全局处理器） | **单例** (`GetInstance()`)，有状态 |
| `sprite.h` / `sprite.cc` | `Sprite` 类（每个 sprite 的窗口，含宠物动画、语音气泡、拖拽、导航按钮、自有会话） | 常规类，`SpriteManager` 创建 |
| `sprite_manager.h` / `sprite_manager.cc` | `SpriteManager` 单例（管理多个 Sprite 实例，会话 ID 查询，"+New" 回调） | **单例** (`GetInstance()`)，有状态 |
| `chat_window.h` / `chat_window.cc` | 中央聊天窗口（消息历史 + 输入面板，跟随焦点 sprite） | 常规类，`VirtualSprite` 持有 |

## 模块组织

```
virtual_sprite/
│
├── virtual_sprite.h/.cc     → VirtualSprite 单例
│   ├── central_window_      → ChatWindow（主聊天界面）
│   ├── ↳ SpriteManager      → Sprite 容器
│   ├── input_callback_      → 外部输入转发
│   └── GlobalInit() 注册:
│       ├── RegUpdateHandler → OnUpdate()
│       ├── RegEventHandler  → 键盘映射
│       ├── SetOutputCallback→ Agent 状态 → Sprite
│       └── SetOnNewSprite   → "+New" 按钮
│
├── sprite_manager.h/.cc     → SpriteManager 单例
│   ├── sprites_[]           → Sprite unique_ptr 容器
│   ├── static: focused_session_ → 焦点追踪
│   └── static: on_new_sprite_   → "+New" 回调
│
├── sprite.h/.cc             → Sprite 类（每个 sprite 一个窗口）
│   ├── sprite_window_       → media_engine::Window（透明、无边框、置顶）
│   ├── session_id_          → 自有 AgentEngine 会话
│   ├── pet_sprite_          → Spritesheet 动画
│   ├── speech_bubble_       → 云朵对话框
│   ├── 鼠标处理器:
│   │   ├── 左键单击 → 切换导航栏 + 设置焦点
│   │   ├── 左键双击 → 切换中央窗口
│   │   └── 拖拽 → 全局鼠标位置 + 偏移量
│   └── 导航按钮: Prev/Next/+New
│
└── chat_window.h/.cc        → ChatWindow
    ├── window_              → media_engine::Window（主窗口，第一个创建的窗口）
    ├── InputPanel           → 消息输入 + 橙色发送按钮
    ├── ChatPanel            → 消息历史
    └── 根据 SpriteManager::focused_session_ 显示对应会话
```

## 代码组织关系

### 内部依赖链
```
┌───────────────────────────────────────────────────────┐
│               VirtualSprite 单例                        │
│  顶层应用 │ 中央窗口 │ 全局处理器注册                     │
└────────┬────────────────────┬──────────────────────────┘
         │                    │
    ┌────┴──────┐       ┌────┴──────────┐
    ▼           ▼       ▼               ▼
┌──────────────┐  ┌────────────────────────────────────┐
│ ChatWindow    │  │       SpriteManager 单例            │
│ 中央聊天窗口   │  │  sprites_[] 容器 │ 焦点追踪          │
│ (主窗口)      │  └────────────────┬───────────────────┘
│ InputPanel   │                   │
│ ChatPanel    │                   ▼
└──────────────┘  ┌────────────────────────────────────┐
                  │           Sprite 类（N 个实例）       │
                  │  ┌──────────────────────────────┐   │
                  │  │ sprite_window_（透明、置顶）    │   │
                  │  │ session_id_ → AgentEngine     │   │
                  │  │ pet_sprite_ → Spritesheet     │   │
                  │  │ speech_bubble_ → SpeechBubble │   │
                  │  │ 鼠标处理器: 单击/双击/拖拽     │   │
                  │  │ 导航栏: Prev/Next/+New        │   │
                  │  └──────────────────────────────┘   │
                  └────────────────────────────────────┘
```

### 创建与生命周期
```
VirtualSprite::GlobalInit()
  │
  ├── 1. central_window_.Create()    → SDL 主窗口（第一个创建）
  │
  ├── 2. create_sprite("Prosophor Assistant")
  │       └→ SpriteManager::CreateSprite()
  │             ├→ new Sprite → Sprite::Create()
  │             │    ├→ MC::CreateMediaWindow()  → 次级窗口
  │             │    ├→ RegMouseHandler          → 鼠标事件
  │             │    └→ RegRenderHandler         → 每帧渲染
  │             └→ sprites_.push_back(unique_ptr)
  │
  ├── 3. create_sprite("Mascot")     → 同上，位置偏移 (80,120)
  │
  ├── 4. RegUpdateHandler(OnUpdate)  → 全局动画更新
  ├── 5. RegEventHandler            → 键盘事件
  └── 6. SetOnNewSprite             → "+New" 按钮创建更多 sprite
```

### 事件流
```
用户输入
  │
  ├── 键盘事件 → RegEventHandler → VirtualSprite::HandleKeyDown
  │                                → input_callback_ → InputEvent
  │
  ├── sprite 鼠标事件 → RegMouseHandler(sprite_window_)
  │   ├── 左键单击 → SetFocusedSession + show_nav_popup_
  │   ├── 双击 → on_toggle_central_（切换中央窗口）
  │   └── 右键 → 全局上下文菜单
  │
  └── 中央窗口输入 → InputPanel::OnSubmit
                      → AgentEngine::SendUserMessage(focused_session_)
                               │
                               └→ SetOutputCallback → sprite::SetAgentState
```

## 依赖
- `media_engine/`（渲染、窗口、控件、颜色、纹理）
- `components/`（spritesheet, speech_bubble, ui_types）
- `prosophor_core/`（AgentEngine, AgentRuntimeState, RenderSnapshot）
- `common/`（日志、工具）
- `platform/`（字体路径）

## 责任约束
- **不**配置提供商或注册工具
- **不**包含核心 LLM 循环逻辑——委托给 AgentEngine
- **不**渲染场景角色（那是 scene/ 的事）
- 仅 UI 编排层——组合控件，不处理业务

## 实例化模式
- `VirtualSprite` — 单例（`Noncopyable` + 静态局部变量），有状态
- `SpriteManager` — 单例，有状态
- `Sprite` — 常规类，`SpriteManager::CreateSprite()` 创建，`unique_ptr` 持有
- `ChatWindow` — 常规类，`VirtualSprite` 直接持有

## 关键约定
- 每个 sprite 拥有自有会话（`session_id_`）
- 中央窗口第一个创建（成为 SDL 主窗口），所有 sprite 是次级窗口
- Sprite 窗口无边框 + 透明 + 跳过任务栏
- 状态到动作的映射通过 `StateToAction()` 用于宠物动画
- 跨层回调通过 `std::function`（`SetOnToggleCentralWindow`, `SetOnNewSprite`）
