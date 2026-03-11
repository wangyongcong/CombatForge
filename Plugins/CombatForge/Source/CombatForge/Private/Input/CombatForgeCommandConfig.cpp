// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/CombatForgeCommandConfig.h"

#include "CombatForge.h"
#include "Input/CombatForgeCommandCompiler.h"

#if WITH_EDITOR
#include "UObject/Package.h"
#endif

UCombatForgeCommandConfig::UCombatForgeCommandConfig()
{
	CompileVersion = 0;
	CompiledSourceHash = 0;
	bCompileSucceeded = false;
}

bool UCombatForgeCommandConfig::CompileOffline()
{
	Modify();

	TArray<FCombatForgeCompileMessage> CompileMessages;
	bCompileSucceeded = FCombatForgeCommandCompiler::CompileCommands(RuntimeSettings, Commands, CompileMessages);
	CompileVersion = FCombatForgeCommandCompiler::CurrentCompilerVersion;
	CompiledSourceHash = FCombatForgeCommandCompiler::ComputeSourceHash(RuntimeSettings, Commands);
	CompileErrorMessage.Reset();

	if (!bCompileSucceeded)
	{
		TArray<FString> ErrorLines;
		ErrorLines.Reserve(CompileMessages.Num());
		for (const FCombatForgeCompileMessage& CompileMessage : CompileMessages)
		{
			ErrorLines.Add(CompileMessage.Message);
		}
		CompileErrorMessage = FString::Join(ErrorLines, TEXT("\n"));
		UE_LOG(LogCombatForge, Warning, TEXT("CombatForge command config '%s' failed offline compilation: %s"), *GetPathName(), *CompileErrorMessage);
	}

#if WITH_EDITOR
	MarkPackageDirty();
#endif

	return bCompileSucceeded;
}

bool UCombatForgeCommandConfig::IsCompileCurrent() const
{
	if (!bCompileSucceeded)
	{
		return false;
	}

	if (CompileVersion != FCombatForgeCommandCompiler::CurrentCompilerVersion)
	{
		return false;
	}

	return CompiledSourceHash == FCombatForgeCommandCompiler::ComputeSourceHash(RuntimeSettings, Commands);
}

#if WITH_EDITOR
void UCombatForgeCommandConfig::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	static const FName RuntimeSettingsName = GET_MEMBER_NAME_CHECKED(UCombatForgeCommandConfig, RuntimeSettings);
	static const FName CommandsName = GET_MEMBER_NAME_CHECKED(UCombatForgeCommandConfig, Commands);

	const FName PropertyName = PropertyChangedEvent.GetMemberPropertyName();
	if (PropertyName == RuntimeSettingsName || PropertyName == CommandsName)
	{
		bCompileSucceeded = false;
		CompileErrorMessage.Reset();
	}
}
#endif
