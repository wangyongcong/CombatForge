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
			if (!MatchesHeldElementState(CurrentState, Element.RequiredMask, Element.AcceptedMask))
			{
				return false;
			}
			return true;
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
				const int32 HeldTicks = GetMinimumHeldTicks(HeldTicksByBit, Element.RequiredMask);
				if (HeldTicks < Element.MinHeldFrames || TickIndex != LatestIndex)
				{
					return false;
				}
				return true;
			}
			return true;
		case ECombatForgeCommandElementMatchKind::Press:
		default:
			if (!MatchesElementState(CurrentState, Element.RequiredMask, Element.AcceptedMask))
			{
				return false;
			}
			if (MatchesElementState(PreviousState, Element.RequiredMask, Element.AcceptedMask))
			{
				return false;
			}
			return true;
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
			bool bElementMatched = MatchElementAt(Element, TickIndex);
			if (!bElementMatched
				&& ElementIndex == 0
				&& Element.MatchKind == ECombatForgeCommandElementMatchKind::Press
				&& IsDirectionalElement(Element))
			{
				const uint16 CurrentState = GetStateAtLogicalIndex(TickIndex);
				bElementMatched = MatchesElementState(CurrentState, Element.RequiredMask, Element.AcceptedMask);
			}

			if (!bElementMatched)
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

int32 FCombatForgetInputBuffer::GetMinimumHeldTicks(const int32 InHeldTicksByBit[16], int32 Mask)
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
