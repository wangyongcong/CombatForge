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
	struct FTokenRuntimeState
	{
		int16 Age = 0;
		int16 PreviousAge = 0;
	};

	struct FCommandRuntimeState
	{
		int32 NextStepIndex = 0;
		int32 SequenceStartTick = INDEX_NONE;
		int32 LastMatchedTick = INDEX_NONE;
		bool bCompletedThisTick = false;
		bool bLatchedComplete = false;
	};

	void ResetRuntimeState();
	void SolveCommands(bool bStateChanged, TArray<const FCombatForgeCommand*>& OutCommands);
	bool TryAdvanceCommand(const FCombatForgeCommand& Command, FCommandRuntimeState& RuntimeState, bool bStateChanged);
	void ResetCommandRuntimeState(FCommandRuntimeState& RuntimeState) const;
	static bool IsCommandHigherPriority(const FCombatForgeCommand& A, const FCombatForgeCommand& B);
	uint16 GetStateAtLogicalIndex(int32 LogicalIndex) const;
	int32 GetMinimumHeldTicks(int32 Mask) const;
	int32 GetMinimumPreviousHeldTicks(int32 Mask) const;
	static bool MatchesElementState(uint16 StateBits, int32 RequiredMask, int32 AcceptedMask);
	bool MatchesHeldElement(const FCombatForgeCommandElement& Element) const;
	bool MatchesPressElement(const FCombatForgeCommandElement& Element, bool bStateChanged) const;
	bool MatchesReleaseElement(const FCombatForgeCommandElement& Element, bool bStateChanged) const;
	bool MatchesElementOnCurrentTick(const FCombatForgeCommandElement& Element, bool bStateChanged) const;

private:
	FCombatForgeInputRuntimeSettings Settings;
	TArray<FCombatForgeCommand> Commands;
	TArray<FCommandRuntimeState> CommandRuntimeStates;
	TArray<uint16> Storage;
	int32 Head = 0;
	int32 Count = 0;
	int32 CurrentTick = 0;
	uint16 PreviousStateBits = 0;
	FTokenRuntimeState TokenStates[16] = {};
	bool bHasReceivedTick = false;
	TArray<FString> DebugRejections;
};
