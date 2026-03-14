// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/CombatForgeInputComponent.h"
#include "CombatForge.h"
#include "Input/CombatForgeCommandConfig.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"

namespace
{
	static constexpr uint16 InputComponentDirectionMask = CombatForgeInput::DirectionMask;

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
		return static_cast<uint16>((StateBits & static_cast<uint16>(~InputComponentDirectionMask)) | NormalizeDirectionalBits(StateBits & InputComponentDirectionMask));
	}
}

UCombatForgeInputComponent::UCombatForgeInputComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCombatForgeInputComponent::SetInputActionBinding(const UInputAction* InputAction, ECombatForgeInputButton Button)
{
	const uint16 TokenMask = Button == ECombatForgeInputButton::None
		? 0
		: static_cast<uint16>(1u << (static_cast<uint8>(Button) + 7));
	if (InputAction == nullptr || TokenMask == 0 || (TokenMask & (TokenMask - 1)) != 0)
	{
		return;
	}

	for (FCombatForgeInputActionBinding& Binding : InputActionBindings)
	{
		if (Binding.InputAction == InputAction)
		{
			Binding.Button = Button;
			return;
		}
	}

	FCombatForgeInputActionBinding& NewBinding = InputActionBindings.AddDefaulted_GetRef();
	NewBinding.Button = Button;
	NewBinding.InputAction = const_cast<UInputAction*>(InputAction);
}

void UCombatForgeInputComponent::SetDirectionalInputAction(const UInputAction* InputAction)
{
	DirectionalInputAction = InputAction;
}

void UCombatForgeInputComponent::ClearInputActionBindings()
{
	InputActionBindings.Reset();
	DirectionalInputAction = nullptr;
}

void UCombatForgeInputComponent::BindEnhancedInput(UEnhancedInputComponent* EnhancedInputComponent)
{
	if (EnhancedInputComponent == nullptr)
	{
		return;
	}

	for (const FCombatForgeInputActionBinding& Binding : InputActionBindings)
	{
		if (Binding.InputAction != nullptr && Binding.Button != ECombatForgeInputButton::None)
		{
			EnhancedInputComponent->BindAction(Binding.InputAction.Get(), ETriggerEvent::Started, this, &UCombatForgeInputComponent::HandleInputStarted);
			EnhancedInputComponent->BindAction(Binding.InputAction.Get(), ETriggerEvent::Completed, this, &UCombatForgeInputComponent::HandleInputCompleted);
		}
	}

	if (DirectionalInputAction != nullptr)
	{
		EnhancedInputComponent->BindAction(DirectionalInputAction.Get(), ETriggerEvent::Triggered, this, &UCombatForgeInputComponent::HandleDirectionalInputTriggered);
		EnhancedInputComponent->BindAction(DirectionalInputAction.Get(), ETriggerEvent::Completed, this, &UCombatForgeInputComponent::HandleDirectionalInputCompleted);
		EnhancedInputComponent->BindAction(DirectionalInputAction.Get(), ETriggerEvent::Canceled, this, &UCombatForgeInputComponent::HandleDirectionalInputCompleted);
	}
}

void UCombatForgeInputComponent::GetBufferedStates(TArray<uint16>& OutStates) const
{
	InputBuffer.GetBufferedStates(OutStates);
}

void UCombatForgeInputComponent::SetInputLogger(UObject* InInputLogger)
{
	if (InInputLogger == nullptr)
	{
		InputLogger = nullptr;
		return;
	}

	if (!InInputLogger->GetClass()->ImplementsInterface(UCombatForgeInputLogger::StaticClass()))
	{
		return;
	}

	InputLogger.SetObject(InInputLogger);
	InputLogger.SetInterface(Cast<ICombatForgeInputLogger>(InInputLogger));
	if (InputLogger.GetInterface() != nullptr)
	{
		InputLogger->ResetInputLog();
	}
}

void UCombatForgeInputComponent::GetDebugRejections(TArray<FString>& OutReasons) const
{
	OutReasons = InputBuffer.GetDebugRejections();
}

void UCombatForgeInputComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeRuntime();
}

void UCombatForgeInputComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const int32 StepMs = FMath::Max(1, FixedStepMs);
	AccumulatorMs += static_cast<double>(DeltaTime) * 1000.0;
	while (AccumulatorMs >= static_cast<double>(StepMs))
	{
		StepSimulation();
		AccumulatorMs -= static_cast<double>(StepMs);
	}
}

void UCombatForgeInputComponent::InitializeRuntime()
{
	FCombatForgeInputRuntimeSettings Settings = RuntimeSettingsOverride;
	TArray<FCombatForgeCommand> Commands = CommandOverrides;

	if (CommandConfig != nullptr)
	{
		Settings = CommandConfig->RuntimeSettings;
		Commands = CommandConfig->Commands;
	}

	AccumulatorMs = 0.0;
	CurrentDirectionalValue = FVector2D::ZeroVector;
	CurrentButtonStateBits = 0;
	DebugSequenceCounter = 0;
	InputBuffer.Configure(Settings, Commands);
	if (InputLogger.GetInterface() != nullptr)
	{
		InputLogger->ResetInputLog();
	}
}

void UCombatForgeInputComponent::HandleInputStarted(const FInputActionInstance& ActionInstance)
{
	ECombatForgeInputToken Token = ECombatForgeInputToken::None;
	if (TryResolveTokenForAction(ActionInstance.GetSourceAction(), Token))
	{
		CurrentButtonStateBits |= static_cast<uint16>(Token);
	}
}

void UCombatForgeInputComponent::HandleInputCompleted(const FInputActionInstance& ActionInstance)
{
	ECombatForgeInputToken Token = ECombatForgeInputToken::None;
	if (TryResolveTokenForAction(ActionInstance.GetSourceAction(), Token))
	{
		CurrentButtonStateBits &= ~static_cast<uint16>(Token);
	}
}

void UCombatForgeInputComponent::HandleDirectionalInputTriggered(const FInputActionInstance& ActionInstance)
{
	CurrentDirectionalValue = ActionInstance.GetValue().Get<FVector2D>();
}

void UCombatForgeInputComponent::HandleDirectionalInputCompleted(const FInputActionInstance& ActionInstance)
{
	CurrentDirectionalValue = FVector2D::ZeroVector;
}

bool UCombatForgeInputComponent::TryResolveTokenForAction(const UInputAction* InputAction, ECombatForgeInputToken& OutToken) const
{
	if (InputAction == nullptr)
	{
		return false;
	}

	for (const FCombatForgeInputActionBinding& Binding : InputActionBindings)
	{
		if (Binding.InputAction == InputAction)
		{
			OutToken = static_cast<ECombatForgeInputToken>(1u << (static_cast<uint8>(Binding.Button) + 7));
			return OutToken != ECombatForgeInputToken::None;
		}
	}

	return false;
}

void UCombatForgeInputComponent::StepSimulation()
{
	const uint16 NewStateBits = static_cast<uint16>((CurrentButtonStateBits & static_cast<uint16>(~InputComponentDirectionMask)) | QuantizeDirectionBits());
	TArray<const FCombatForgeCommand*> Commands;
	const bool bStateChanged = InputBuffer.Tick(NewStateBits, Commands);

	if (bStateChanged)
	{
		const uint16 LoggedStateBits = NormalizeStateBits(NewStateBits);
		if (InputLogger.GetInterface() != nullptr)
		{
			InputLogger->AddInputLogEntry(++DebugSequenceCounter, LoggedStateBits, Commands);
		}
		OnRawInputEvent.Broadcast(LoggedStateBits);
		for (const FCombatForgeCommand* Command : Commands)
		{
			if (Command != nullptr)
			{
				OnCommand.Broadcast(*Command);
			}
		}
	}
}

uint16 UCombatForgeInputComponent::QuantizeDirectionalValue(const FVector2D& DirectionalValue, float DeadZone, float FacingSign)
{
	const float X = DirectionalValue.X;
	const float Y = DirectionalValue.Y;
	if (FMath::Abs(X) < DeadZone && FMath::Abs(Y) < DeadZone)
	{
		return 0;
	}

	const bool bUp = Y > DeadZone;
	const bool bDown = Y < -DeadZone;
	const float ForwardAxis = X * (FacingSign >= 0.0f ? 1.0f : -1.0f);
	const bool bForward = ForwardAxis > DeadZone;
	const bool bBack = ForwardAxis < -DeadZone;

	uint16 DirectionBits = 0;
	if (bUp && !bDown)
	{
		DirectionBits |= static_cast<uint16>(ECombatForgeInputToken::Up);
	}
	else if (bDown && !bUp)
	{
		DirectionBits |= static_cast<uint16>(ECombatForgeInputToken::Down);
	}

	if (bForward && !bBack)
	{
		DirectionBits |= static_cast<uint16>(ECombatForgeInputToken::Forward);
	}
	else if (bBack && !bForward)
	{
		DirectionBits |= static_cast<uint16>(ECombatForgeInputToken::Back);
	}

	return DirectionBits;
}

uint16 UCombatForgeInputComponent::QuantizeDirectionBits() const
{
	const float DeadZone = CommandConfig != nullptr ? CommandConfig->RuntimeSettings.DirectionDeadZone : RuntimeSettingsOverride.DirectionDeadZone;
	return QuantizeDirectionalValue(CurrentDirectionalValue, DeadZone);
}
