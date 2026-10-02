#include "Debug/Identity5Debug.h"
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

// ─────────────────────────────────────────────────────────────
// 2) 调试总开关（CVar）
//    TAutoConsoleVariable 是"带值的变量"：有默认值、可被 ini 覆盖、可在控制台改。
//    ECVF_Cheat：标记为作弊类，Shipping 构建默认不可用（发布版不会留调试后门）。
//    放在文件作用域 = 模块加载时自动注册，不需要手工注册代码。
// ─────────────────────────────────────────────────────────────
static TAutoConsoleVariable<int32> CVarIDFDebugEnabled(
	TEXT("IDF.Debug.Enabled"),
	1,
	TEXT("IDF 调试输出总开关：1 开启，0 关闭（关闭后所有屏幕调试输出静默）"),
	ECVF_Cheat);

bool FIDFDebug::IsEnabled()
{
	// 用 GetValueOnAnyThread() 还是 GetValueOnGameThread()？
	//  - 本项目所有调试输出都在游戏线程调用，用 GetValueOnGameThread() 语义更明确；
	//  - 它的代价是会断言"必须在游戏线程"，能帮你提前发现跨线程误用。
	return CVarIDFDebugEnabled.GetValueOnGameThread() != 0;
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
// 4) ScreenAndLog()：屏幕 + 日志双通道
//    ⚠️ 模板定义必须放在头文件才能被其他翻译单元使用；放在 .cpp 里只能本文件用。
//       所以这里【不用模板】，而是用 UE_LOG 的格式化入口 + FString 拼接。
// ─────────────────────────────────────────────────────────────
void FIDFDebug::ScreenAndLog(const FLogCategoryBase& Category, FColor Color, uint64 Key,
							 const FString& Message, float Duration, float ThrottleSeconds)
{
	// 屏幕部分
	Screen(Key, Message, Duration, Color, ThrottleSeconds);

	// 日志部分：UE_LOG 的第一个参数要求是"日志类别【变量名】"（宏内部用 #Category 取名字），
	// 因此无法把 FLogCategoryBase& 这种变量传进去，只能走底层入口 FMsg::Logf。
	//
	// 已核实签名（Runtime/Core/Public/Logging/LogMacros.h:62）：
	//   static void Logf(const ANSICHAR* File, int32 Line, const FLogCategoryName& Category,
	//                    ELogVerbosity::Type Verbosity, const FmtType& Fmt, Types... Args)
	// 两个要点：
	//   1) File 形参是 ANSICHAR*，所以 __FILE__（窄字符）直接传，【不要】套 TEXT()；
	//   2) Category 形参是 FLogCategoryName（UE5.8 中为 FLazyName 或 FName 的条件别名，
	//      见 LogCategory.h:13/16），而 GetCategoryName() 返回的正是 FLogCategoryName&，
	//      类型直接匹配，无需转换。
	FMsg::Logf(__FILE__, __LINE__, Category.GetCategoryName(),
			   ELogVerbosity::Log, TEXT("%s"), *Message);
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