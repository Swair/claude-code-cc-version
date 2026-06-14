# 模块: components — 可组合 UI 控件

## 用途
`media_engine` 之上的可复用 UI 控件层。所有组件都是独立的封装单元——不因"简单"而内联，不因"复杂"而特殊。

## 组件体系

```
底层原语（media_engine/）
═══════════════════════════════════════════
PanelContainer          ← 页面壳（白底圆角 + 标题 + 内容区）
  ├── TitleBar          ← 子组件：OrangeDeep 标题
  ├── Background        ← 子组件：圆角矩形 + 描边
  └── ContentArea       ← 子组件：内容区坐标计算

DrawList                 ← 2D 绘图原语
  ├── RoundRect         ← 填充圆角矩形
  ├── RoundRectOutline  ← 圆角矩形描边
  ├── Text              ← 文字
  ├── Selection         ← 选中态：色条 + 填充
  ├── ChannelsSplit     ← 绘制通道分层
  ├── ChannelsSetCurrent
  └── ChannelsMerge

通用组件（components/）
═══════════════════════════════════════════
容器类：
  BorderedContainer     ← ScopedChild + Borders + 可选底色 + 圆角
  ItemList              ← 可选中列表容器（Y 追踪 + 列表项）
  SelectableItem        ← 列表项（InvisibleButton + 三态视觉）

卡片体系（Card 为核心，参数化配色）：
  Card                  ← 背景 + 边框 + 标题（上方/内部）+ Field() + 自适应/固定高度
                          ChannelsSplit 实现正确图层顺序
  WhiteCard             ← White + CreamBorder（简化封装，title_above=false）
  FocusCard             ← OrangeLightest + OrangeWarm（三态交互，固定高度）
  StatCard              ← White + CreamBorder + 左侧色条

布局/操作辅助：
  SplitPanel            ← 左右分栏坐标计算
  ActionBar             ← 底部 Save/Cancel 按钮栏
  SectionScroll         ← [已删除] 被 Card 取代
  PlaceholderPage       ← 占位页（Coming Soon）
  PanelHelper           ← 存量 helper（LabelRow / SectionCard / Spacing 等）

业务卡片（panels/ 层）：
  ModelCard             ← BorderedContainer + model/temperature/max_tokens/context_window
  ProviderEntryCard     ← BorderedContainer + 字段 + ModelCard × N
```

### Card 参数详解

```cpp
Card(x, y, w, title, sm = 1.0f,
     bg_color = Beige,
     border_color = Gray63,
     title_color = OrangeDeep,
     radius = 6.0f,
     fixed_h = 0,          // 0=自适应，>0=固定高度
     title_above = true);   // true=标题在边框上方，false=在内部
```

- 构造时：ChannelsSplit(2)，内容渲染在通道 1（前景）
- 析构时：通道 0 绘制背景 + 边框（背景层）
- 图层顺序：背景 → 标题 → 内容字段 → 边框

### Container vs Card 命名约定

| 后缀 | 含义 | 可复用性 |
|------|------|---------|
| `*Container` | 通用容器，不绑定业务 | 高，任何视图可用 |
| `*Card` | 定制卡片，绑定具体业务 | 低，仅特定视图使用 |

## 文件清单

| 文件 | 职责 | 使用方 |
|------|------|--------|
| `bordered_container.h/.cc` | 带边框 ScopedChild，可选底色/圆角/定位 | ModelCard, ProviderEntryCard |
| `selectable_item.h/.cc` | 三态列表项（内部使用 SelectableItem） | ItemList |
| `item_list.h/.cc` | 可选中列表容器 | config/providers/roles view |
| `panel_kit.h/.cc` | Card, SplitPanel, ActionBar, FocusCard, WhiteCard, StatCard, 等 | 所有 panel |
| `sidebar.h/.cc` | 左侧导航栏 | chat_window |
| `chat_panel.h/.cc` | 聊天消息历史 | chat_window |
| `speech_bubble.h/.cc` | 语音气泡 | sprite |
| `spritesheet.h/.cc` | 精灵表动画 | sprite |
| `status_bar.h/.cc` | 底部状态栏 | virtual_sprite |

## 关键约定
- 所有组件在 `prosophor` 命名空间
- 构造即绘制（ImGui 立即模式），析构执行收尾操作
- 不因"简单"而内联——有名字的概念就应封装
