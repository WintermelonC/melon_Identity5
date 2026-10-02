#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Engine/EngineBaseTypes.h"

/**
 * 屏幕调试输出的统一入口。
 *
 * 解决三个问题：
 *  1. GEngine->AddOnScreenDebugMessage 参数多、Key 需要手工管理，散落各处容易重复/覆盖；
 *  2. 高频调用（例如每 0.1s 一次的交互扫描）会刷爆屏幕，需要节流；
 *  3. 需要统一的调试总开关，避免发布版本残留调试输出。
 *
 * 注意：本工具依赖 GEngine 与 HUD 绘制，仅在「有渲染的客户端/服务器」有效。
 *       Dedicated Server（-server）上 GEngine 存在但没有视口，屏幕消息不会显示（也不报错）。
 */
struct IDENTITY5_API FIDFDebug
{
	/**
	 * 显示一条屏幕调试消息。
	 * @param Key              消息槽位（同一 Key 会【替换】旧消息，而不是叠加）。
	 *                         建议每个调试显示项使用稳定的常量 Key，不要用随机数或字符串哈希。
	 * @param Message          显示文本
	 * @param Duration         持续秒数。>0 表示显示这么久；=0 表示只显示这一帧（需每帧调用）；
	 *                         <0 表示显示 0 帧但仍会进入消息列表（几乎不用）
	 * @param Color            颜色
	 * @param ThrottleSeconds  节流间隔（秒）：距上次【相同 Key】的输出不足该秒数则跳过。0 = 不节流
	 */
	static void Screen(uint64 Key, const FString& Message, float Duration = 0.f,
					   const FColor& Color = FColor::White, float ThrottleSeconds = 0.f);
	
	/** 清空所有屏幕调试消息（等价于 GEngine->ClearOnScreenDebugMessages()） */
	static void ClearScreen();
	
	/** 移除某个 Key 的消息 */
	static void RemoveScreen(uint64 Key);
	
	/** 判断某个 Key 的消息当前是否在屏幕上 */
	static bool ScreenMessageExists(uint64 Key);

	/** 调试总开关：由 CVar "IDF.Debug.Enabled" 控制 */
	static bool IsEnabled();
	
	/** 是否绘制持续调试信息：由 CVar "IDF.Debug.Draw" 控制。
	 *  供 S11（交互点可视化）/ S13（破译进度）等"每帧绘制"的系统查询。
	 *  注意：一次性命令（如 IDF.Debug.Print）不应检查它——见 S04 文档的设计说明。 */
	static bool ShouldDrawDebugInfo();
	
	/** 把 ENetMode 转成可读字符串 */
	static const TCHAR* NetModeToString(ENetMode NetMode);
	
	/** 取当前网络模式的可读字符串（内部调用 World->GetNetMode()） */
	static FString GetNetModeString(const UWorld* World);
	
private:
	/** Key -> 上次输出时间（FPlatformTime::Seconds()），用于节流。仅游戏线程访问 */
	static TMap<uint64, double> LastPrintTimes;
};