// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/CombatForgeInputBuffer.h"
#include "CombatForge.h"
#include "Input/CombatForgeCommandCompiler.h"
#include "Input/CombatForgeInputUtility.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	static constexpr uint16 InputBufferDirectionMask = CombatForgeInput::DirectionMask;
	static constexpr uint16 InputBufferVerticalMask =
		static_cast<uint16>(ECombatForgeInputToken::Up) |
		static_cast<uint16>(ECombatForgeInputToken::Down);
	static constexpr uint16 InputBufferHorizontalMask =
		static_cast<uint16>(ECombatForgeInputToken::Forward) |
		static_cast<uint16>(ECombatForgeInputToken::Back);

	static FString FormatStateBitsForLog(uint16 StateBits)
	{
		const uint16 NormalizedBits = CombatForgeInput::NormalizeStateBits(StateBits);
		TArray<FString> Parts;

		const uint16 DirectionBits = NormalizedBits & InputBufferDirectionMask;
		switch (DirectionBits)
		{
		case static_cast<uint16>(ECombatForgeInputToken::Up):
			Parts.Add(TEXT("U"));
			break;
		case static_cast<uint16>(ECombatForgeInputToken::Down):
			Parts.Add(TEXT("D"));
			break;
		case static_cast<uint16>(ECombatForgeInputToken::Forward):
			Parts.Add(TEXT("F"));
			break;
		case static_cast<uint16>(ECombatForgeInputToken::Back):
			Parts.Add(TEXT("B"));
			break;
		case static_cast<uint16>(ECombatForgeInputToken::Up) | static_cast<uint16>(ECombatForgeInputToken::Forward):
			Parts.Add(TEXT("UF"));
			break;
		case static_cast<uint16>(ECombatForgeInputToken::Up) | static_cast<uint16>(ECombatForgeInputToken::Back):
			Parts.Add(TEXT("UB"));
			break;
		case static_cast<uint16>(ECombatForgeInputToken::Down) | static_cast<uint16>(ECombatForgeInputToken::Forward):
			Parts.Add(TEXT("DF"));
			break;
		case static_cast<uint16>(ECombatForgeInputToken::Down) | static_cast<uint16>(ECombatForgeInputToken::Back):
			Parts.Add(TEXT("DB"));
			break;
		default:
			Parts.Add(TEXT("N"));
			break;
		}

		struct FButtonLabel
		{
			ECombatForgeInputToken Token;
			const TCHAR* Label;
		};

		static const FButtonLabel Buttons[] =
		{
			{ ECombatForgeInputToken::X, TEXT("X") },
			{ ECombatForgeInputToken::Y, TEXT("Y") },
			{ ECombatForgeInputToken::Z, TEXT("Z") },
			{ ECombatForgeInputToken::A, TEXT("A") },
			{ ECombatForgeInputToken::B, TEXT("B") },
			{ ECombatForgeInputToken::C, TEXT("C") }
		};

		for (const FButtonLabel& Button : Buttons)
		{
			if ((NormalizedBits & static_cast<uint16>(Button.Token)) != 0)
			{
				Parts.Add(Button.Label);
			}
		}

		return FString::Join(Parts, TEXT("+"));
	}

	static FString DescribeElement(const FCombatForgeCommandElement& Element)
	{
		const TCHAR* MatchKind = TEXT("Press");
		switch (Element.MatchKind)
		{
		case ECombatForgeCommandElementMatchKind::Held:
			MatchKind = TEXT("Held");
			break;
		case ECombatForgeCommandElementMatchKind::Release:
			MatchKind = TEXT("Release");
			break;
		case ECombatForgeCommandElementMatchKind::Press:
		default:
			break;
		}

		return FString::Printf(
			TEXT("%s req=%s accepted=0x%04x hold=%d strict=%s"),
			MatchKind,
			*FormatStateBitsForLog(Element.RequiredMask),
			Element.AcceptedMask,
			Element.MinHeldFrames,
			Element.bStrictAfterPrevious ? TEXT("true") : TEXT("false"));
	}

	static bool MatchesHeldElementState(uint16 StateBits, int32 RequiredMask, int32 AcceptedMask)
	{
		checkSlow(StateBits == CombatForgeInput::NormalizeStateBits(StateBits));
		if ((static_cast<int32>(StateBits) & RequiredMask) != RequiredMask)
		{
			return false;
		}

		const uint16 RequiredDirections = static_cast<uint16>(RequiredMask & InputBufferDirectionMask);
		if (RequiredDirections == 0)
		{
			return true;
		}

		uint16 AllowedExtraDirections = static_cast<uint16>(AcceptedMask & InputBufferDirectionMask);
		if (AllowedExtraDirections == 0)
		{
			const bool bHasVertical = (RequiredDirections & InputBufferVerticalMask) != 0;
			const bool bHasHorizontal = (RequiredDirections & InputBufferHorizontalMask) != 0;
			if (bHasVertical && !bHasHorizontal)
			{
				AllowedExtraDirections = InputBufferHorizontalMask;
			}
			else if (bHasHorizontal && !bHasVertical)
			{
				AllowedExtraDirections = InputBufferVerticalMask;
			}
		}

		const uint16 StateDirections = StateBits & InputBufferDirectionMask;
		const uint16 ExtraDirections = static_cast<uint16>(StateDirections & ~RequiredDirections);
		return (ExtraDirections & static_cast<uint16>(~AllowedExtraDirections)) == 0;
	}

	static bool IsDirectionalElement(const FCombatForgeCommandElement& Element)
	{
		return (Element.RequiredMask & InputBufferDirectionMask) != 0;
	}
}

void FCombatForgetInputBuffer::Configure(const FCombatForgeInputRuntimeSettings& InSettings, const TArray<FCombatForgeCommand>& InCommands)
{
	Settings = InSettings;
	Commands = InCommands;
	DebugRejections.Reset();

	TArray<FCombatForgeCompileMessage> CompileMessages;
	FCombatForgeCommandCompiler::CompileCommands(Settings, Commands, CompileMessages);
	for (const FCombatForgeCompileMessage& Message : CompileMessages)
	{
		if (Message.bIsError)
		{
			DebugRejections.Add(Message.Message);
		}
	}

	ResetRuntimeState();
	CommandRuntimeStates.Reset();
	CommandRuntimeStates.SetNum(Commands.Num());
}

void FCombatForgetInputBuffer::ResetRuntimeState()
{
	Storage.Reset();
	Storage.SetNum(FMath::Max(1, Settings.BufferCapacity));
	Head = 0;
	Count = 0;
	CurrentTick = 0;
	CurrentStateBits = 0;
	PreviousStateBits = 0;
	PressedBits = 0;
	ReleasedBits = 0;
	bHasReceivedTick = false;
	for (int32 BitIndex = 0; BitIndex < 16; ++BitIndex)
	{
		TokenStates[BitIndex] = {};
	}
}

bool FCombatForgetInputBuffer::Tick(uint16 StateBits, TArray<const FCombatForgeCommand*>& OutCommands)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("CombatForge.InputBuffer.Tick");

	OutCommands.Reset();
	++CurrentTick;
	StateBits = CombatForgeInput::NormalizeStateBits(StateBits);

	const uint16 PreviousBits = CurrentStateBits;
	const bool bStateChanged = StateBits != PreviousBits;
	const int32 Capacity = Storage.Num();
	if (Count < Capacity)
	{
		const int32 InsertIndex = (Head + Count) % Capacity;
		Storage[InsertIndex] = StateBits;
		++Count;
	}
	else
	{
		Storage[Head] = StateBits;
		Head = (Head + 1) % Capacity;
	}

	PreviousStateBits = PreviousBits;
	CurrentStateBits = StateBits;
	PressedBits = static_cast<uint16>(CurrentStateBits & ~PreviousStateBits);
	ReleasedBits = static_cast<uint16>(PreviousStateBits & ~CurrentStateBits);

	for (int32 BitIndex = 0; BitIndex < 16; ++BitIndex)
	{
		FTokenRuntimeState& TokenState = TokenStates[BitIndex];
		TokenState.PreviousAge = TokenState.Age;

		const uint16 BitMask = static_cast<uint16>(1u << BitIndex);
		const bool bIsDown = (StateBits & BitMask) != 0;
		if (bIsDown)
		{
			TokenState.Age = TokenState.PreviousAge > 0 ? static_cast<int16>(TokenState.PreviousAge + 1) : 1;
		}
		else
		{
			TokenState.Age = TokenState.PreviousAge < 0 ? static_cast<int16>(TokenState.PreviousAge - 1) : -1;
		}
	}

	SolveCommands(bStateChanged, OutCommands);
	bHasReceivedTick = true;
	return bStateChanged;
}

void FCombatForgetInputBuffer::GetBufferedStates(TArray<uint16>& OutStates) const
{
	OutStates.Reset();
	OutStates.Reserve(Count);
	for (int32 LogicalIndex = 0; LogicalIndex < Count; ++LogicalIndex)
	{
		OutStates.Add(GetStateAtLogicalIndex(LogicalIndex));
	}
}

void FCombatForgetInputBuffer::SolveCommands(bool bStateChanged, TArray<const FCombatForgeCommand*>& OutCommands)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("CombatForge.InputBuffer.SolveCommands");

	for (int32 CommandIndex = 0; CommandIndex < Commands.Num(); ++CommandIndex)
	{
		FCommandRuntimeState& RuntimeState = CommandRuntimeStates[CommandIndex];
		RuntimeState.bCompletedThisTick = false;
		if (TryAdvanceCommand(Commands[CommandIndex], RuntimeState, bStateChanged))
		{
			OutCommands.Add(&Commands[CommandIndex]);
		}
	}

	OutCommands.Sort([](const FCombatForgeCommand& A, const FCombatForgeCommand& B)
	{
		return FCombatForgetInputBuffer::IsCommandHigherPriority(A, B);
	});
}

bool FCombatForgetInputBuffer::TryAdvanceCommand(const FCombatForgeCommand& Command, FCommandRuntimeState& RuntimeState, bool bStateChanged)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("CombatForge.InputBuffer.TryAdvanceCommand");

	const int32 ElementCount = Command.Elements.Num();
	if (ElementCount <= 0)
	{
		return false;
	}

	const FCombatForgeCommandElement& LastElement = Command.Elements.Last();

	// Held-terminal commands emit once, then stay latched until the held condition stops matching.
	if (RuntimeState.bLatchedComplete)
	{
		if (MatchesElementOnCurrentTick(LastElement, bStateChanged))
		{
			return false;
		}

		RuntimeState.bLatchedComplete = false;
	}

	// Any partial sequence expires once it runs past its authored input window.
	if (RuntimeState.NextStepIndex > 0 && RuntimeState.SequenceStartTick != INDEX_NONE)
	{
		const int32 ElapsedTicks = CurrentTick - RuntimeState.SequenceStartTick;
		if (ElapsedTicks > Command.InputWindowFrames)
		{
			ResetCommandRuntimeState(RuntimeState);
		}
	}

	if (RuntimeState.NextStepIndex > 0)
	{
		const FCombatForgeCommandElement& PendingElement = Command.Elements[RuntimeState.NextStepIndex];
		// Strict steps must be the very next qualifying state change after the previous step.
		if (PendingElement.bStrictAfterPrevious
			&& bStateChanged
			&& PendingElement.MatchKind != ECombatForgeCommandElementMatchKind::Held
			&& !MatchesElementOnCurrentTick(PendingElement, true))
		{
			ResetCommandRuntimeState(RuntimeState);
		}
	}

	// Non-held steps cannot advance on a stable tick, so avoid re-evaluating them until input changes.
	if (RuntimeState.NextStepIndex < ElementCount
		&& !bStateChanged
		&& Command.Elements[RuntimeState.NextStepIndex].MatchKind != ECombatForgeCommandElementMatchKind::Held)
	{
		return false;
	}

	bool bConsumedEventThisTick = false;
	while (RuntimeState.NextStepIndex < ElementCount)
	{
		const FCombatForgeCommandElement& Element = Command.Elements[RuntimeState.NextStepIndex];
		if (RuntimeState.NextStepIndex > 0)
		{
			const FCombatForgeCommandElement& PreviousElement = Command.Elements[RuntimeState.NextStepIndex - 1];
			// If the previous step was a hold, it must remain valid while waiting for the next step.
			if (PreviousElement.MatchKind == ECombatForgeCommandElementMatchKind::Held
				&& !MatchesHeldElement(PreviousElement))
			{
				ResetCommandRuntimeState(RuntimeState);
				break;
			}
		}

		if (bConsumedEventThisTick && Element.MatchKind != ECombatForgeCommandElementMatchKind::Held)
		{
			// One press/release event can satisfy at most one non-held element per tick.
			break;
		}

		const bool bMatched = MatchesElementOnCurrentTick(Element, bStateChanged);
		
		if (!bMatched)
		{
			break;
		}

		if (RuntimeState.NextStepIndex == 0)
		{
			RuntimeState.SequenceStartTick = CurrentTick;
		}

		RuntimeState.LastMatchedTick = CurrentTick;
		++RuntimeState.NextStepIndex;
		if (Element.MatchKind != ECombatForgeCommandElementMatchKind::Held)
		{
			// Held elements may chain into later checks on the same tick; press/release elements may not.
			bConsumedEventThisTick = true;
		}
	}

	if (RuntimeState.NextStepIndex >= ElementCount)
	{
		const bool bLatchCompletion = LastElement.MatchKind == ECombatForgeCommandElementMatchKind::Held;
		ResetCommandRuntimeState(RuntimeState);
		RuntimeState.bCompletedThisTick = true;
		RuntimeState.bLatchedComplete = bLatchCompletion;
		return true;
	}

	return false;
}

void FCombatForgetInputBuffer::ResetCommandRuntimeState(FCommandRuntimeState& RuntimeState) const
{
	RuntimeState.NextStepIndex = 0;
	RuntimeState.SequenceStartTick = INDEX_NONE;
	RuntimeState.LastMatchedTick = INDEX_NONE;
	RuntimeState.bCompletedThisTick = false;
	RuntimeState.bLatchedComplete = false;
}

bool FCombatForgetInputBuffer::IsCommandHigherPriority(const FCombatForgeCommand& A, const FCombatForgeCommand& B)
{
	if (A.Priority != B.Priority)
	{
		return A.Priority > B.Priority;
	}
	if (A.Elements.Num() != B.Elements.Num())
	{
		return A.Elements.Num() > B.Elements.Num();
	}
	return A.Id < B.Id;
}

uint16 FCombatForgetInputBuffer::GetStateAtLogicalIndex(int32 LogicalIndex) const
{
	if (LogicalIndex < 0 || LogicalIndex >= Count || Storage.Num() == 0)
	{
		return 0;
	}
	return Storage[(Head + LogicalIndex) % Storage.Num()];
}

int32 FCombatForgetInputBuffer::GetMinimumHeldTicks(int32 Mask) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("CombatForge.InputBuffer.GetMinimumHeldTicks");

	if (Mask == 0)
	{
		return 0;
	}

	int32 MinimumHeldTicks = TNumericLimits<int32>::Max();
	uint32 Remaining = static_cast<uint32>(Mask) & 0xffffu;
	while (Remaining != 0)
	{
		const uint32 BitIndex = FMath::CountTrailingZeros(Remaining);
		Remaining &= (Remaining - 1);

		const int32 HeldTicks = FMath::Max(0, static_cast<int32>(TokenStates[BitIndex].Age));
		if (HeldTicks == 0)
		{
			return 0;
		}

		MinimumHeldTicks = FMath::Min(MinimumHeldTicks, HeldTicks);
	}

	return MinimumHeldTicks == TNumericLimits<int32>::Max() ? 0 : MinimumHeldTicks;
}

int32 FCombatForgetInputBuffer::GetMinimumPreviousHeldTicks(int32 Mask) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("CombatForge.InputBuffer.GetMinimumPreviousHeldTicks");

	if (Mask == 0)
	{
		return 0;
	}

	int32 MinimumHeldTicks = TNumericLimits<int32>::Max();
	uint32 Remaining = static_cast<uint32>(Mask) & 0xffffu;
	while (Remaining != 0)
	{
		const uint32 BitIndex = FMath::CountTrailingZeros(Remaining);
		Remaining &= (Remaining - 1);

		const int32 HeldTicks = FMath::Max(0, static_cast<int32>(TokenStates[BitIndex].PreviousAge));
		if (HeldTicks == 0)
		{
			return 0;
		}

		MinimumHeldTicks = FMath::Min(MinimumHeldTicks, HeldTicks);
	}

	return MinimumHeldTicks == TNumericLimits<int32>::Max() ? 0 : MinimumHeldTicks;
}

bool FCombatForgetInputBuffer::MatchesElementState(uint16 StateBits, int32 RequiredMask, int32 AcceptedMask)
{
	checkSlow(StateBits == CombatForgeInput::NormalizeStateBits(StateBits));
	if ((static_cast<int32>(StateBits) & RequiredMask) != RequiredMask)
	{
		return false;
	}

	const uint16 RequiredDirections = static_cast<uint16>(RequiredMask & InputBufferDirectionMask);
	if (RequiredDirections == 0)
	{
		return true;
	}

	uint16 AcceptedDirections = static_cast<uint16>(AcceptedMask & InputBufferDirectionMask);
	const uint16 StateDirections = StateBits & InputBufferDirectionMask;
	const uint16 ExtraDirections = static_cast<uint16>(StateDirections & ~RequiredDirections);
	return (ExtraDirections & static_cast<uint16>(~AcceptedDirections)) == 0;
}

bool FCombatForgetInputBuffer::MatchesHeldElement(const FCombatForgeCommandElement& Element) const
{
	if (!MatchesHeldElementState(CurrentStateBits, Element.RequiredMask, Element.AcceptedMask))
	{
		return false;
	}
	if (Element.MinHeldFrames > 0 && GetMinimumHeldTicks(Element.RequiredMask) < Element.MinHeldFrames)
	{
		return false;
	}
	return true;
}

bool FCombatForgetInputBuffer::MatchesPressElement(const FCombatForgeCommandElement& Element, bool bStateChanged) const
{
	if (!bStateChanged)
	{
		return false;
	}

	if (!IsDirectionalElement(Element)
		&& (PressedBits & static_cast<uint16>(Element.RequiredMask)) == 0)
	{
		return false;
	}

	if (!MatchesElementState(CurrentStateBits, Element.RequiredMask, Element.AcceptedMask))
	{
		return false;
	}
	return !MatchesElementState(PreviousStateBits, Element.RequiredMask, Element.AcceptedMask);
}

bool FCombatForgetInputBuffer::MatchesReleaseElement(const FCombatForgeCommandElement& Element, bool bStateChanged) const
{
	if (!bStateChanged)
	{
		return false;
	}

	if ((ReleasedBits & static_cast<uint16>(Element.RequiredMask)) == 0)
	{
		return false;
	}

	if (!MatchesElementState(PreviousStateBits, Element.RequiredMask, Element.AcceptedMask))
	{
		return false;
	}
	if (MatchesElementState(CurrentStateBits, Element.RequiredMask, Element.AcceptedMask))
	{
		return false;
	}
	if (Element.MinHeldFrames > 0 && GetMinimumPreviousHeldTicks(Element.RequiredMask) < Element.MinHeldFrames)
	{
		return false;
	}
	return true;
}

bool FCombatForgetInputBuffer::MatchesElementOnCurrentTick(const FCombatForgeCommandElement& Element, bool bStateChanged) const
{
	switch (Element.MatchKind)
	{
	case ECombatForgeCommandElementMatchKind::Held:
		return MatchesHeldElement(Element);
	case ECombatForgeCommandElementMatchKind::Release:
		return MatchesReleaseElement(Element, bStateChanged);
	case ECombatForgeCommandElementMatchKind::Press:
	default:
		return MatchesPressElement(Element, bStateChanged);
	}
}
