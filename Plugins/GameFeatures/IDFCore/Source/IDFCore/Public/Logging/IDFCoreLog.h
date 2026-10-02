#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogIDFNet,         Log, All);   // 网络：复制、RPC、预测校正
DECLARE_LOG_CATEGORY_EXTERN(LogIDFAbility,     Log, All);   // GAS：能力激活/失败、标签变化
DECLARE_LOG_CATEGORY_EXTERN(LogIDFAttribute,   Log, All);   // GAS：属性变化与结算
DECLARE_LOG_CATEGORY_EXTERN(LogIDFInteraction, Log, All);   // 交互：扫描、请求、服务器校验