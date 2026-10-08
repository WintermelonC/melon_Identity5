#include "GameplayTags/IDFGameplayTags.h"

namespace IDFTags
{
	// ── 输入 ──
	UE_DEFINE_GAMEPLAY_TAG(Input_Interact,     "Input.Interact");
	UE_DEFINE_GAMEPLAY_TAG(Input_InteractHold, "Input.InteractHold");
	UE_DEFINE_GAMEPLAY_TAG(Input_Sprint,       "Input.Sprint");
	UE_DEFINE_GAMEPLAY_TAG(Input_Crouch,       "Input.Crouch");
	UE_DEFINE_GAMEPLAY_TAG(Input_Skill1,       "Input.Skill1");
	UE_DEFINE_GAMEPLAY_TAG(Input_Skill2,       "Input.Skill2");
	UE_DEFINE_GAMEPLAY_TAG(Input_UseItem,      "Input.UseItem");

	// ── 能力标识 ──
	UE_DEFINE_GAMEPLAY_TAG(Ability_Test,       "Ability.Test");

	// ── 角色状态 ──
	UE_DEFINE_GAMEPLAY_TAG(State_Ready,                 "State.Ready");

	UE_DEFINE_GAMEPLAY_TAG(State_Survivor_Healthy,      "State.Survivor.Healthy");
	UE_DEFINE_GAMEPLAY_TAG(State_Survivor_Injured,      "State.Survivor.Injured");
	UE_DEFINE_GAMEPLAY_TAG(State_Survivor_Downed,       "State.Survivor.Downed");
	UE_DEFINE_GAMEPLAY_TAG(State_Survivor_Carried,      "State.Survivor.Carried");
	UE_DEFINE_GAMEPLAY_TAG(State_Survivor_OnChair_Phase1, "State.Survivor.OnChair.Phase1");
	UE_DEFINE_GAMEPLAY_TAG(State_Survivor_OnChair_Phase2, "State.Survivor.OnChair.Phase2");
	UE_DEFINE_GAMEPLAY_TAG(State_Survivor_Escaped,      "State.Survivor.Escaped");
	UE_DEFINE_GAMEPLAY_TAG(State_Survivor_Eliminated,   "State.Survivor.Eliminated");

	UE_DEFINE_GAMEPLAY_TAG(State_Hunter_Chasing,     "State.Hunter.Chasing");
	UE_DEFINE_GAMEPLAY_TAG(State_Hunter_Attacking,   "State.Hunter.Attacking");
	UE_DEFINE_GAMEPLAY_TAG(State_Hunter_WipingBlade, "State.Hunter.WipingBlade");
	UE_DEFINE_GAMEPLAY_TAG(State_Hunter_Stunned,     "State.Hunter.Stunned");
	UE_DEFINE_GAMEPLAY_TAG(State_Hunter_Carrying,    "State.Hunter.Carrying");

	// ── SetByCaller 数据标签 ──
	UE_DEFINE_GAMEPLAY_TAG(Data_Damage,        "Data.Damage");
	UE_DEFINE_GAMEPLAY_TAG(Data_Healing,       "Data.Healing");

	// ── 冷却 ──
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Test,      "Cooldown.Test");
	
	UE_DEFINE_GAMEPLAY_TAG(InitState_Spawned,          "InitState.Spawned");
	UE_DEFINE_GAMEPLAY_TAG(InitState_DataAvailable,    "InitState.DataAvailable");
	UE_DEFINE_GAMEPLAY_TAG(InitState_DataInitialized,  "InitState.DataInitialized");
	UE_DEFINE_GAMEPLAY_TAG(InitState_GameplayReady,    "InitState.GameplayReady");
}
