#include "Identity5Debug.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "HAL/IConsoleManager.h"
#include "Logging/LogCategory.h"

// ─────────────────────────────────────────────────────────────
// 1) 静态成员定义
//    TMap 是"每个 Key 上次输出的时间"。因为它是 static，生命周期 = 整个进程，
//    所以 PIE 反复启停不会重置它 —— 这一点对节流无害（时间戳单调递增），
//    但要知道它不会自己清理。条目数 = 你用过的 Key 数量，可忽略不计。
// ─────────────────────────────────────────────────────────────
TMap<uint64, double> FIDFDebug::LastPrintTimes;

// 总开关：0 时所有屏幕调试输出静默
static TAutoConsoleVariable<int32> CVarIDFDebugEnabled(
	TEXT("IDF.Debug.Enabled"),
	1,
	TEXT("IDF 调试输出总开关：1 开启，0 关闭（关闭后所有屏幕调试输出静默）"),
	ECVF_Cheat);

// 持续可视化开关：只影响"每帧绘制"的调试叠加层，不影响一次性命令
static TAutoConsoleVariable<int32> CVarIDFDrawDebugInfo(
	TEXT("IDF.Debug.Draw"),
	1,
	TEXT("是否在屏幕上绘制 IDF 调试信息：1 开启，0 关闭"),
	ECVF_Cheat);

bool FIDFDebug::IsEnabled()
{
	// 用 GetValueOnAnyThread() 还是 GetValueOnGameThread()？
	//  - 本项目所有调试输出都在游戏线程调用，用 GetValueOnGameThread() 语义更明确；
	//  - 它的代价是会断言"必须在游戏线程"，能帮你提前发现跨线程误用。
	return CVarIDFDebugEnabled.GetValueOnGameThread() != 0;
}

bool FIDFDebug::ShouldDrawDebugInfo()
{
	return CVarIDFDrawDebugInfo.GetValueOnGameThread() != 0;
}

// ─────────────────────────────────────────────────────────────
// 3) 核心：Screen()
// ─────────────────────────────────────────────────────────────
void FIDFDebug::Screen(uint64 Key, const FString& Message, float Duration,
					   const FColor& Color, float ThrottleSeconds)
{
	// (a) 总开关：关闭时直接返回。注意"先判开关再判节流"，
	//     避免关闭状态下还在维护节流表。
	if (!IsEnabled())
	{
		return;
	}
	
	// (b) GEngine 可能为空：编辑器关闭游戏视口、命令行工具、命令行 cook 等上下文。
	//     这是最常见的崩溃点，必须最先判空。
	if (GEngine == nullptr)
	{
		return;
	}
	
	// (c) 节流：只有 ThrottleSeconds > 0 且"相同 Key 距上次输出不足间隔"时才跳过。
	//     时间基准用 FPlatformTime::Seconds()（单调递增的高精度秒），
	//     不要用 FApp::GetDeltaTime() 累加 —— 会随帧率漂移，也不该用于"时间点"比较。
	if (ThrottleSeconds > 0)
	{
		const double Now = FPlatformTime::Seconds();
		if (double* LastTime = LastPrintTimes.Find(Key))
		{
			if (Now - *LastTime < static_cast<double>(ThrottleSeconds))
			{
				return;   // 未到下一次允许输出的时间，静默跳过
			}
		}
		LastPrintTimes.Add(Key, Now);   // 首次或已到期：记录本次输出时间
	}
	
	// (d) 真正输出。
	//     参数语义（已核实 Engine/Classes/Engine/Engine.h:2197）：
	//       Key            —— 同一 Key 的消息会【替换】，不是叠加；
	//       TimeToDisplay  —— 显示时长（秒）；0 = 只显示本帧；
	//       bNewerOnTop    —— 【仅在 Key == INDEX_NONE 时生效】（引擎注释明确写了这一点）。
	//                         我们始终传固定 Key，所以这个参数对我们没有实际作用，传默认值即可。
	GEngine->AddOnScreenDebugMessage(Key, Duration, Color, Message);
}

// ─────────────────────────────────────────────────────────────
// 5) 其余小工具
// ─────────────────────────────────────────────────────────────
void FIDFDebug::ClearScreen()
{
	if (GEngine)
	{
		GEngine->ClearOnScreenDebugMessages();
	}
	LastPrintTimes.Reset();
}

void FIDFDebug::RemoveScreen(uint64 Key)
{
	if (GEngine)
	{
		GEngine->RemoveOnScreenDebugMessage(Key);
	}
	LastPrintTimes.Remove(Key);
}

bool FIDFDebug::ScreenMessageExists(uint64 Key)
{
	if (GEngine)
	{
		return GEngine->OnScreenDebugMessageExists(Key);
	}
	return false;
}

// ─────────────────────────────────────────────────────────────
// 6) ENetMode → 字符串
//    ⚠️ ENetMode 是裸枚举（EngineBaseTypes.h:978: `enum ENetMode : int`），
//       没有 UENUM 标记，因此 UEnum::GetValueAsString() 用不了 —— 必须手写 switch。
//       这是"看起来该有现成 API、实际没有"的典型情况，写错就是编译错误。
// ─────────────────────────────────────────────────────────────
const TCHAR* FIDFDebug::NetModeToString(ENetMode NetMode)
{
	switch (NetMode)
	{
	case NM_Standalone:       return TEXT("Standalone");
	case NM_DedicatedServer:  return TEXT("DedicatedServer");
	case NM_ListenServer:     return TEXT("ListenServer");
	case NM_Client:           return TEXT("Client");
	default:                  return TEXT("Unknown");
	}
}

FString FIDFDebug::GetNetModeString(const UWorld* World)
{
	if (World == nullptr)
	{
		return TEXT("World=nullptr");
	}
	return NetModeToString(World->GetNetMode());
}