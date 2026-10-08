#pragma once

#include "NativeGameplayTags.h"

/**
 * 项目原生 GameplayTag 的集中声明处。
 *
 * 使用规则（务必遵守，否则编译失败或产生重复符号）：
 *   - 本文件用 UE_DECLARE_GAMEPLAY_TAG_EXTERN 声明；
 *   - 对应的 UE_DEFINE_GAMEPLAY_TAG 全部写在 IDFGameplayTags.cpp 里，且【只能有一处】；
 *   - 严禁把 UE_DEFINE_GAMEPLAY_TAG 写进头文件（宏内有 static_assert 拦截）。
 *
 * 命名约定：参考 design_analysis.md 第 2 轮 5.1 节。
 * 标签按"用到时才加"的节奏增长，不要一次全写完。
 */
namespace IDFTags
{
	// ═══════════════════════════════════════════════════
	// 输入（Input.*）—— ASC 按这些标签把输入转发给对应能力
	// ═══════════════════════════════════════════════════
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Interact);
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_InteractHold);
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Sprint);
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Crouch);
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Skill1);
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Skill2);
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_UseItem);
	
	// ═══════════════════════════════════════════════════
	// 能力标识（Ability.*）
	// ═══════════════════════════════════════════════════
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Test);
	
	// ═══════════════════════════════════════════════════
	// 角色状态（State.*）
	// ═══════════════════════════════════════════════════
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Ready);

	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Survivor_Healthy);
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Survivor_Injured);
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Survivor_Downed);
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Survivor_Carried);
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Survivor_OnChair_Phase1);
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Survivor_OnChair_Phase2);
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Survivor_Escaped);
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Survivor_Eliminated);

	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hunter_Chasing);
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hunter_Attacking);
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hunter_WipingBlade);
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hunter_Stunned);
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Hunter_Carrying);

	// ═══════════════════════════════════════════════════
	// 属性集用的 SetByCaller 数据标签（Data.*）
	//    S05 留下的 TODO：PostGameplayEffectExecute 里读取伤害/治疗量的正确来源。
	//    GE 里用 SetByCaller(Data.Damage) 传值，代码用这个标签取。
	// ═══════════════════════════════════════════════════
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Healing);

	// ═══════════════════════════════════════════════════
	// 冷却阻断（Cooldown.*）
	// ═══════════════════════════════════════════════════
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Test);
	
	// 初始化状态标签（对应 UGameFrameworkComponentManager 的状态链）
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_Spawned);
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_DataAvailable);
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_DataInitialized);
	IDFCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InitState_GameplayReady);
}
