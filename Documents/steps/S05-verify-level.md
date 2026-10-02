# S05 验证步骤：建测试关卡并放置 GAS 测试 Actor

> 对应 `implementation_roadmap.md` 的 S05，是 [S05-gas-skeleton.md](S05-gas-skeleton.md) 第 6 节自测清单的**前置动作**。
> **为什么现在就要建这张图**：S05 的验证要跑 PIE，而 PIE 必须有一张地图。而且这张图**不是一次性的**——S15 会在这同一张 `L_MilitaryFactory` 上做军工厂灰盒，现在先把"地面 + 光照 + 出生点"打好底，后面直接往上加分区。
> 本节预计 1 小时。

---

## 0. 前置检查（先确认工程能编译）

在打开编辑器之前，确认：

```powershell
# 1) 解决方案里应该有 IDFCore（上一轮重新生成过工程文件）
Select-String -Path 'G:\Project\Unreal Projects\Identity5\Identity5.sln' -Pattern 'IDFCore' | Select-Object -First 3

# 2) 插件的 dll 应该是新模块名（不是旧的 IDFCoreRuntime）
Get-ChildItem 'G:\Project\Unreal Projects\Identity5\Plugins\GameFeatures\IDFCore\Binaries\Win64' -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name
```

期望：第 1 条有输出；第 2 条能看到 `UnrealEditor-IDFCore.dll`（若目录不存在，说明还没编译，先去编辑器编译一次）。

> ⚠️ **编译不过就别往下走**。如果 `IDFGasTestActor` / `IDFHealthSet` 有编译错误，先解决——本节的步骤依赖它们能被拖进关卡（拖不进去通常就是类没编译成功，
> 表现是 Content Browser 里搜不到该 Actor 类型）。

---

## 1. 建目录与关卡

### 1.1 先建目录（省得保存时手忙脚乱）

```
Content Browser → 右键 Content → New Folder → 命名 IDF
Content Browser → 右键 IDF     → New Folder → 命名 Maps
```

最终路径：`Content/IDF/Maps/`（与 `Config/DefaultEngine.ini` 里已经配好的 `/Game/IDF/Maps/L_MilitaryFactory` **完全对应**）。

### 1.2 新建关卡（用引擎模板，这样光照是现成的）

```
菜单 File → New Level...
  → 选 "Basic"（有的版本显示为 "Basic / Default"，图标是一块地面 + 一个方向光）
  → Create
```

**为什么用 Basic 而不是 Empty Level**：`Empty Level` **没有光照、没有天空、没有地面**——你会看到一片纯黑，然后花时间怀疑是不是渲染坏了。Basic 模板自带：一块地面、Directional Light、Sky Atmosphere、Sky Light、Exponential Height Fog，正好是我们需要的底子。

### 1.3 立刻另存到目标路径

```
菜单 File → Save Current Level As...
  → 路径选 Content/IDF/Maps
  → 文件名填 L_MilitaryFactory
  → Save

然后 File → Save All（★ 别只按 Ctrl+S 就以为存了）
```

> **为什么强调 Save All**：新建关卡时引擎会同时生成若干子资产（如世界设置相关资源），只保存关卡本身会留下"未保存的资产"提示，下次打开可能丢东西。

### 1.4 确认配置已经指向这张图

因为 S03 已经把配置写好了（`/Game/IDF/Maps/L_MilitaryFactory`），现在这张图一存在，配置就自动生效：

```
菜单 Edit → Project Settings → Project → Maps & Modes

预期显示：
  Editor Startup Map  = L_MilitaryFactory
  Game Default Map    = L_MilitaryFactory
  Default GameMode    = Identity5GameMode
```

**如果这三项显示的是别的**（例如 `OpenWorld` 或空白），说明 `Config/DefaultEngine.ini` 没生效或被 `Saved/Config/` 覆盖——回到 `steps/S03-project-config.md` 排查。

---

## 2. 整备地面（Basic 模板已有一块，按需调整）

Basic 模板自带的地面可能偏小。为了 S05 的测试（以及将来灰盒的底子），建议把它整理成一块明确的大平地：

| 项 | 建议值 | 说明 |
|---|---|---|
| 地面 | 直接用模板自带的 Floor，或自己拖一个 | 也可以自己放：Content Browser → 打开 **View Options → Show Engine Content** → 搜索 `Plane`，路径是 `/Engine/BasicShapes/Plane` |
| 缩放 | `X = 100, Y = 100, Z = 1`（Plane 默认 100×100 单位） | 得到 100m × 100m 的地面，够 S05 用，也够 S15 起步 |
| 位置 | `(0, 0, 0)` | 便于后面按坐标摆分区 |
| 材质 | 默认即可 | S15 再统一换灰盒材质 |
| 碰撞 | 必须为 `BlockAll`（默认就是） | 没碰撞角色会直接掉下去 |

> **可选**：如果你想顺便为将来的灰盒做准备，可以把地面换成 `Cube`（`/Engine/BasicShapes/Cube`，缩放 `X=100, Y=100, Z=1` → 100m×100m×1m 的厚板），Cube 的碰撞体比 Plane 更"厚"，角色从边缘掉下去的概率更低。

### 2.1 检查光照（Basic 模板通常已带，缺了就补）

在 **Outliner** 里确认至少有这些 Actor；缺哪个就用左上角 **+ 添加** 按钮加：

| Actor | 作用 | 缺了会怎样 |
|---|---|---|
| `DirectionalLight` | 主光源（太阳） | 全黑 |
| `SkyAtmosphere` | 天空与大气散射 | 背景是纯黑，没有天空 |
| `SkyLight` | 环境光（间接照明） | 阴影里死黑，看不清楚 |
| `ExponentialHeightFog` | 远景雾 | 可选，不影响验证 |

> `DirectionalLight` 的 **Mobility 建议设为 `Movable`**（细节面板 → Transform → Mobility）。`Static` 需要烘焙光照，我们这种灰盒测试不需要，还容易看到"未构建光照"的提示。

---

## 3. 放一个 PlayerStart

```
左上角 "+" 添加 → 搜索 PlayerStart → 拖到地面上方一点（Z 大约 100~200）
```

**为什么现在就放**：PIE 时 `AGameModeBase::ChoosePlayerStart` 会找 `PlayerStart`；找不到时会在世界原点附近生成（可能卡在地面里）。S09 做 5 窗口联机时需要 **5 个** PlayerStart，现在先放 1 个不冲突，后面复制 4 个即可。

放在地面**上方**而不是地面上，避免角色出生瞬间和地面碰撞体互相挤压。

---

## 4. 放置 `AIDFGasTestActor`

### 4.1 从 Content Browser 拖

```
Content Browser → 右上角设置（齿轮）→ 勾选 "Show C++ Classes"（有时叫 Show Engine/C++ Content）
  → 筛选器选 "C++ Classes" → 展开 IDFCore → 找到 AIDFGasTestActor
  → 拖到地面的上方
```

> **找不到这个类？** 依次检查：
> 1. `IDFGasTestActor.h/.cpp` 是否真的建在 `Plugins/GameFeatures/IDFCore/Source/IDFCore/Public/Debug/` 与 `Private/Debug/` 下；
> 2. 插件是否编译成功（`Plugins/GameFeatures/IDFCore/Binaries/Win64/` 下有 dll）；
> 3. 编辑器是否重启过（新类需要重启才能出现在 C++ Classes 里）；
> 4. `UCLASS()` 上的宏是否是 `IDFCORE_API`（写成 `IDENTITY5_API` 会链接失败，类不可用）。

### 4.2 给测试 Actor 加一个视觉标记（否则它是个"看不见的 Actor"）

`AIDFGasTestActor` 继承 `AActor`，**默认没有网格**，所以在视口里是个只有图标没有实体的东西。两种做法：

| 做法 | 说明 | 推荐度 |
|---|---|---|
| **只靠日志验证**（最省事） | 不建网格，只看 Output Log 与屏幕调试输出 | ⭐ 先做这个，S05 的验证本来就看日志 |
| 加 `UTextRenderComponent` 显示血量 | 能在世界里直观看到 `100.0 / 100.0`，5 窗口时一眼看出哪端错了 | 想更直观时做 |

**如果要加文本显示**（可选），在 `IDFGasTestActor.h` 里加一个组件，并在血量变化时刷新它：

```cpp
// 头文件里
UPROPERTY(VisibleAnywhere, Category = "IDF|Debug")
TObjectPtr<UTextRenderComponent> StatusText;
```

```cpp
// 构造函数里
StatusText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusText"));
StatusText->SetupAttachment(RootComponent);
StatusText->SetHorizontalAlignment(EHTA_Center);
StatusText->SetWorldSize(60.f);
StatusText->SetTextRenderColor(FColor::Green);
```

```cpp
// 在 UIHealthSet 的 OnRep_Health 里或测试 Actor 的 Tick 里刷新：
StatusText->SetText(FText::FromString(
    FString::Printf(TEXT("HP %.0f / %.0f"), HealthSet->GetHealth(), HealthSet->GetMaxHealth())));
```

> ⚠️ **重要陷阱**：`UTextRenderComponent` 的 `Text` 属性**没有标记 `Replicated`**（我核实过 `TextRenderComponent.h` 的属性声明）。
> 也就是说：**服务器改了文字，客户端不会自动同步**。所以用它验证网络同步时，必须在**每个客户端本地**刷新文字（例如在 `OnRep_Health` 里调 `SetText`）。
> 反过来，这也说明了一件事：**别用渲染组件当复制状态的显示**——在客户端读到的属性值刷新，才是正确做法。

---

## 5. PIE 测试设置

### 5.1 单窗口先跑通

```
点击 ▶ 播放（Play）→ 选 "Selected Viewport" 或 "New Editor Window"
```

**第一步只用 1 个窗口**，确认能连上、能生成、日志对。

### 5.2 再跑 5 窗口（S05 的关键验证）

S09 才会正式配 5 窗口，但 S05 的**验证项 6（客户端属性复制）** 就已经需要多端了。临时设置：

```
▶ 播放按钮右侧 ▼ → Advanced Settings...（高级设置）
  Number of Players        = 5
  Net Mode                 = Play As Listen Server
  Run Under One Process    = 不勾选（5 个独立进程，更贴近真实联机）
                            ※ 如果出现端口冲突/连不上，改回勾选
  New Editor Window (PIE)  = 勾选
→ 关闭设置面板，点播放
```

（也可以把这段写进 `Config/DefaultEditorPerProjectUserSettings.ini`，见 `testing_pie.md` 第 1.1 节。**注意那是在 `Saved/Config/` 下的用户级文件**，不是 `Config/Default*.ini`。）

---

## 6. 预期日志与验证清单

在 **Output Log** 里筛选 `LogIDFAbility`、`LogIDFAttribute`、`LogIdentity5`。

### 6.1 你期望看到的三类输出

**① 插件与模块加载（每个进程各一次）**

```
LogIdentity5: Identity5 主模块已加载（编译时间 Oct  2 2026 22:xx:xx）
```

**② 测试 Actor 初始化（`BeginPlay` 里那行日志）**

```
LogIDFAbility: [GAS 测试] IDFGasTestActor_0 初始化完成（服务器=是），生命 100.0/100.0
```

**③ 属性集注册确认（`PrintDebugInfo` 的输出）**

```
LogIDFAbility: ========== IDF ASC 调试信息 ==========
LogIDFAbility:   属性集[0] = IDFHealthSet
LogIDFAbility:   能力[0] = ...（S06 之前应该是空的或只有占位）
LogIDFAbility:   当前标签   : State.Ready
LogIDFAbility:   生命       : 100.0 / 100.0
LogIDFAbility: ======================================
```

> 在 S05 阶段，`PrintDebugInfo()` 还**没有**被自动调用。你需要在 `AIDFGasTestActor::BeginPlay()` 末尾手动加一行：
> ```cpp
> if (AbilitySystemComponent)
> {
>     // 延迟一帧更保险：确保属性集注册与 ActorInfo 都就绪
>     AbilitySystemComponent->PrintDebugInfo();
> }
> ```
> （将来 S04 的 `IDF.Debug.GAS 1` 命令会接管这件事。）

### 6.2 逐项打勾

| # | 验证项 | 怎么看 | 期望 |
|---|---|---|---|
| 1 | 关卡能打开、不黑 | 视口 | 有地面、有天空、有光照 |
| 2 | 测试 Actor 已生成 | Outliner / 日志 | 出现 `IDFGasTestActor_0`，日志里的 `服务器=是` |
| 3 | 属性集注册成功 | 日志 | `属性集[0] = IDFHealthSet`（**不是**"未挂载"） |
| 4 | 初始血量正确 | 日志 | `100.0 / 100.0` |
| 5 | 标签查询不崩 | 日志 | `当前标签` 有输出（可能是空串） |
| 6 | **客户端也能读到属性** | **切到客户端窗口看日志** | 客户端窗口同样打印 `生命 100.0/100.0`，且 `服务器=否` |
| 7 | 夹取生效 | 临时加 `HealthSet->SetHealth(150.f)` | 打出来的生命是 **100.0**（被 `PreAttributeChange` 夹回 MaxHealth），不是 150 |
| 8 | Meta 属性不复制 | 代码审查 | `GetLifetimeReplicatedProps` 里**没有** `Damage` / `Healing` |

### 6.3 只有验证项 6 失败时的排查顺序

| 症状 | 根因 | 修复 |
|---|---|---|
| 客户端 `0.0/0.0`，服务器 `100.0/100.0` | `bReplicates` 为 false（`AActor` 默认 false） | 构造函数里 `bReplicates = true;` |
| 已开复制仍为 0 | `GetLifetimeReplicatedProps` 里漏了复制宏 | 用 `DOREPLIFETIME_CONDITION_NOTIFY(UIDFHealthSet, Health, COND_None, REPNOTIFY_Always)` |
| 复制宏也写了 | `OnRep_Health` 没加 `UFUNCTION()` | **加 `UFUNCTION()`——漏了不报错，只是永远不被调用**（最隐蔽的一条） |
| 都有 | `AddAttributeSetSubobject` 调用在 `InitAbilityActorInfo` **之前** | 调整 `BeginPlay` 顺序 |
| 值对但文字不更新 | 用了 `TextRenderComponent` 但没在客户端本地刷新 | 在 `OnRep_Health` 里 `SetText`（该组件不复制文字） |
| **只在单窗口正常** | 你看的是服务器窗口的日志 | 5 个窗口分别开 Output Log，或用 `stat net` 确认当前窗口是 `Client` 还是 `ListenServer` |

---

## 7. 完成本节后的状态

| 项 | 状态 |
|---|---|
| `Content/IDF/Maps/L_MilitaryFactory` | ✅ 存在，含地面 + 光照 + PlayerStart |
| `Config/DefaultEngine.ini` 的地图配置 | ✅ 自动生效（指向这张图） |
| S05 自测 8 项 | 全部通过（重点第 6 项） |
| 可提交的内容 | `Content/IDF/Maps/L_MilitaryFactory.umap`（**走 LFS**） |

### 提交

```powershell
cd 'G:\Project\Unreal Projects\Identity5'
git add Content/IDF Plugins/GameFeatures/IDFCore Source/Identity5 Config/DefaultEngine.ini
git status --short          # 提交前扫一眼，确认没有 Binaries/Intermediate 混进来
git commit -m "feat(level): 新建 L_MilitaryFactory 测试关卡（地面/光照/PlayerStart）并完成 S05 GAS 骨架验证"
```

> ⚠️ **`.umap` 会走 Git LFS**（`.gitattributes` 里配过）。第一次提交大关卡时 Git 会调用 LFS，确认：
> ```powershell
> git lfs ls-files
> ```
> 输出里应该出现 `L_MilitaryFactory.umap`。

---

## 8. 常见坑

| 现象 | 原因 | 处理 |
|---|---|---|
| 编辑器启动后是一片黑 | 用 `Empty Level` 建的，没有光照/天空 | File → New Level 选 **Basic** 重来，或在当前关卡补 `DirectionalLight` + `SkyAtmosphere` + `SkyLight` |
| 角色一生成就往下掉 | 地面没有碰撞，或 `PlayerStart` 在虚空里 | 确认地面碰撞为 `BlockAll`；`PlayerStart` 放在地面上方 |
| 关卡改了但重开没变 | 只按了 Ctrl+S 保存当前关卡，或存到了别处 | `File → Save All`；用 `File → Recent Levels` 确认路径是 `/Game/IDF/Maps/L_MilitaryFactory` |
| Content Browser 搜不到 `IDFGasTestActor` | 类没编译成功 / 没重启编辑器 / 过滤条件不对 | 依次检查；并把过滤器切到 "C++ Classes" |
| PIE 报 `Failed to load map` | 配置指向的路径与关卡实际路径不一致（大小写、文件夹名） | 对照 `Project Settings → Maps & Modes` 与 Content Browser 里的真实路径 |
| 5 窗口只有一个能连上 | 多进程 PIE 端口冲突 | Advanced Settings 里勾选 `Run Under One Process`；详见 `testing_pie.md` 第 4 章 |
| 日志太多找不到自己那几行 | 5 个进程日志混在一起 | Output Log 的过滤器里只勾 `LogIDFAbility` / `LogIDFAttribute`；或看 `Saved/Logs/` 下各进程独立日志 |
