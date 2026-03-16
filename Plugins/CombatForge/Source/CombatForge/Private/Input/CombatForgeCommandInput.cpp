// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/CombatForgeCommandInput.h"
#include "CombatForge.h"
#include "Input/CombatForgeCommandConfig.h"
#include "Input/CombatForgeInputUtility.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"


UCombatForgeCommandInput::UCombatForgeCommandInput()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCombatForgeCommandInput::SetInputActionBinding(const UInputAction* InputAction, ECombatForgeInputButton Button)
{
	const uint16 TokenMask = Button == ECombatForgeInputButton::None
		? 0
		: static_cast<uint16>(1u << (static_cast<uint8>(Button) + 7));
	if (InputAction == nullptr || TokenMask == 0 || (TokenMask & (TokenMask - 1)) != 0)
	{
		return;
	}

	for (FCombatForgeInputButtonBinding& Binding : InputActionBindings)
	{
		if (Binding.InputAction == InputAction)
		{
			Binding.Button = Button;
			return;
		}
	}

	FCombatForgeInputButtonBinding& NewBinding = InputActionBindings.AddDefaulted_GetRef();
	NewBinding.Button = Button;
	NewBinding.InputAction = const_cast<UInputAction*>(InputAction);
}

void UCombatForgeCommandInput::SetDirectionalInputAction(const UInputAction* InputAction)
{
	DirectionalInputAction = InputAction;
}

void UCombatForgeCommandInput::ClearInputActionBindings()
{
	InputActionBindings.Reset();
	DirectionalInputAction = nullptr;
}

void UCombatForgeCommandInput::BindEnhancedInput(UEnhancedInputComponent* EnhancedInputComponent)
{
	if (EnhancedInputComponent == nullptr)
	{
		return;
	}

	for (const FCombatForgeInputButtonBinding& Binding : InputActionBindings)
	{
		if (Binding.InputAction != nullptr && Binding.Button != ECombatForgeInputButton::None)
		{
			EnhancedInputComponent->BindAction(Binding.InputAction.Get(), ETriggerEvent::Started, this, &UCombatForgeCommandInput::HandleInputStarted);
			EnhancedInputComponent->BindAction(Binding.InputAction.Get(), ETriggerEvent::Completed, this, &UCombatForgeCommandInput::HandleInputCompleted);
		}
	}

	if (DirectionalInputAction != nullptr)
	{
		EnhancedInputComponent->BindAction(DirectionalInputAction.Get(), ETriggerEvent::Triggered, this, &UCombatForgeCommandInput::HandleDirectionalInputTriggered);
		EnhancedInputComponent->BindAction(DirectionalInputAction.Get(), ETriggerEvent::Completed, this, &UCombatForgeCommandInput::HandleDirectionalInputCompleted);
		EnhancedInputComponent->BindAction(DirectionalInputAction.Get(), ETriggerEvent::Canceled, this, &UCombatForgeCommandInput::HandleDirectionalInputCompleted);
	}
}

void UCombatForgeCommandInput::GetBufferedStates(TArray<uint16>& OutStates) const
{
	InputBuffer.GetBufferedStates(OutStates);
}

void UCombatForgeCommandInput::GetDebugRejections(TArray<FString>& OutReasons) const
{
	OutReasons = InputBuffer.GetDebugRejections();
}

void UCombatForgeCommandInput::BeginPlay()
{
	Super::BeginPlay();
	InitializeRuntime();
}

void UCombatForgeCommandInput::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
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

void UCombatForgeCommandInput::InitializeRuntime()
{
	AccumulatorMs = 0.0;
	CurrentDirectionalValue = FVector2D::ZeroVector;
	CurrentButtonStateBits = 0;
	if (CommandConfig != nullptr)
	{
		InputBuffer.Configure(CommandConfig->RuntimeSettings, CommandConfig->Commands);
	}
	ResetInputLogger();
}

void UCombatForgeCommandInput::HandleInputStarted(const FInputActionInstance& ActionInstance)
{
	ECombatForgeInputToken Token = ECombatForgeInputToken::None;
	if (TryResolveTokenForAction(ActionInstance.GetSourceAction(), Token))
	{
		CurrentButtonStateBits |= static_cast<uint16>(Token);
	}
}

void UCombatForgeCommandInput::HandleInputCompleted(const FInputActionInstance& ActionInstance)
{
	ECombatForgeInputToken Token = ECombatForgeInputToken::None;
	if (TryResolveTokenForAction(ActionInstance.GetSourceAction(), Token))
	{
		CurrentButtonStateBits &= ~static_cast<uint16>(Token);
	}
}

void UCombatForgeCommandInput::HandleDirectionalInputTriggered(const FInputActionInstance& ActionInstance)
{
	CurrentDirectionalValue = ActionInstance.GetValue().Get<FVector2D>();
}

void UCombatForgeCommandInput::HandleDirectionalInputCompleted(const FInputActionInstance& ActionInstance)
{
	CurrentDirectionalValue = FVector2D::ZeroVector;
}

bool UCombatForgeCommandInput::TryResolveTokenForAction(const UInputAction* InputAction, ECombatForgeInputToken& OutToken) const
{
	if (InputAction == nullptr)
	{
		return false;
	}

	for (const FCombatForgeInputButtonBinding& Binding : InputActionBindings)
	{
		if (Binding.InputAction == InputAction)
		{
			OutToken = static_cast<ECombatForgeInputToken>(1u << (static_cast<uint8>(Binding.Button) + 7));
			return OutToken != ECombatForgeInputToken::None;
		}
	}

	return false;
}

void UCombatForgeCommandInput::StepSimulation()
{
	const uint16 NewStateBits = static_cast<uint16>((CurrentButtonStateBits & static_cast<uint16>(~CombatForgeInput::DirectionMask)) | QuantizeDirectionBits());
	const bool bStateChanged = InputBuffer.Tick(NewStateBits, CurrentInputCommands);

	if (bStateChanged || !CurrentInputCommands.IsEmpty())
	{
		OnCommand.Broadcast(CurrentInputCommands);
		
		if (OnInputEvent.IsBound())
		{
			for (const FCombatForgeCommand* Command : CurrentInputCommands)
			{
				if (Command->EventTag.IsValid())
				{
					EmitInputEvent(Command->EventTag);
				}
			}
		}

		if (InputLogger.GetInterface() != nullptr)
		{
			LogInputCommands(NewStateBits, CurrentInputCommands);
		}
	}
}

uint16 UCombatForgeCommandInput::QuantizeDirectionalValue(const FVector2D& DirectionalValue, float DeadZone, float FacingSign)
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

uint16 UCombatForgeCommandInput::QuantizeDirectionBits() const
{
	const float DeadZone = CommandConfig != nullptr ? CommandConfig->RuntimeSettings.DirectionDeadZone : 0;
	return QuantizeDirectionalValue(CurrentDirectionalValue, DeadZone);
}
