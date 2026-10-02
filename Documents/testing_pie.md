# 五窗口联机测试手册（PIE 多进程）

> 对应 `design_analysis.md` 第 2 轮第 9 章与决策 D6：**不打包、不做真联机匹配**，最终验收形态 = 在 UE 编辑器中同时打开 **5 个 PIE 窗口**（1 个 Listen Server + 4 个 Client）。
> 本文是"照着做就能复现"的操作手册，实现轮完成后按此验收。

---

## 1. 一次性配置

### 1.1 编辑器 PIE 设置（推荐用配置落库，避免每次手点）

写入 `Config/DefaultEditorPerProjectUserSettings.ini`：

```ini
[/Script/UnrealEd.LevelEditorPlaySettings]
PlayNetMode=PIE_ListenServer
RunUnderOneProcess=False
PlayNumberOfClients=5
PlayNetDedicated=False
bLaunchSeparateServer=False
NewWindowWidth=960
NewWindowHeight=540
CenterConsoleCommand=1
```

也可以手动设置：**编辑器 → 播放按钮右侧下拉 → 高级设置（Advanced Settings）**，按下表填写：

| 设置项 | 值 | 说明 |
|---|---|---|
| Number of Players | **5** | 5 个窗口 |
| Net Mode | **Play As Listen Server** | 第 1 个窗口兼做服务器 |
| Run Under One Process | **关闭**（不勾） | 5 个独立进程，最接近真实联机；若端口冲突再改回勾选 |
| Separate Server | 关闭 | 本轮不用专用服 |
| New Editor Window (PIE) | 勾选 | 每客户端一个独立窗口 |

### 1.2 地图与 GameMode

`Config/DefaultEngine.ini`：

```ini
[/Script/EngineSettings.GameMapsSettings]
EditorStartupMap=/Game/IDF/Maps/L_MilitaryFactory
GameDefaultMap=/Game/IDF/Maps/L_MilitaryFactory
GlobalDefaultGameMode=/Script/IDMMatch.IDMGameMode
```

（实现轮创建 `L_MilitaryFactory` 与 `AIDMGameMode` 之后才生效；在此之前先用任意空关卡验证联机。）

### 1.3 网络模拟（可选，用来测预测与容差）

编辑器 → **播放 → 高级设置 → Network Emulation**：

| 档位 | 延迟 | 丢包 | 用途 |
|---|---|---|---|
| Disabled | 0 | 0 | 日常开发 |
| Average | 80ms | 1% | 常规验证 |
| Poor | 200ms | 5% | 验收"翻窗不错位、进度一致" |

也可以在运行中改（每个客户端窗口的控制台）：

```
net PktLag=150
net PktLoss=3
net PktOrder=1
```

---

## 2. 启动与检查

### 2.1 启动

1. 关闭 Live Coding 面板（**不要**在 PIE 运行中热重载，会导致所有窗口断线）；
2. 打开 `L_MilitaryFactory`；
3. 点 **播放（Play）**；
4. 编辑器会拉起 5 个窗口。第一个（标题通常是 `… (Server)` 或 `… (Listen Server)`）是服务器窗口。

### 2.2 每窗口自检（屏幕左上角调试信息应显示）

| 检查项 | 期望 |
|---|---|
| 阵营 | 1 个窗口显示"监管者"，4 个显示"求生者" |
| 角色 | 每个窗口都有 1 个可操控的占位角色 |
| 标签 | `State.Ready` 存在（`IDF.Debug.GAS 1`） |
| 网络模式 | `stat net` 显示 1 个 Server + 4 个 Client |

> 阵营分配规则（实现轮）：按 `PlayerState` 索引，索引 0 → 监管者，1~4 → 求生者。

---

## 3. 验收测试用例

### TC-01 移动同步

| 步骤 | 操作 | 期望 |
|---|---|---|
| 1 | 在 Client 2 窗口点击使其获得焦点，WASD 移动 | 该窗口角色移动；其余 4 个窗口在 200ms 内同步 |
| 2 | 在 Server 窗口重复 | 同 |

### TC-02 交互提示

| 步骤 | 操作 | 期望 |
|---|---|---|
| 1 | 走向假密码机（2.2m 内） | UI 出现"按住 F 破译" + 长按环 |
| 2 | 走出范围 | 提示消失 |
| 3 | 对着已破译完成的密码机 | 提示灰显，原因是"已破译" |

### TC-03 交互进度同步

| 步骤 | 操作 | 期望 |
|---|---|---|
| 1 | 在 Server 窗口按住 F 破译 | 破译进度条增长 |
| 2 | 同时观察其余 4 个窗口 | 进度条同步增长，误差 < 1% |
| 3 | 中途松开 F | 所有窗口进度条停止，服务器按规则处理 |

### TC-04 服务器裁定（防作弊）

| 步骤 | 操作 | 期望 |
|---|---|---|
| 1 | 控制台执行 `IDF.Debug.Interact.ForceRequest 0`（超距离强制请求） | 服务器日志记录拒绝原因；客户端表现立即停止，不出现"假进度" |
| 2 | 控制台执行 `IDF.Debug.Interact.ForceRequest 1`（非法标签请求） | 同上 |

### TC-05 延迟容差

| 步骤 | 操作 | 期望 |
|---|---|---|
| 1 | 对所有客户端设置 `net PktLag=150`、`net PktLoss=3` | — |
| 2 | 反复靠近/远离交互点并按住 F | 交互仍能稳定建立（容差 120cm 生效），不出现频繁拒绝 |
| 3 | 观察是否有位置回弹（橡皮筋） | 无剧烈回弹 |

### TC-06 断线与退出

| 步骤 | 操作 | 期望 |
|---|---|---|
| 1 | 直接关闭 Client 4 窗口 | 服务器与其他客户端不崩；该角色按第 1 轮 4.5 节策略处理 |
| 2 | 停止 PIE | 所有窗口正常关闭，无残留进程 |

---

## 4. 常见问题

| 现象 | 原因 | 处理 |
|---|---|---|
| 5 个窗口里只有 1 个能连上，其余报 "Failed to connect / Connection lost" | 多进程 PIE 端口冲突 | 二选一：① 勾选 **Run Under One Process**（单进程多窗口，仍走真实 UDP 复制）；② 保持多进程，把 `PlayNumberOfClients` 降到 2~3 逐步排查 |
| 只有一个窗口有画面，其余黑屏 | 单独进程启动慢 / GPU 资源竞争 | 等待 10~30 秒；把 `NewWindowWidth/Height` 降到 720p（1280×720 或更低） |
| 编辑器卡死 | 5 个窗口同时用 Lumen/Nanite 渲染 | 临时在 `DefaultEngine.ini` 用 `[/Script/Engine.RendererSettings]` 覆盖，或在每个窗口控制台执行 `r.DynamicGlobalIlluminationMethod 0`、`r.Lumen.DiffuseIndirect.Allow 0` |
| 日志刷屏看不清 | 5 个进程日志混在一起 | 用 `-log` 参数启动编辑器；或直接看 `Saved/Logs/` 下各进程的独立日志 |
| 输入不生效 | 窗口未获得焦点 | 先点击目标窗口再操作（若需要"后台窗口也响应"，只在本机上可用 `Slate.EnableGlobalHitTest 1` 之类的非常规手段，不建议） |
| 改了 C++ 后 PIE 断开所有窗口 | Live Coding 在 PIE 中重载 | **先停止 PIE 再编译** |
| 想单独调试服务器 | 需要专用服输出 | 用 `UnrealEditor.exe Identity5.uproject -server -log -game` 启动一个无头服务器，再用客户端连 `open 127.0.0.1:7777`（本轮不必做，留作 M7 压测） |

---

## 5. 常用控制台命令（实现轮落地）

| 命令 | 用途 |
|---|---|
| `IDF.Debug.Interact 1` | 屏幕显示所有交互点状态与当前交互提示 |
| `IDF.Debug.GAS 1` | 打印 ASC 的能力、标签、关键属性 |
| `IDF.Debug.Tags.Validate` | 校验 GameplayTag 命名规范与引用完整性 |
| `IDF.Debug.Teleport Survivor 1` | 传送测试（配合地图分区） |
| `IDF.Match.ForcePhase Playing` | 跳过 Warmup 直接进对局 |
| `IDF.Tuning.Set ChairPhase1Duration 28` | 运行时改手感参数（不持久） |
| `stat net` | 复制量与带宽 |
| `net PktLag=150` / `net PktLoss=3` | 网络模拟 |
| `showdebug abilitysystem` | GAS 内置调试（配合 Gameplay Insights） |

---

## 6. 与验收标准（AC-R2）的对应

| 验收项 | 对应测试用例 |
|---|---|
| AC-R2-2 五窗口联机 | 第 2 章 |
| AC-R2-3 移动同步 | TC-01 |
| AC-R2-4 阵营分配 | 第 2.2 章 |
| AC-R2-5 GAS 生效 | 第 2.2 章 + `IDF.Debug.GAS 1` |
| AC-R2-6 交互提示 | TC-02 |
| AC-R2-7 服务器裁定 | TC-04 |
| AC-R2-8 进度同步 | TC-03 + TC-05 |
| AC-R2-9 标签校验 | `IDF.Debug.Tags.Validate` |
| AC-R2-10 插件清单 | 见 `plugin_setup.md` |
