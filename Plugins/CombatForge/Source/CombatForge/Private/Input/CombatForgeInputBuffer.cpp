// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/CombatForgeInputBuffer.h"
#include "CombatForge.h"
#include "Input/CombatForgeCommandCompiler.h"

namespace
{
	static constexpr uint16 InputBufferDirectionMask = CombatForgeInput::DirectionMask;
	static constexpr uint16 InputBufferVerticalMask =
		static_cast<uint16>(ECombatForgeInputToken::Up) |
		static_cast<uint16>(ECombatForgeInputToken::Down);
	static constexpr uint16 InputBufferHorizontalMask =
		static_cast<uint16>(ECombatForgeInputToken::Forward) |
		static_cast<uint16>(ECombatForgeInputToken::Back);

	static uint16 NormalizeDirectionalBits(uint16 DirectionBits)
	{
		bool bUp = (DirectionBits & static_cast<uint16>(ECombatForgeInputToken::Up)) != 0;
		bool bDown = (DirectionBits & static_cast<uint16>(ECombatForgeInputToken::Down)) != 0;
		bool bForward = (DirectionBits & static_cast<uint16>(ECombatForgeInputToken::Forward)) != 0;
		bool bBack = (DirectionBits & static_cast<uint16>(ECombatForgeInputToken::Back)) != 0;

		if (bUp && bDown)
		{
			bUp = false;
			bDown = false;
		}
		if (bForward && bBack)
		{
			bForward = false;
			bBack = false;
		}

		uint16 Result = 0;
		if (bUp)
		{
			Result |= static_cast<uint16>(ECombatForgeInputToken::Up);
		}
		if (bDown)
		{
			Result |= static_cast<uint16>(ECombatForgeInputToken::Down);
		}
		if (bForward)
		{
			Result |= static_cast<uint16>(ECombatForgeInputToken::Forward);
		}
		if (bBack)
		{
			Result |= static_cast<uint16>(ECombatForgeInputToken::Back);
		}
		return Result;
	}

	static uint16 NormalizeStateBits(uint16 StateBits)
	{
		return static_cast<uint16>((StateBits & static_cast<uint16>(~InputBufferDirectionMask)) | NormalizeDirectionalBits(StateBits & InputBufferDirectionMask));
	}

	static FString FormatStateBitsForLog(uint16 StateBits)
	{
		const uint16 NormalizedBits = NormalizeStateBits(StateBits);
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
		StateBits = NormalizeStateBits(StateBits);
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
	PreviousStateBits = 0;
	bHasReceivedTick = false;
	for (int32 BitIndex = 0; BitIndex < 16; ++BitIndex)
	{
		TokenStates[BitIndex] = {};
	}
}

bool FCombatForgetInputBuffer::Tick(uint16 StateBits, TArray<const FCombatForgeCommand*>& OutCommands)
{
	OutCommands.Reset();
	++CurrentTick;
	StateBits = NormalizeStateBits(StateBits);

	const uint16 PreviousBits = PreviousStateBits;
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

	PreviousStateBits = StateBits;
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
	if (Command.Elements.Num() <= 0)
	{
		return false;
	}

	if (RuntimeState.bLatchedComplete)
	{
		if (MatchesElementOnCurrentTick(Command.Elements.Last(), bStateChanged))
		{
			return false;
		}

		RuntimeState.bLatchedComplete = false;
	}

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
		if (PendingElement.bStrictAfterPrevious
			&& bStateChanged
			&& PendingElement.MatchKind != ECombatForgeCommandElementMatchKind::Held
			&& !MatchesElementOnCurrentTick(PendingElement, true))
		{
			ResetCommandRuntimeState(RuntimeState);
		}
	}

	bool bAdvanced = false;
	bool bConsumedEventThisTick = false;
	while (RuntimeState.NextStepIndex < Command.Elements.Num())
	{
		const FCombatForgeCommandElement& Element = Command.Elements[RuntimeState.NextStepIndex];
		if (RuntimeState.NextStepIndex > 0)
		{
			const FCombatForgeCommandElement& PreviousElement = Command.Elements[RuntimeState.NextStepIndex - 1];
			if (PreviousElement.MatchKind == ECombatForgeCommandElementMatchKind::Held
				&& !MatchesHeldElement(PreviousElement))
			{
				ResetCommandRuntimeState(RuntimeState);
				break;
			}
		}

		if (bConsumedEventThisTick && Element.MatchKind != ECombatForgeCommandElementMatchKind::Held)
		{
			UE_LOG(
				LogCombatForge,
				Verbose,
				TEXT("Command %d waiting at step %d because this tick already consumed an event"),
				Command.Id,
				RuntimeState.NextStepIndex);
			break;
		}

		const bool bMatched = MatchesElementOnCurrentTick(Element, bStateChanged);
		UE_LOG(
			LogCombatForge,
			Verbose,
			TEXT("Command %d step %d %s on tick %d current=%s changed=%s matched=%s"),
			Command.Id,
			RuntimeState.NextStepIndex,
			*DescribeElement(Element),
			CurrentTick,
			*FormatStateBitsForLog(PreviousStateBits),
			bStateChanged ? TEXT("true") : TEXT("false"),
			bMatched ? TEXT("true") : TEXT("false"));

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
		bAdvanced = true;
		if (Element.MatchKind != ECombatForgeCommandElementMatchKind::Held)
		{
			bConsumedEventThisTick = true;
		}
	}

	if (RuntimeState.NextStepIndex >= Command.Elements.Num())
	{
		const bool bLatchCompletion = Command.Elements.Last().MatchKind == ECombatForgeCommandElementMatchKind::Held;
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
	int32 MinimumHeldTicks = TNumericLimits<int32>::Max();
	for (int32 BitIndex = 0; BitIndex < 16; ++BitIndex)
	{
		const uint16 BitMask = static_cast<uint16>(1u << BitIndex);
		if ((Mask & BitMask) == 0)
		{
			continue;
		}

		MinimumHeldTicks = FMath::Min(MinimumHeldTicks, FMath::Max(0, static_cast<int32>(TokenStates[BitIndex].Age)));
	}

	return MinimumHeldTicks == TNumericLimits<int32>::Max() ? 0 : MinimumHeldTicks;
}

int32 FCombatForgetInputBuffer::GetMinimumPreviousHeldTicks(int32 Mask) const
{
	int32 MinimumHeldTicks = TNumericLimits<int32>::Max();
	for (int32 BitIndex = 0; BitIndex < 16; ++BitIndex)
	{
		const uint16 BitMask = static_cast<uint16>(1u << BitIndex);
		if ((Mask & BitMask) == 0)
		{
			continue;
		}

		MinimumHeldTicks = FMath::Min(MinimumHeldTicks, FMath::Max(0, static_cast<int32>(TokenStates[BitIndex].PreviousAge)));
	}

	return MinimumHeldTicks == TNumericLimits<int32>::Max() ? 0 : MinimumHeldTicks;
}

bool FCombatForgetInputBuffer::MatchesElementState(uint16 StateBits, int32 RequiredMask, int32 AcceptedMask)
{
	StateBits = NormalizeStateBits(StateBits);
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
	if (!MatchesHeldElementState(PreviousStateBits, Element.RequiredMask, Element.AcceptedMask))
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

	const uint16 CurrentState = PreviousStateBits;
	const uint16 PreviousState = Count >= 2 ? GetStateAtLogicalIndex(Count - 2) : 0;
	if (!MatchesElementState(CurrentState, Element.RequiredMask, Element.AcceptedMask))
	{
		return false;
	}
	return !MatchesElementState(PreviousState, Element.RequiredMask, Element.AcceptedMask);
}

bool FCombatForgetInputBuffer::MatchesReleaseElement(const FCombatForgeCommandElement& Element, bool bStateChanged) const
{
	if (!bStateChanged)
	{
		return false;
	}

	const uint16 CurrentState = PreviousStateBits;
	uint16 PreviousState = 0;
	if (Count >= 2)
	{
		PreviousState = GetStateAtLogicalIndex(Count - 2);
	}

	if (!MatchesElementState(PreviousState, Element.RequiredMask, Element.AcceptedMask))
	{
		return false;
	}
	if (MatchesElementState(CurrentState, Element.RequiredMask, Element.AcceptedMask))
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
