// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/CombatForgeInputTextLogger.h"

#include "Input/CombatForgeInputLoggerWidget.h"
#include "Input/CombatForgeInputTypes.h"

namespace
{
	static constexpr uint16 DirectionMask = CombatForgeInput::DirectionMask;

	static void AddToken(
		TArray<FCombatForgeInputDisplayToken>& Tokens,
		ECombatForgeInputDisplayTokenKind Kind,
		const TCHAR* Text)
	{
		FCombatForgeInputDisplayToken& Token = Tokens.AddDefaulted_GetRef();
		Token.Kind = Kind;
		Token.Text = Text;
	}
}

void UCombatForgeInputTextLogger::SetOutputWidget(UCombatForgeInputLoggerWidget* InOutputWidget)
{
	OutputWidget = InOutputWidget;
}

void UCombatForgeInputTextLogger::BuildDisplayTokens(uint16 StateBits, TArray<FCombatForgeInputDisplayToken>& OutTokens)
{
	OutTokens.Reset();

	const uint16 DirectionBits = StateBits & DirectionMask;
	switch (DirectionBits)
	{
	case static_cast<uint16>(ECombatForgeInputToken::Up):
		AddToken(OutTokens, ECombatForgeInputDisplayTokenKind::Direction, TEXT("↑"));
		break;
	case static_cast<uint16>(ECombatForgeInputToken::Down):
		AddToken(OutTokens, ECombatForgeInputDisplayTokenKind::Direction, TEXT("↓"));
		break;
	case static_cast<uint16>(ECombatForgeInputToken::Forward):
		AddToken(OutTokens, ECombatForgeInputDisplayTokenKind::Direction, TEXT("→"));
		break;
	case static_cast<uint16>(ECombatForgeInputToken::Back):
		AddToken(OutTokens, ECombatForgeInputDisplayTokenKind::Direction, TEXT("←"));
		break;
	case static_cast<uint16>(ECombatForgeInputToken::Up) | static_cast<uint16>(ECombatForgeInputToken::Forward):
		AddToken(OutTokens, ECombatForgeInputDisplayTokenKind::Direction, TEXT("↗"));
		break;
	case static_cast<uint16>(ECombatForgeInputToken::Up) | static_cast<uint16>(ECombatForgeInputToken::Back):
		AddToken(OutTokens, ECombatForgeInputDisplayTokenKind::Direction, TEXT("↖"));
		break;
	case static_cast<uint16>(ECombatForgeInputToken::Down) | static_cast<uint16>(ECombatForgeInputToken::Forward):
		AddToken(OutTokens, ECombatForgeInputDisplayTokenKind::Direction, TEXT("↘"));
		break;
	case static_cast<uint16>(ECombatForgeInputToken::Down) | static_cast<uint16>(ECombatForgeInputToken::Back):
		AddToken(OutTokens, ECombatForgeInputDisplayTokenKind::Direction, TEXT("↙"));
		break;
	default:
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
		if ((StateBits & static_cast<uint16>(Button.Token)) != 0)
		{
			AddToken(OutTokens, ECombatForgeInputDisplayTokenKind::Button, Button.Label);
		}
	}

	if (OutTokens.Num() == 0)
	{
		AddToken(OutTokens, ECombatForgeInputDisplayTokenKind::Text, TEXT("N"));
	}
}

FString UCombatForgeInputTextLogger::FormatStateBitsForDisplay(uint16 StateBits)
{
	TArray<FCombatForgeInputDisplayToken> Tokens;
	BuildDisplayTokens(StateBits, Tokens);

	TArray<FString> Parts;
	Parts.Reserve(Tokens.Num());
	for (const FCombatForgeInputDisplayToken& Token : Tokens)
	{
		Parts.Add(Token.Text);
	}

	return FString::Join(Parts, TEXT(" "));
}

void UCombatForgeInputTextLogger::ResetInputLog()
{
	if (OutputWidget != nullptr)
	{
		OutputWidget->ResetLog();
	}
}

void UCombatForgeInputTextLogger::AddInputLogEntry(int32 Sequence, uint16 StateBits, const TArray<const FCombatForgeCommand*>& Commands)
{
	AppendInputLogEntry(Sequence, StateBits, Commands);
}

void UCombatForgeInputTextLogger::AppendInputLogEntry(int32 Sequence, uint16 StateBits, const TArray<const FCombatForgeCommand*>& Commands)
{
	(void)Sequence;
	FString Line = FormatStateBitsForDisplay(StateBits);

	TArray<FString> MatchedNames;
	for (const FCombatForgeCommand* Command : Commands)
	{
		if (Command == nullptr)
		{
			continue;
		}

		if (!Command->DisplayName.IsEmpty())
		{
			MatchedNames.Add(Command->DisplayName);
		}
		else if (!Command->CommandString.IsEmpty())
		{
			MatchedNames.Add(Command->CommandString);
		}
		else
		{
			MatchedNames.Add(FString::Printf(TEXT("Command %d"), Command->Id));
		}
	}

	if (MatchedNames.Num() > 0)
	{
		Line += FString::Printf(TEXT("  |  %s"), *FString::Join(MatchedNames, TEXT(", ")));
	}

	UE_LOG(LogTemp, Log, TEXT("%s"), *Line);
	if (OutputWidget != nullptr)
	{
		OutputWidget->AppendLogLine(Line);
	}
}
