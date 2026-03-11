// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/CombatForgeInputTypes.h"

class COMBATFORGE_API FCombatForgeCommandCompiler
{
public:
	static constexpr int32 CurrentCompilerVersion = 1;

	static bool IsCommandCurrent(
		const FCombatForgeInputRuntimeSettings& Settings,
		const FCombatForgeCommand& Command);

	static bool CompileCommands(
		const FCombatForgeInputRuntimeSettings& Settings,
		TArray<FCombatForgeCommand>& InOutCommands,
		TArray<FCombatForgeCompileMessage>& OutMessages);

	static uint32 ComputeSourceHash(
		const FCombatForgeInputRuntimeSettings& Settings,
		const TArray<FCombatForgeCommand>& Commands);
};
