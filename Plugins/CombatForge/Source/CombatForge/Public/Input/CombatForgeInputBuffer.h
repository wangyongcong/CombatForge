// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/CombatForgeInputTypes.h"

class COMBATFORGE_API FCombatForgetInputBuffer
{
public:
	void Configure(const FCombatForgeInputRuntimeSettings& InSettings, const TArray<FCombatForgeCommand>& InCommands);
	bool Tick(uint16 StateBits, TArray<const FCombatForgeCommand*>& OutCommands);
	void GetBufferedStates(TArray<uint16>& OutStates) const;
	const TArray<FString>& GetDebugRejections() const { return DebugRejections; }
	const TArray<FCombatForgeCommand>& GetCompiledCommandsForTesting() const { return Commands; }

private:
	void ResetRuntimeState();
	void SolveCommands(TArray<const FCombatForgeCommand*>& OutCommands) const;
	bool TryMatchCommand(const FCombatForgeCommand& Command) const;
	static bool IsCommandHigherPriority(const FCombatForgeCommand& A, const FCombatForgeCommand& B);
	uint16 GetStateAtLogicalIndex(int32 LogicalIndex) const;
	static int32 GetMinimumHeldTicks(const int32 HeldTicksByBit[16], uint16 Mask);
	static bool MatchesElementState(uint16 StateBits, uint16 RequiredMask, uint16 AcceptedMask);
	bool HasInterveningChanges(int32 EarlierIndex, int32 LaterIndex) const;

private:
	FCombatForgeInputRuntimeSettings Settings;
	TArray<FCombatForgeCommand> Commands;
	TArray<uint16> Storage;
	int32 Head = 0;
	int32 Count = 0;
	int32 CurrentTick = 0;
	uint16 PreviousStateBits = 0;
	int32 HeldTicksByBit[16] = {};
	TArray<FString> DebugRejections;
};
