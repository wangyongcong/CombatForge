// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/CombatForgeInputBuffer.h"
#include "Input/CombatForgeCommandCompiler.h"
#include "Input/CombatForgeCommandConfig.h"
#include "Input/CombatForgeInputComponent.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CombatForgeInputTestHelpers
{
	static constexpr uint16 DownForward =
		static_cast<uint16>(ECombatForgeInputToken::Down) |
		static_cast<uint16>(ECombatForgeInputToken::Forward);
	static constexpr uint16 DownBack =
		static_cast<uint16>(ECombatForgeInputToken::Down) |
		static_cast<uint16>(ECombatForgeInputToken::Back);
	static constexpr uint16 UpForward =
		static_cast<uint16>(ECombatForgeInputToken::Up) |
		static_cast<uint16>(ECombatForgeInputToken::Forward);
	static constexpr uint16 UpBack =
		static_cast<uint16>(ECombatForgeInputToken::Up) |
		static_cast<uint16>(ECombatForgeInputToken::Back);

	static void TickState(FCombatForgetInputBuffer& InputBuffer, uint16 StateBits, TArray<const FCombatForgeCommand*>& OutCommands)
	{
		InputBuffer.Tick(StateBits, OutCommands);
	}

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatForgeCommandCompilerDeterminismTest,
	"CombatForge.Input.Compiler.DeterministicHashAndNormalize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatForgeCommandCompilerDeterminismTest::RunTest(const FString& Parameters)
{
	FCombatForgeInputRuntimeSettings Settings;
	Settings.DefaultInputWindowFrames = 20;

	FCombatForgeCommand Definition;
	Definition.Id = 10;
	Definition.CommandString = TEXT("  $D + x , > ~30a ");
	Definition.InputWindowFrames = 0;
	Definition.Priority = 7;

	TArray<FCombatForgeCompileMessage> FirstMessages;
	TArray<FCombatForgeCommand> FirstCompile{ Definition };
	const bool bFirstSucceeded = FCombatForgeCommandCompiler::CompileCommands(Settings, FirstCompile, FirstMessages);
	TestTrue(TEXT("Compiler accepts valid command definitions"), bFirstSucceeded);
	TestEqual(TEXT("Compiler emits exactly one compiled command"), FirstCompile.Num(), 1);
	TestEqual(TEXT("Compiler writes structured element data"), FirstCompile[0].Elements.Num(), 2);
	TestEqual(TEXT("Compiler preserves source command string"), FirstCompile[0].CommandString, FString(TEXT("  $D + x , > ~30a ")));

	TArray<FCombatForgeCommand> SecondCompile{ Definition };
	TArray<FCombatForgeCompileMessage> SecondMessages;
	const bool bSecondSucceeded = FCombatForgeCommandCompiler::CompileCommands(Settings, SecondCompile, SecondMessages);
	TestTrue(TEXT("Compiler remains deterministic across repeated compiles"), bSecondSucceeded);
	TestEqual(TEXT("Repeated compile preserves structured element count"), SecondCompile[0].Elements.Num(), FirstCompile[0].Elements.Num());
	TestEqual(TEXT("Repeated compile preserves first element mask"), SecondCompile[0].Elements[0].RequiredMask, FirstCompile[0].Elements[0].RequiredMask);

	const uint32 FirstHash = FCombatForgeCommandCompiler::ComputeSourceHash(Settings, FirstCompile);
	const uint32 SecondHash = FCombatForgeCommandCompiler::ComputeSourceHash(Settings, SecondCompile);
	TestEqual(TEXT("Source hash is deterministic"), SecondHash, FirstHash);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatForgeCommandConfigCacheValidityTest,
	"CombatForge.Input.Compiler.ConfigCacheValidity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatForgeCommandConfigCacheValidityTest::RunTest(const FString& Parameters)
{
	UCombatForgeCommandConfig* Config = NewObject<UCombatForgeCommandConfig>();
	Config->RuntimeSettings.DefaultInputWindowFrames = 20;

	FCombatForgeCommand Definition;
	Definition.Id = 11;
	Definition.CommandString = TEXT("a,b");
	Config->Commands = { Definition };

	TestFalse(TEXT("Fresh assets start without a current compile"), Config->IsCompileCurrent());
	TestTrue(TEXT("Offline compile succeeds for valid assets"), Config->CompileOffline());
	TestEqual(TEXT("Offline compile writes structured elements back to the asset"), Config->Commands[0].Elements.Num(), 2);
	TestTrue(TEXT("Offline compile marks the asset current"), Config->IsCompileCurrent());

	Config->Commands[0].CommandString = TEXT("a,>b");
	TestFalse(TEXT("Editing source commands marks compile state stale"), Config->IsCompileCurrent());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatForgeCommandDisplayNameHashStabilityTest,
	"CombatForge.Input.Compiler.DisplayNameIgnoredByHash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatForgeCommandDisplayNameHashStabilityTest::RunTest(const FString& Parameters)
{
	FCombatForgeInputRuntimeSettings Settings;

	FCombatForgeCommand Command;
	Command.Id = 21;
	Command.DisplayName = TEXT("Hadouken");
	Command.CommandString = TEXT("D,DF,F,x");
	Command.InputWindowFrames = 20;
	Command.Priority = 5;

	TArray<FCombatForgeCommand> Commands{ Command };
	const uint32 FirstHash = FCombatForgeCommandCompiler::ComputeSourceHash(Settings, Commands);

	Commands[0].DisplayName = TEXT("Fireball");
	const uint32 SecondHash = FCombatForgeCommandCompiler::ComputeSourceHash(Settings, Commands);

	TestEqual(TEXT("Display name changes do not alter compiler source hash"), SecondHash, FirstHash);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatForgeInputBufferDeterminismTest,
	"CombatForge.Input.Buffer.WraparoundAndExpiry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatForgeInputBufferDeterminismTest::RunTest(const FString& Parameters)
{
	FCombatForgeInputRuntimeSettings Settings;
	Settings.BufferCapacity = 3;
	Settings.MaxBufferFrames = 2;
	Settings.DefaultInputWindowFrames = 20;

	FCombatForgetInputBuffer InputBuffer;
	InputBuffer.Configure(Settings, {});

	TArray<const FCombatForgeCommand*> Intents;
	CombatForgeInputTestHelpers::TickState(InputBuffer, static_cast<uint16>(ECombatForgeInputToken::X), Intents);
	CombatForgeInputTestHelpers::TickState(InputBuffer, 0, Intents);
	CombatForgeInputTestHelpers::TickState(InputBuffer, static_cast<uint16>(ECombatForgeInputToken::Y), Intents);
	CombatForgeInputTestHelpers::TickState(InputBuffer, 0, Intents);

	TArray<uint16> Buffered;
	InputBuffer.GetBufferedStates(Buffered);
	TestEqual(TEXT("Ring buffer keeps fixed-size history"), Buffered.Num(), 3);
	TestEqual(TEXT("Newest buffered state stores latest bits"), Buffered[2], 0u);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatForgeInputMugenSequenceTest,
	"CombatForge.Input.Matcher.MugenSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatForgeInputMugenSequenceTest::RunTest(const FString& Parameters)
{
	FCombatForgeInputRuntimeSettings Settings;
	Settings.BufferCapacity = 16;
	Settings.DefaultInputWindowFrames = 10;

	FCombatForgeCommand Plain;
	Plain.Id = 1;
	Plain.CommandString = TEXT("a,b");
	Plain.InputWindowFrames = 10;

	FCombatForgetInputBuffer InputBuffer;
	InputBuffer.Configure(Settings, { Plain });
	TArray<const FCombatForgeCommand*> Intents;
	CombatForgeInputTestHelpers::TickState(InputBuffer, 0, Intents);
	CombatForgeInputTestHelpers::TickState(InputBuffer, static_cast<uint16>(ECombatForgeInputToken::A), Intents);
	CombatForgeInputTestHelpers::TickState(InputBuffer, static_cast<uint16>(ECombatForgeInputToken::A), Intents);
	CombatForgeInputTestHelpers::TickState(InputBuffer, static_cast<uint16>(ECombatForgeInputToken::A) | static_cast<uint16>(ECombatForgeInputToken::B), Intents);
	TestEqual(TEXT("a,b does not require releasing a before pressing b"), Intents.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatForgeInputMugenHeldTest,
	"CombatForge.Input.Matcher.MugenHeld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatForgeInputMugenHeldTest::RunTest(const FString& Parameters)
{
	FCombatForgeInputRuntimeSettings Settings;
	Settings.BufferCapacity = 16;
	Settings.DefaultInputWindowFrames = 10;

	FCombatForgeCommand Held;
	Held.Id = 2;
	Held.CommandString = TEXT("/a,b");
	Held.InputWindowFrames = 10;

	FCombatForgetInputBuffer InputBuffer;
	InputBuffer.Configure(Settings, { Held });
	TArray<const FCombatForgeCommand*> Intents;
	CombatForgeInputTestHelpers::TickState(InputBuffer, 0, Intents);
	CombatForgeInputTestHelpers::TickState(InputBuffer, static_cast<uint16>(ECombatForgeInputToken::A), Intents);
	CombatForgeInputTestHelpers::TickState(InputBuffer, static_cast<uint16>(ECombatForgeInputToken::A) | static_cast<uint16>(ECombatForgeInputToken::B), Intents);
	TestEqual(TEXT("/a,b requires a to be held when b is pressed"), Intents.Num(), 1);

	FCombatForgetInputBuffer FailBuffer;
	FailBuffer.Configure(Settings, { Held });
	Intents.Reset();
	CombatForgeInputTestHelpers::TickState(FailBuffer, 0, Intents);
	CombatForgeInputTestHelpers::TickState(FailBuffer, static_cast<uint16>(ECombatForgeInputToken::A), Intents);
	CombatForgeInputTestHelpers::TickState(FailBuffer, 0, Intents);
	CombatForgeInputTestHelpers::TickState(FailBuffer, static_cast<uint16>(ECombatForgeInputToken::B), Intents);
	TestEqual(TEXT("/a,b fails if a is not held at b"), Intents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatForgeInputMugenStrictTest,
	"CombatForge.Input.Matcher.MugenStrict",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatForgeInputMugenStrictTest::RunTest(const FString& Parameters)
{
	FCombatForgeInputRuntimeSettings Settings;
	Settings.BufferCapacity = 16;
	Settings.DefaultInputWindowFrames = 10;

	FCombatForgeCommand Strict;
	Strict.Id = 3;
	Strict.CommandString = TEXT("a,>b");
	Strict.InputWindowFrames = 10;

	FCombatForgetInputBuffer InputBuffer;
	InputBuffer.Configure(Settings, { Strict });
	TArray<const FCombatForgeCommand*> Intents;
	CombatForgeInputTestHelpers::TickState(InputBuffer, 0, Intents);
	CombatForgeInputTestHelpers::TickState(InputBuffer, static_cast<uint16>(ECombatForgeInputToken::A), Intents);
	CombatForgeInputTestHelpers::TickState(InputBuffer, static_cast<uint16>(ECombatForgeInputToken::A), Intents);
	CombatForgeInputTestHelpers::TickState(InputBuffer, static_cast<uint16>(ECombatForgeInputToken::A) | static_cast<uint16>(ECombatForgeInputToken::B), Intents);
	TestEqual(TEXT("a,>b matches when there are no intervening changes"), Intents.Num(), 1);

	FCombatForgetInputBuffer FailBuffer;
	FailBuffer.Configure(Settings, { Strict });
	Intents.Reset();
	CombatForgeInputTestHelpers::TickState(FailBuffer, 0, Intents);
	CombatForgeInputTestHelpers::TickState(FailBuffer, static_cast<uint16>(ECombatForgeInputToken::A), Intents);
	CombatForgeInputTestHelpers::TickState(FailBuffer, static_cast<uint16>(ECombatForgeInputToken::A) | static_cast<uint16>(ECombatForgeInputToken::X), Intents);
	CombatForgeInputTestHelpers::TickState(FailBuffer, static_cast<uint16>(ECombatForgeInputToken::A) | static_cast<uint16>(ECombatForgeInputToken::X) | static_cast<uint16>(ECombatForgeInputToken::B), Intents);
	TestEqual(TEXT("a,>b fails with an intervening input change"), Intents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatForgeInputMugenReleaseAndChargeTest,
	"CombatForge.Input.Matcher.MugenReleaseAndCharge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatForgeInputMugenReleaseAndChargeTest::RunTest(const FString& Parameters)
{
	FCombatForgeInputRuntimeSettings Settings;
	Settings.BufferCapacity = 64;
	Settings.DefaultInputWindowFrames = 40;

	FCombatForgeCommand Release;
	Release.Id = 4;
	Release.CommandString = TEXT("~a,b");
	Release.InputWindowFrames = 10;

	FCombatForgeCommand Charge;
	Charge.Id = 5;
	Charge.CommandString = TEXT("~30a");
	Charge.InputWindowFrames = 40;
	Charge.Priority = 2;

	FCombatForgetInputBuffer ReleaseBuffer;
	ReleaseBuffer.Configure(Settings, { Release });
	TArray<const FCombatForgeCommand*> Intents;
	CombatForgeInputTestHelpers::TickState(ReleaseBuffer, 0, Intents);
	CombatForgeInputTestHelpers::TickState(ReleaseBuffer, static_cast<uint16>(ECombatForgeInputToken::A), Intents);
	CombatForgeInputTestHelpers::TickState(ReleaseBuffer, 0, Intents);
	CombatForgeInputTestHelpers::TickState(ReleaseBuffer, static_cast<uint16>(ECombatForgeInputToken::B), Intents);
	TestEqual(TEXT("~a,b matches release then button"), Intents.Num(), 1);

	FCombatForgetInputBuffer ChargeBuffer;
	ChargeBuffer.Configure(Settings, { Charge });
	Intents.Reset();
	CombatForgeInputTestHelpers::TickState(ChargeBuffer, 0, Intents);
	for (int32 Tick = 0; Tick < 30; ++Tick)
	{
		CombatForgeInputTestHelpers::TickState(ChargeBuffer, static_cast<uint16>(ECombatForgeInputToken::A), Intents);
	}
	CombatForgeInputTestHelpers::TickState(ChargeBuffer, 0, Intents);
	TestEqual(TEXT("~30a matches a charged release"), Intents.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatForgeInputMugenDirectionalSupersetTest,
	"CombatForge.Input.Matcher.MugenDirectionalSuperset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatForgeInputMugenDirectionalSupersetTest::RunTest(const FString& Parameters)
{
	FCombatForgeInputRuntimeSettings Settings;
	Settings.BufferCapacity = 16;
	Settings.DefaultInputWindowFrames = 10;

	FCombatForgeCommand Command;
	Command.Id = 6;
	Command.CommandString = TEXT("$D,x");
	Command.InputWindowFrames = 10;

	FCombatForgetInputBuffer InputBuffer;
	InputBuffer.Configure(Settings, { Command });

	TArray<const FCombatForgeCommand*> Intents;
	CombatForgeInputTestHelpers::TickState(InputBuffer, 0, Intents);
	CombatForgeInputTestHelpers::TickState(InputBuffer, CombatForgeInputTestHelpers::DownForward, Intents);
	CombatForgeInputTestHelpers::TickState(
		InputBuffer,
		CombatForgeInputTestHelpers::DownForward | static_cast<uint16>(ECombatForgeInputToken::X),
		Intents);

	TestEqual(TEXT("$D,x accepts a diagonal down direction"), Intents.Num(), 1);

	FCombatForgetInputBuffer RejectBuffer;
	RejectBuffer.Configure(Settings, { Command });
	Intents.Reset();
	CombatForgeInputTestHelpers::TickState(RejectBuffer, 0, Intents);
	CombatForgeInputTestHelpers::TickState(RejectBuffer, static_cast<uint16>(ECombatForgeInputToken::Forward), Intents);
	CombatForgeInputTestHelpers::TickState(
		RejectBuffer,
		static_cast<uint16>(ECombatForgeInputToken::Forward) | static_cast<uint16>(ECombatForgeInputToken::X),
		Intents);
	TestEqual(TEXT("$D,x rejects forward without down"), Intents.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatForgeInputExactCardinalDirectionTest,
	"CombatForge.Input.Matcher.ExactCardinalDirections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatForgeInputExactCardinalDirectionTest::RunTest(const FString& Parameters)
{
	FCombatForgeInputRuntimeSettings Settings;
	Settings.BufferCapacity = 16;
	Settings.DefaultInputWindowFrames = 10;

	FCombatForgeCommand ForwardAndButton;
	ForwardAndButton.Id = 7;
	ForwardAndButton.CommandString = TEXT("F+a");
	ForwardAndButton.InputWindowFrames = 10;

	FCombatForgetInputBuffer DiagonalRejectBuffer;
	DiagonalRejectBuffer.Configure(Settings, { ForwardAndButton });

	TArray<const FCombatForgeCommand*> Intents;
	CombatForgeInputTestHelpers::TickState(DiagonalRejectBuffer, 0, Intents);
	CombatForgeInputTestHelpers::TickState(
		DiagonalRejectBuffer,
		CombatForgeInputTestHelpers::DownForward | static_cast<uint16>(ECombatForgeInputToken::A),
		Intents);
	TestEqual(TEXT("F+a rejects diagonal down-forward when exact cardinals are required"), Intents.Num(), 0);

	FCombatForgetInputBuffer ForwardAcceptBuffer;
	ForwardAcceptBuffer.Configure(Settings, { ForwardAndButton });
	Intents.Reset();
	CombatForgeInputTestHelpers::TickState(ForwardAcceptBuffer, 0, Intents);
	CombatForgeInputTestHelpers::TickState(
		ForwardAcceptBuffer,
		static_cast<uint16>(ECombatForgeInputToken::Forward) | static_cast<uint16>(ECombatForgeInputToken::A),
		Intents);
	TestEqual(TEXT("F+a still matches exact forward plus button"), Intents.Num(), 1);

	FCombatForgeCommand DownSuperset;
	DownSuperset.Id = 8;
	DownSuperset.CommandString = TEXT("$D+a");
	DownSuperset.InputWindowFrames = 10;

	FCombatForgetInputBuffer SupersetBuffer;
	SupersetBuffer.Configure(Settings, { DownSuperset });
	Intents.Reset();
	CombatForgeInputTestHelpers::TickState(SupersetBuffer, 0, Intents);
	CombatForgeInputTestHelpers::TickState(
		SupersetBuffer,
		CombatForgeInputTestHelpers::DownForward | static_cast<uint16>(ECombatForgeInputToken::A),
		Intents);
	TestEqual(TEXT("$D+a still accepts diagonal down-forward"), Intents.Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatForgeInputHeldCardinalNeighborDiagonalTest,
	"CombatForge.Input.Matcher.HeldCardinalAcceptsNeighborDiagonal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatForgeInputHeldCardinalNeighborDiagonalTest::RunTest(const FString& Parameters)
{
	FCombatForgeInputRuntimeSettings Settings;
	Settings.BufferCapacity = 16;
	Settings.DefaultInputWindowFrames = 10;

	FCombatForgeCommand QuarterCircle;
	QuarterCircle.Id = 9;
	QuarterCircle.CommandString = TEXT("/D,DF,F+a");
	QuarterCircle.InputWindowFrames = 10;

	FCombatForgetInputBuffer InputBuffer;
	InputBuffer.Configure(Settings, { QuarterCircle });

	TArray<const FCombatForgeCommand*> Intents;
	CombatForgeInputTestHelpers::TickState(InputBuffer, 0, Intents);
	CombatForgeInputTestHelpers::TickState(InputBuffer, static_cast<uint16>(ECombatForgeInputToken::Down), Intents);
	CombatForgeInputTestHelpers::TickState(InputBuffer, CombatForgeInputTestHelpers::DownForward, Intents);
	CombatForgeInputTestHelpers::TickState(InputBuffer, static_cast<uint16>(ECombatForgeInputToken::Forward), Intents);
	CombatForgeInputTestHelpers::TickState(
		InputBuffer,
		static_cast<uint16>(ECombatForgeInputToken::Forward) | static_cast<uint16>(ECombatForgeInputToken::A),
		Intents);
	TestEqual(TEXT("/D succeeds on the DF anchor tick for quarter-circle motions"), Intents.Num(), 1);

	FCombatForgeCommand ExactForwardAndButton;
	ExactForwardAndButton.Id = 10;
	ExactForwardAndButton.CommandString = TEXT("F+a");
	ExactForwardAndButton.InputWindowFrames = 10;

	FCombatForgetInputBuffer ExactForwardBuffer;
	ExactForwardBuffer.Configure(Settings, { ExactForwardAndButton });
	Intents.Reset();
	CombatForgeInputTestHelpers::TickState(ExactForwardBuffer, 0, Intents);
	CombatForgeInputTestHelpers::TickState(
		ExactForwardBuffer,
		CombatForgeInputTestHelpers::DownForward | static_cast<uint16>(ECombatForgeInputToken::A),
		Intents);
	TestEqual(TEXT("Press semantics remain exact for plain forward"), Intents.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCombatForgeInputFirstDirectionalPressAcceptsHeldStateTest,
	"CombatForge.Input.Matcher.FirstDirectionalPressAcceptsHeldState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCombatForgeInputFirstDirectionalPressAcceptsHeldStateTest::RunTest(const FString& Parameters)
{
	FCombatForgeInputRuntimeSettings Settings;
	Settings.BufferCapacity = 16;
	Settings.DefaultInputWindowFrames = 10;

	FCombatForgeCommand DragonPunch;
	DragonPunch.Id = 11;
	DragonPunch.CommandString = TEXT("F,D,DF+a");
	DragonPunch.InputWindowFrames = 10;

	FCombatForgetInputBuffer InputBuffer;
	InputBuffer.Configure(Settings, { DragonPunch });

	TArray<const FCombatForgeCommand*> Intents;
	CombatForgeInputTestHelpers::TickState(InputBuffer, 0, Intents);
	CombatForgeInputTestHelpers::TickState(InputBuffer, static_cast<uint16>(ECombatForgeInputToken::Forward), Intents);
	CombatForgeInputTestHelpers::TickState(InputBuffer, CombatForgeInputTestHelpers::DownForward, Intents);
	CombatForgeInputTestHelpers::TickState(InputBuffer, static_cast<uint16>(ECombatForgeInputToken::Down), Intents);
	CombatForgeInputTestHelpers::TickState(
		InputBuffer,
		CombatForgeInputTestHelpers::DownForward | static_cast<uint16>(ECombatForgeInputToken::A),
		Intents);
	TestEqual(TEXT("First directional press element can match from held state"), Intents.Num(), 1);

	FCombatForgeCommand Buttons;
	Buttons.Id = 12;
	Buttons.CommandString = TEXT("a,b");
	Buttons.InputWindowFrames = 10;

	FCombatForgetInputBuffer ButtonBuffer;
	ButtonBuffer.Configure(Settings, { Buttons });
	Intents.Reset();
	CombatForgeInputTestHelpers::TickState(ButtonBuffer, 0, Intents);
	CombatForgeInputTestHelpers::TickState(ButtonBuffer, static_cast<uint16>(ECombatForgeInputToken::A), Intents);
	CombatForgeInputTestHelpers::TickState(ButtonBuffer, static_cast<uint16>(ECombatForgeInputToken::A), Intents);
	CombatForgeInputTestHelpers::TickState(ButtonBuffer, static_cast<uint16>(ECombatForgeInputToken::A) | static_cast<uint16>(ECombatForgeInputToken::B), Intents);
	TestEqual(TEXT("Non-directional first press semantics are unchanged"), Intents.Num(), 1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
