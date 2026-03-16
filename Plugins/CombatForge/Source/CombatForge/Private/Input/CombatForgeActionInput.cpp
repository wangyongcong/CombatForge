// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/CombatForgeActionInput.h"

#include "EnhancedInputComponent.h"
#include "InputAction.h"

UCombatForgeActionInput::UCombatForgeActionInput()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCombatForgeActionInput::BeginPlay()
{
	Super::BeginPlay();
}

void UCombatForgeActionInput::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	++CurrentFrame;
}

void UCombatForgeActionInput::BindEnhancedInput(UEnhancedInputComponent* EnhancedInputComponent)
{
	if (EnhancedInputComponent == nullptr)
	{
		return;
	}

	for (const FCombatForgeInputTagBinding& Binding : InputActionBindings)
	{
		if (Binding.InputAction != nullptr && Binding.EventTag.IsValid())
		{
			EnhancedInputComponent->BindAction(Binding.InputAction.Get(), ETriggerEvent::Started, this, &UCombatForgeActionInput::HandleInputStarted);
		}
	}
}

void UCombatForgeActionInput::SetInputActionBinding(const UInputAction* InputAction, const FGameplayTag& EventTag)
{
	if (InputAction == nullptr || !EventTag.IsValid())
	{
		return;
	}

	for (FCombatForgeInputTagBinding& Binding : InputActionBindings)
	{
		if (Binding.InputAction == InputAction)
		{
			Binding.EventTag = EventTag;
			return;
		}
	}

	FCombatForgeInputTagBinding& NewBinding = InputActionBindings.AddDefaulted_GetRef();
	NewBinding.InputAction = const_cast<UInputAction*>(InputAction);
	NewBinding.EventTag = EventTag;
}

void UCombatForgeActionInput::ClearInputActionBindings()
{
	InputActionBindings.Reset();
}

bool UCombatForgeActionInput::EmitBoundInputEvent(const UInputAction* InputAction)
{
	for (const FCombatForgeInputTagBinding& Binding : InputActionBindings)
	{
		if (Binding.InputAction == InputAction && Binding.EventTag.IsValid())
		{
			EmitInputEvent(Binding.EventTag);
			return true;
		}
	}

	return false;
}

bool UCombatForgeActionInput::TriggerInputAction(const UInputAction* InputAction)
{
	return EmitBoundInputEvent(InputAction);
}

void UCombatForgeActionInput::HandleInputStarted(const FInputActionInstance& ActionInstance)
{
	EmitBoundInputEvent(ActionInstance.GetSourceAction());
}

