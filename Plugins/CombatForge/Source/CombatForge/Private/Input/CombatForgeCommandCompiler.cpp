// Copyright Epic Games, Inc. All Rights Reserved.

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

	static FString RemoveWhitespace(const FString& InString)
	{
		FString Result;
		Result.Reserve(InString.Len());
		for (TCHAR Character : InString)
		{
			if (!FChar::IsWhitespace(Character))
			{
				Result.AppendChar(Character);
			}
		}
		return Result;
	}

	static bool TryConsumeLiteral(const FString& Source, int32& Index, const TCHAR* Literal)
	{
		const int32 Length = FCString::Strlen(Literal);
		if (Source.Mid(Index, Length).Equals(Literal))
		{
			Index += Length;
			return true;
		}
		return false;
	}

	static uint16 ExpandDirectionalSuperset(uint16 DirectionMaskBits)
	{
		switch (DirectionMaskBits)
		{
		case static_cast<uint16>(ECombatForgeInputToken::Up):
		case static_cast<uint16>(ECombatForgeInputToken::Down):
			return HorizontalMask;
		case static_cast<uint16>(ECombatForgeInputToken::Forward):
		case static_cast<uint16>(ECombatForgeInputToken::Back):
			return VerticalMask;
		default:
			return 0;
		}
	}

	static bool TryParseBaseSymbol(const FString& Source, int32& Index, uint16& OutMask, bool& bOutIsDirection, FString& OutNormalized)
	{
		if (TryConsumeLiteral(Source, Index, TEXT("UB")))
		{
			OutMask = static_cast<uint16>(ECombatForgeInputToken::Up) | static_cast<uint16>(ECombatForgeInputToken::Back);
			bOutIsDirection = true;
			OutNormalized = TEXT("UB");
			return true;
		}
		if (TryConsumeLiteral(Source, Index, TEXT("DB")))
		{
			OutMask = static_cast<uint16>(ECombatForgeInputToken::Down) | static_cast<uint16>(ECombatForgeInputToken::Back);
			bOutIsDirection = true;
			OutNormalized = TEXT("DB");
			return true;
		}
		if (TryConsumeLiteral(Source, Index, TEXT("DF")))
		{
			OutMask = static_cast<uint16>(ECombatForgeInputToken::Down) | static_cast<uint16>(ECombatForgeInputToken::Forward);
			bOutIsDirection = true;
			OutNormalized = TEXT("DF");
			return true;
		}
		if (TryConsumeLiteral(Source, Index, TEXT("UF")))
		{
			OutMask = static_cast<uint16>(ECombatForgeInputToken::Up) | static_cast<uint16>(ECombatForgeInputToken::Forward);
			bOutIsDirection = true;
			OutNormalized = TEXT("UF");
			return true;
		}
		if (TryConsumeLiteral(Source, Index, TEXT("z")))
		{
			OutMask = static_cast<uint16>(ECombatForgeInputToken::Z);
			bOutIsDirection = false;
			OutNormalized = TEXT("z");
			return true;
		}
		if (TryConsumeLiteral(Source, Index, TEXT("c")))
		{
			OutMask = static_cast<uint16>(ECombatForgeInputToken::C);
			bOutIsDirection = false;
			OutNormalized = TEXT("c");
			return true;
		}

		if (Index >= Source.Len())
		{
			return false;
		}

		switch (Source[Index])
		{
		case TEXT('U'):
			OutMask = static_cast<uint16>(ECombatForgeInputToken::Up);
			bOutIsDirection = true;
			OutNormalized = TEXT("U");
			++Index;
			return true;
		case TEXT('B'):
			OutMask = static_cast<uint16>(ECombatForgeInputToken::Back);
			bOutIsDirection = true;
			OutNormalized = TEXT("B");
			++Index;
			return true;
		case TEXT('D'):
			OutMask = static_cast<uint16>(ECombatForgeInputToken::Down);
			bOutIsDirection = true;
			OutNormalized = TEXT("D");
			++Index;
			return true;
		case TEXT('F'):
			OutMask = static_cast<uint16>(ECombatForgeInputToken::Forward);
			bOutIsDirection = true;
			OutNormalized = TEXT("F");
			++Index;
			return true;
		case TEXT('x'):
			OutMask = static_cast<uint16>(ECombatForgeInputToken::X);
			bOutIsDirection = false;
			OutNormalized = TEXT("x");
			++Index;
			return true;
		case TEXT('y'):
			OutMask = static_cast<uint16>(ECombatForgeInputToken::Y);
			bOutIsDirection = false;
			OutNormalized = TEXT("y");
			++Index;
			return true;
		case TEXT('z'):
			OutMask = static_cast<uint16>(ECombatForgeInputToken::Z);
			bOutIsDirection = false;
			OutNormalized = TEXT("z");
			++Index;
			return true;
		case TEXT('a'):
			OutMask = static_cast<uint16>(ECombatForgeInputToken::A);
			bOutIsDirection = false;
			OutNormalized = TEXT("a");
			++Index;
			return true;
		case TEXT('b'):
			OutMask = static_cast<uint16>(ECombatForgeInputToken::B);
			bOutIsDirection = false;
			OutNormalized = TEXT("b");
			++Index;
			return true;
		case TEXT('c'):
			OutMask = static_cast<uint16>(ECombatForgeInputToken::C);
			bOutIsDirection = false;
			OutNormalized = TEXT("c");
			++Index;
			return true;
		default:
			return false;
		}
	}

	static void AddError(TArray<FCombatForgeCompileMessage>& OutMessages, int32 CommandId, const FString& Message)
	{
		FCombatForgeCompileMessage& CompileMessage = OutMessages.AddDefaulted_GetRef();
		CompileMessage.bIsError = true;
		CompileMessage.CommandId = CommandId;
		CompileMessage.Message = Message;
	}

	static uint32 ComputeCommandSourceHash(const FCombatForgeInputRuntimeSettings& Settings, const FCombatForgeCommand& Command)
	{
		uint32 Hash = GetTypeHash(FCombatForgeCommandCompiler::CurrentCompilerVersion);
		Hash = HashCombineFast(Hash, GetTypeHash(Settings.DefaultInputWindowFrames));
		Hash = HashCombineFast(Hash, GetTypeHash(Command.Id));
		Hash = HashCombineFast(Hash, GetTypeHash(Command.CommandString));
		Hash = HashCombineFast(Hash, GetTypeHash(Command.InputWindowFrames));
		Hash = HashCombineFast(Hash, GetTypeHash(Command.Priority));
		return Hash;
	}

	static bool CompileSingleCommand(
		const FCombatForgeInputRuntimeSettings& Settings,
		FCombatForgeCommand& InOutCommand,
		TArray<FCombatForgeCompileMessage>& OutMessages)
	{
		const FString Source = RemoveWhitespace(InOutCommand.CommandString);
		if (Source.IsEmpty())
		{
			InOutCommand.CompiledVersion = FCombatForgeCommandCompiler::CurrentCompilerVersion;
			InOutCommand.CompiledSourceHash = ComputeCommandSourceHash(Settings, InOutCommand);
			return InOutCommand.Elements.Num() > 0;
		}

		TArray<FCombatForgeCommandElement> CompiledElements;
		InOutCommand.InputWindowFrames = InOutCommand.InputWindowFrames > 0 ? InOutCommand.InputWindowFrames : Settings.DefaultInputWindowFrames;

		int32 Index = 0;
		bool bStrictForNext = false;
		while (Index < Source.Len())
		{
			if (Source[Index] == TEXT('>'))
			{
				bStrictForNext = true;
				++Index;
				continue;
			}

			FCombatForgeCommandElement Element;
			Element.bStrictAfterPrevious = bStrictForNext;
			bStrictForNext = false;

			bool bIsHeld = false;
			bool bIsRelease = false;
			if (Index < Source.Len() && Source[Index] == TEXT('/'))
			{
				bIsHeld = true;
				++Index;
			}
			if (Index < Source.Len() && Source[Index] == TEXT('~'))
			{
				bIsRelease = true;
				++Index;
				const int32 NumberStart = Index;
				while (Index < Source.Len() && FChar::IsDigit(Source[Index]))
				{
					++Index;
				}
				if (NumberStart != Index)
				{
					Element.MinHeldFrames = FCString::Atoi(*Source.Mid(NumberStart, Index - NumberStart));
				}
			}

			Element.MatchKind = bIsRelease ? ECombatForgeCommandElementMatchKind::Release : (bIsHeld ? ECombatForgeCommandElementMatchKind::Held : ECombatForgeCommandElementMatchKind::Press);

			TArray<FString> ElementTokens;
			while (Index < Source.Len())
			{
				bool bSuperset = false;
				if (Source[Index] == TEXT('$'))
				{
					bSuperset = true;
					++Index;
				}

				uint16 ParsedMask = 0;
				bool bIsDirection = false;
				FString NormalizedToken;
				if (!TryParseBaseSymbol(Source, Index, ParsedMask, bIsDirection, NormalizedToken))
				{
					AddError(OutMessages, InOutCommand.Id, FString::Printf(TEXT("Command %d failed to compile near '%s'"), InOutCommand.Id, *Source.Mid(Index)));
					return false;
				}

				Element.RequiredMask |= ParsedMask;
				if (bSuperset)
				{
					if (!bIsDirection)
					{
						AddError(OutMessages, InOutCommand.Id, FString::Printf(TEXT("Command %d uses $ on a non-direction token"), InOutCommand.Id));
						return false;
					}
					Element.AcceptedMask |= ExpandDirectionalSuperset(ParsedMask);
				}

				if (Index >= Source.Len() || Source[Index] != TEXT('+'))
				{
					break;
				}
				++Index;
			}

			if (Element.RequiredMask == 0)
			{
				AddError(OutMessages, InOutCommand.Id, FString::Printf(TEXT("Command %d has an empty element"), InOutCommand.Id));
				return false;
			}

			CompiledElements.Add(Element);

			if (Index < Source.Len())
			{
				if (Source[Index] != TEXT(','))
				{
					AddError(OutMessages, InOutCommand.Id, FString::Printf(TEXT("Command %d failed to compile near '%s'"), InOutCommand.Id, *Source.Mid(Index)));
					return false;
				}
				++Index;
			}
		}

		InOutCommand.Elements = MoveTemp(CompiledElements);
		InOutCommand.CompiledVersion = FCombatForgeCommandCompiler::CurrentCompilerVersion;
		InOutCommand.CompiledSourceHash = ComputeCommandSourceHash(Settings, InOutCommand);
		return InOutCommand.Elements.Num() > 0;
	}
}

bool FCombatForgeCommandCompiler::IsCommandCurrent(
	const FCombatForgeInputRuntimeSettings& Settings,
	const FCombatForgeCommand& Command)
{
	if (Command.CompiledVersion != CurrentCompilerVersion)
	{
		return false;
	}

	if (Command.CompiledSourceHash != ComputeCommandSourceHash(Settings, Command))
	{
		return false;
	}

	if (!Command.CommandString.IsEmpty())
	{
		return Command.Elements.Num() > 0;
	}

	return Command.Elements.Num() > 0;
}

bool FCombatForgeCommandCompiler::CompileCommands(
	const FCombatForgeInputRuntimeSettings& Settings,
	TArray<FCombatForgeCommand>& InOutCommands,
	TArray<FCombatForgeCompileMessage>& OutMessages)
{
	OutMessages.Reset();
	bool bSucceeded = true;

	for (FCombatForgeCommand& Command : InOutCommands)
	{
		if (IsCommandCurrent(Settings, Command))
		{
			continue;
		}

		bSucceeded &= CompileSingleCommand(Settings, Command, OutMessages);
	}

	return bSucceeded && OutMessages.Num() == 0;
}

uint32 FCombatForgeCommandCompiler::ComputeSourceHash(
	const FCombatForgeInputRuntimeSettings& Settings,
	const TArray<FCombatForgeCommand>& Commands)
{
	uint32 Hash = GetTypeHash(CurrentCompilerVersion);
	Hash = HashCombineFast(Hash, GetTypeHash(Settings.BufferCapacity));
	Hash = HashCombineFast(Hash, GetTypeHash(Settings.MaxBufferFrames));
	Hash = HashCombineFast(Hash, GetTypeHash(Settings.DefaultInputWindowFrames));
	Hash = HashCombineFast(Hash, GetTypeHash(Settings.DirectionDeadZone));

	for (const FCombatForgeCommand& Command : Commands)
	{
		Hash = HashCombineFast(Hash, GetTypeHash(Command.Id));
		Hash = HashCombineFast(Hash, GetTypeHash(Command.CommandString));
		Hash = HashCombineFast(Hash, GetTypeHash(Command.InputWindowFrames));
		Hash = HashCombineFast(Hash, GetTypeHash(Command.Priority));
		Hash = HashCombineFast(Hash, GetTypeHash(Command.CompiledVersion));
		Hash = HashCombineFast(Hash, GetTypeHash(Command.CompiledSourceHash));
		for (const FCombatForgeCommandElement& Element : Command.Elements)
		{
			Hash = HashCombineFast(Hash, GetTypeHash(Element.RequiredMask));
			Hash = HashCombineFast(Hash, GetTypeHash(Element.AcceptedMask));
			Hash = HashCombineFast(Hash, GetTypeHash(static_cast<uint8>(Element.MatchKind)));
			Hash = HashCombineFast(Hash, GetTypeHash(Element.MinHeldFrames));
			Hash = HashCombineFast(Hash, GetTypeHash(Element.bStrictAfterPrevious));
		}
	}

	return Hash;
}
