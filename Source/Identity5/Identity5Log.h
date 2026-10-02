#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogIdentity5,     Log, All);   // 主模块：生命周期、模块装配
DECLARE_LOG_CATEGORY_EXTERN(LogIDFNet,        Log, All);   // 网络：复制、RPC、预测校正
DECLARE_LOG_CATEGORY_EXTERN(LogIDFAbility,    Log, All);   // GAS：能力激活/失败、标签变化
DECLARE_LOG_CATEGORY_EXTERN(LogIDFAttribute,  Log, All);   // GAS：属性变化与结算
DECLARE_LOG_CATEGORY_EXTERN(LogIDFInteraction,Log, All);   // 交互：扫描、请求、服务器校验
DECLARE_LOG_CATEGORY_EXTERN(LogIDFMatch,      Log, All);   // 对局：阶段、计数、胜负
DECLARE_LOG_CATEGORY_EXTERN(LogIDFAI,         Log, All);   // AI：感知、黑板、行为树
DECLARE_LOG_CATEGORY_EXTERN(LogIDFUI,         Log, All);   // UI：页面栈、ViewModel 绑定
