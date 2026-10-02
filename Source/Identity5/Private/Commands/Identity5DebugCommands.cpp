// IDF 调试控制台入口。
//
// 设计要点：
//  1) 所有 FAutoConsoleCommand* / TAutoConsoleVariable / FAutoConsoleVariableRef 都声明为
//     文件作用域【静态对象】—— 它们在模块加载时构造、卸载时析构，
//     构造函数内部就是"注册命令"的动作，所以不需要任何手工注册代码。
//  2) 命令名统一 IDF. 前缀，控制台里输入 "IDF." 会列出全部。
//  3) 关于 Flags：ECVF_Cheat 的语义是"在最终发布版中隐藏、用户无法修改"，
//     开发期（编辑器/PIE/Development）一切正常。所以这里统一用 ECVF_Cheat，
//     将来真要发布时无需改代码。若你在 PIE 里发现命令找不到，把它改成 ECVF_Default
//     再试一次即可（这一步能帮你确认问题是否出在 Flags 上）。

#include "CoreMinimal.h"
#include "Logging/Identity5Log.h"
#include "Debug/Identity5Debug.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"

// ═══════════════════════════════════════════════════════════════
// 控制台变量（CVar）：有值、表示状态
// ═══════════════════════════════════════════════════════════════

// ① 用 FAutoConsoleVariableRef 把"一个已有变量"暴露成 CVar。
//    适合：已经有一个 int32/bool/float 成员，想让它可被控制台修改。
static int32 GIDFDrawDebugInfo = 0;
static FAutoConsoleVariableRef CVarIDFDrawDebugInfo(
	TEXT("IDF.Debug.Draw"),
	GIDFDrawDebugInfo,
	TEXT("是否在屏幕上绘制 IDF 调试信息：0 关闭，1 开启"),
	ECVF_Cheat);

// 供其它系统查询（后续 S11 可视化交互点、S13 显示破译进度都会用到）。
// 放在头文件里声明、这里定义，避免各处直接访问 GIDFDrawDebugInfo。
bool IDFDebug_ShouldDrawDebugInfo()
{
	return GIDFDrawDebugInfo != 0;
}

// ② 用 TAutoConsoleVariable 创建一个"自带存储"的 CVar。
//    适合：值只被内部使用，不需要暴露成普通变量。
//    读取方式与 ① 不同：要调用 GetValueOnGameThread()。
static TAutoConsoleVariable<int32> CVarIDFDebugVerboseLog(
	TEXT("IDF.Debug.VerboseLog"),
	0,
	TEXT("是否把 IDF 调试信息同时写入日志：0 关闭，1 开启"),
	ECVF_Cheat);

static bool IDFDebug_ShouldVerboseLog()
{
	return CVarIDFDebugVerboseLog.GetValueOnGameThread() != 0;
}

// ═══════════════════════════════════════════════════════════════
// 控制台命令：一个动作
// ═══════════════════════════════════════════════════════════════

// ③ 最小验证命令：带参数
static FAutoConsoleCommandWithWorldAndArgs CmdIDFDebugPrint(
	TEXT("IDF.Debug.Print"),
	TEXT("验证调试基础设施：在屏幕与日志中输出一条消息。用法：IDF.Debug.Print [文本...]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			// ⚠️ 不要用 FString::Join —— 该 API 在 UE5.8 中【不存在】
			//    （FString 所在的 Containers/UnrealString.h 里没有 Join，只有 FText::Join，
			//      且 FText::Join 的参数顺序是 (Delimiter, Args)，很容易记错）。
			//    这里用手写拼接，最直观也最稳。
			FString Text;
			for (int32 Index = 0; Index < Args.Num(); ++Index)
			{
				if (Index > 0)
				{
					Text.AppendChar(TEXT(' '));
				}
				Text.Append(Args[Index]);
			}
			if (Text.IsEmpty())
			{
				Text = TEXT("Hello, Identity5");
			}
			
			UE_LOG(LogIdentity5, Log, TEXT("IDE.Debug.Print 被调用: %s"), *Text);
			FIDFDebug::Screen(1, Text, 5.f, FColor::Cyan);
		}));
		
// ④ "转储世界状态"命令：需要遍历世界（S08 做阵营分配时的主力验证工具）
static FAutoConsoleCommandWithWorldAndArgs CmdIDFDumpWorld(
	TEXT("IDF.Debug.DumpWorld"),
	TEXT("打印当前世界的调试摘要：地图、网络模式、玩家、Pawn。"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if (World == nullptr)
			{
				UE_LOG(LogIdentity5, Warning, TEXT("IDF.Debug.DumpWorld：没有有效的 World"));
				return;
			}
			
			UE_LOG(LogIdentity5, Log, TEXT("========== IDF 世界摘要 =========="));
			UE_LOG(LogIdentity5, Log, TEXT("地图名      : %s"), *World->GetMapName());
			UE_LOG(LogIdentity5, Log, TEXT("网络模式    : %s"), *FIDFDebug::GetNetModeString(World));
			UE_LOG(LogIdentity5, Log, TEXT("玩家控制器数: %d"), World->GetNumPlayerControllers());

			// 遍历玩家。
			// 已核实（Engine/Classes/Engine/World.h:93 与 :2764）：
			//   FConstPlayerControllerIterator = TArray<TWeakObjectPtr<APlayerController>>::TConstIterator
			// 因此解引用得到的是【弱指针】，必须 Get() 并判空。
			int32 PlayerIndex = 0;
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				APlayerController* PC = It->Get();
				if (PC == nullptr)
				{
					UE_LOG(LogIdentity5, Warning, TEXT("  [%d] PlayerController 已失效（弱指针为空）"), PlayerIndex);
					++PlayerIndex;
					continue;
				}
				
				const APawn* Pawn = PC->GetPawn();
				const APlayerState* PS = PC->PlayerState;
				
				UE_LOG(LogIdentity5, Log, TEXT("  [%d] 控制器类=%s  本地控制=%s  PlayerState=%s  Pawn=%s"),
					PlayerIndex,
					*GetNameSafe(PC->GetClass()),
					PC->IsLocalController() ? TEXT("是") : TEXT("否"),
					PS ? *GetNameSafe(PS->GetClass()) : TEXT("<无>"),
					Pawn ? *GetNameSafe(Pawn->GetClass()) : TEXT("<无 Pawn>"));
				
				// Pawn 的位置对排查"生成在哪里"很有用
				if (Pawn != nullptr)
				{
					const FVector Loc = Pawn->GetActorLocation();
					
					// ⚠️ 注意：APlayerState::GetPlayerName() 是【按值返回 FString】。
					//    若写成 *PS->GetPlayerName()，虽然在同一表达式内临时对象尚未析构、
					//    实际能跑通，但非常脆弱（且容易被误改成跨语句使用）。这里先取到局部变量。
					const FString PlayerName = PS ? PS->GetPlayerName() : FString(TEXT("<未知>"));
					
					UE_LOG(LogIdentity5, Log, TEXT("       位置=(%.0f, %.0f, %.0f)  所属玩家=%s"),
						Loc.X, Loc.Y, Loc.Z, *PlayerName);
				}
				
				++PlayerIndex;
			}
			
			UE_LOG(LogIdentity5, Log, TEXT("=================================="));
		}));

// ⑤ 清屏命令：只做一件事，无参数
static FAutoConsoleCommand CmdIDFDebugClear(
	TEXT("IDF.Debug.Clear"),
	TEXT("清空所有 IDF 屏幕调试消息。"),
	FConsoleCommandDelegate::CreateLambda(
		[]()
		{
			FIDFDebug::ClearScreen();
			UE_LOG(LogIdentity5, Log, TEXT("IDF 屏幕调试消息已清空"));
		}));

// ⑥ 打印调试开关当前状态（验证 CVar 是否真的生效，比在控制台里逐个查询省事）
static FAutoConsoleCommand CmdIDFDebugStatus(
	TEXT("IDF.Debug.Status"),
	TEXT("打印 IDF 调试开关的当前状态。"),
	FConsoleCommandDelegate::CreateLambda(
		[]()
		{
			UE_LOG(LogIdentity5, Log, TEXT("=== IDF 调试开关 ==="));
			UE_LOG(LogIdentity5, Log, TEXT("  IDF.Debug.Enabled    = %d"), FIDFDebug::IsEnabled() ? 1 : 0);
			UE_LOG(LogIdentity5, Log, TEXT("  IDF.Debug.Draw       = %d"), GIDFDrawDebugInfo);
			UE_LOG(LogIdentity5, Log, TEXT("  IDF.Debug.VerboseLog = %d"), IDFDebug_ShouldVerboseLog() ? 1 : 0);
			UE_LOG(LogIdentity5, Log, TEXT("  编译时间             = %s %s"), TEXT(__DATE__), TEXT(__TIME__));
		}));