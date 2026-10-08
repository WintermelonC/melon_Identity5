// Copyright Epic Games, Inc. All Rights Reserved.

#include "Identity5.h"
#include "IDFLog/Identity5Log.h"
#include "Modules/ModuleManager.h"

/**
 * 主游戏模块。
 * UE 的模块有明确的生命周期：StartupModule() 在模块加载时调用（引擎初始化早期），
 * ShutdownModule() 在退出时调用。理解这个时机很重要——例如它比 GameMode 的构造更早，
 * 比世界（World）创建更早，所以这里不能访问 GetWorld()。
 */
class FIdentity5Module : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
		
		UE_LOG(LogIdentity5, Log, TEXT("Identity5 主模块已加载（编译时间 %s %s）"), TEXT(__DATE__), TEXT(__TIME__));
	}
	
	virtual void ShutdownModule() override
	{
		UE_LOG(LogIdentity5, Log, TEXT("Identity5 主模块已卸载"));
		
		FDefaultGameModuleImpl::ShutdownModule();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE( FIdentity5Module, Identity5, "Identity5" );
