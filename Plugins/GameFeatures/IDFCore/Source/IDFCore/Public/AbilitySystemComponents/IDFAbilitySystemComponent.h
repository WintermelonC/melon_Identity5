#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "IDFAbilitySystemComponent.generated.h"

/**
 * 项目自定义 ASC。
 *
 * S05 阶段它是一个薄壳，只负责：
 *   1) 显式打开复制
 *   2) 提供统一的调试打印入口
 * 后续会在它上面实现：
 *   S06 —— 能力输入转发（AbilityInputTagPressed/Released）、UIDFAbilitySet 装配
 *   S07 —— NotifyAbilityFailed 统一失败反馈
 *   S11 —— 属性变化订阅的统一出口
 */
UCLASS()
class IDFCORE_API UIDFAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
public:
	UIDFAbilitySystemComponent();
	
	/** 打印当前 ASC 的能力、标签、关键属性（由 IDF.Debug.GAS 1 触发）
	 *  刻意【不加 const】：调试函数没必要 const，且能避免碰到 GAS 内部
	 *  某些 const 重载不完整的坑（宁可少一个 const 也不要卡在编译错误上）。 */
	void PrintDebugInfo();
};