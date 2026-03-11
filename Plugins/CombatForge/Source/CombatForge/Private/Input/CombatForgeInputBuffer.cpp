// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/CombatForgeInputBuffer.h"
#include "Input/CombatForgeCommandCompiler.h"

namespace
{
	static constexpr uint16 DirectionMask = CombatForgeInput::DirectionMask;
	static constexpr uint16 VerticalMask =
		static_cast<uint16>(ECombatForgeInputToken::Up) |
		static_cast<uint16>(ECombatForgeInputToken::Down);
	static constexpr uint16 HorizontalMask =
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
		return static_cast<uint16>((StateBits & static_cast<uint16>(~DirectionMask)) | NormalizeDirectionalBits(StateBits & DirectionMask));
	}

}

void FCombatForgetInputBuffer::Configure(const FCombatForgeInputRuntimeSettings& InSettings, const TArray<FCombatForgeCommand>& InCommands)
{
	Settings = InSettings;
	ResetRuntimeState();
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
}

void FCombatForgetInputBuffer::ResetRuntimeState()
{
	Storage.Reset();
	Storage.SetNum(FMath::Max(1, Settings.BufferCapacity));
	Head = 0;
	Count = 0;
	CurrentTick = 0;
	PreviousStateBits = 0;
	for (int32 BitIndex = 0; BitIndex < 16; ++BitIndex)
	{
		HeldTicksByBit[BitIndex] = 0;
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

	PreviousStateBits = StateBits;
	if (bStateChanged)
	{
		SolveCommands(OutCommands);
	}

	for (int32 BitIndex = 0; BitIndex < 16; ++BitIndex)
	{
		const uint16 BitMask = static_cast<uint16>(1u << BitIndex);
		const bool bIsDown = (StateBits & BitMask) != 0;
		if (bIsDown)
		{
			HeldTicksByBit[BitIndex] += 1;
		}
		else
		{
			HeldTicksByBit[BitIndex] = 0;
		}
	}

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

void FCombatForgetInputBuffer::SolveCommands(TArray<const FCombatForgeCommand*>& OutCommands) const
{
	for (const FCombatForgeCommand& Command : Commands)
	{
		if (TryMatchCommand(Command))
		{
			OutCommands.Add(&Command);
		}
	}
	OutCommands.Sort([](const FCombatForgeCommand& A, const FCombatForgeCommand& B)
	{
		return FCombatForgetInputBuffer::IsCommandHigherPriority(A, B);
	});
}

bool FCombatForgetInputBuffer::TryMatchCommand(const FCombatForgeCommand& Command) const
{
	if (Count <= 1 || Command.Elements.Num() <= 0)
	{
		return false;
	}

	const int32 LatestIndex = Count - 1;
	const int32 WindowStart = FMath::Max(0, Count - Command.InputWindowFrames);
	int32 MatchedTickIndex = LatestIndex;

	const auto MatchElementAt = [&](const FCombatForgeCommandElement& Element, int32 TickIndex) -> bool
	{
		if (TickIndex <= 0 || TickIndex >= Count)
		{
			return false;
		}
		const uint16 CurrentState = GetStateAtLogicalIndex(TickIndex);
		const uint16 PreviousState = GetStateAtLogicalIndex(TickIndex - 1);

		switch (Element.MatchKind)
		{
		case ECombatForgeCommandElementMatchKind::Held:
			return MatchesElementState(CurrentState, Element.RequiredMask, Element.AcceptedMask);
		case ECombatForgeCommandElementMatchKind::Release:
			if (!MatchesElementState(PreviousState, Element.RequiredMask, Element.AcceptedMask))
			{
				return false;
			}
			if (MatchesElementState(CurrentState, Element.RequiredMask, Element.AcceptedMask))
			{
				return false;
			}
			if (Element.MinHeldFrames > 0)
			{
				return GetMinimumHeldTicks(HeldTicksByBit, Element.RequiredMask) >= Element.MinHeldFrames && TickIndex == LatestIndex;
			}
			return true;
		case ECombatForgeCommandElementMatchKind::Press:
		default:
			return MatchesElementState(CurrentState, Element.RequiredMask, Element.AcceptedMask)
				&& !MatchesElementState(PreviousState, Element.RequiredMask, Element.AcceptedMask);
		}
	};

	if (!MatchElementAt(Command.Elements.Last(), LatestIndex))
	{
		return false;
	}

	for (int32 ElementIndex = Command.Elements.Num() - 2; ElementIndex >= 0; --ElementIndex)
	{
		const FCombatForgeCommandElement& Element = Command.Elements[ElementIndex];
		const FCombatForgeCommandElement& NextElement = Command.Elements[ElementIndex + 1];

		if (Element.MatchKind == ECombatForgeCommandElementMatchKind::Held)
		{
			if (!MatchElementAt(Element, MatchedTickIndex))
			{
				return false;
			}
			continue;
		}

		bool bFound = false;
		for (int32 TickIndex = MatchedTickIndex - 1; TickIndex >= WindowStart; --TickIndex)
		{
			if (!MatchElementAt(Element, TickIndex))
			{
				continue;
			}
			if (NextElement.bStrictAfterPrevious && HasInterveningChanges(TickIndex, MatchedTickIndex))
			{
				continue;
			}
			MatchedTickIndex = TickIndex;
			bFound = true;
			break;
		}

		if (!bFound)
		{
			return false;
		}
	}

	return true;
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

int32 FCombatForgetInputBuffer::GetMinimumHeldTicks(const int32 InHeldTicksByBit[16], uint16 Mask)
{
	int32 MinimumHeldTicks = TNumericLimits<int32>::Max();
	for (int32 BitIndex = 0; BitIndex < 16; ++BitIndex)
	{
		const uint16 BitMask = static_cast<uint16>(1u << BitIndex);
		if ((Mask & BitMask) == 0)
		{
			continue;
		}

		MinimumHeldTicks = FMath::Min(MinimumHeldTicks, InHeldTicksByBit[BitIndex]);
	}

	return MinimumHeldTicks == TNumericLimits<int32>::Max() ? 0 : MinimumHeldTicks;
}

bool FCombatForgetInputBuffer::MatchesElementState(uint16 StateBits, uint16 RequiredMask, uint16 AcceptedMask)
{
	StateBits = NormalizeStateBits(StateBits);
	if ((StateBits & RequiredMask) != RequiredMask)
	{
		return false;
	}

	const uint16 RequiredDirections = RequiredMask & DirectionMask;
	if (RequiredDirections == 0)
	{
		return true;
	}

	uint16 AcceptedDirections = AcceptedMask & DirectionMask;
	if (AcceptedDirections == 0)
	{
		const bool bHasVertical = (RequiredDirections & VerticalMask) != 0;
		const bool bHasHorizontal = (RequiredDirections & HorizontalMask) != 0;
		if (bHasVertical && !bHasHorizontal)
		{
			AcceptedDirections = HorizontalMask;
		}
		else if (bHasHorizontal && !bHasVertical)
		{
			AcceptedDirections = VerticalMask;
		}
	}

	const uint16 StateDirections = StateBits & DirectionMask;
	const uint16 ExtraDirections = static_cast<uint16>(StateDirections & ~RequiredDirections);
	return (ExtraDirections & static_cast<uint16>(~AcceptedDirections)) == 0;
}

bool FCombatForgetInputBuffer::HasInterveningChanges(int32 EarlierIndex, int32 LaterIndex) const
{
	if (EarlierIndex < 0 || LaterIndex >= Count || EarlierIndex >= LaterIndex)
	{
		return true;
	}
	const uint16 Baseline = GetStateAtLogicalIndex(EarlierIndex);
	for (int32 Index = EarlierIndex + 1; Index < LaterIndex; ++Index)
	{
		if (GetStateAtLogicalIndex(Index) != Baseline)
		{
			return true;
		}
	}
	return false;
}


