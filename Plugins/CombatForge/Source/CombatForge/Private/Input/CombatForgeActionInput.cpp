// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/CombatForgeActionInput.h"
#include "Framework/CombatForgeCharacter.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "Framework/CombatForgeGameplayTags.h"

UCombatForgeActionInput::UCombatForgeActionInput()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCombatForgeActionInput::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<ACombatForgeCharacter>(GetOwner());
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

	// Direction inputs
	for (const FCombatForgeInputTagBinding& Binding : InputDirectionBindings)
	{
		if (Binding.InputAction != nullptr && Binding.EventTag.IsValid())
		{
			if (Binding.EventTag == CombatForgeTags::CombatForge_Input_Move)
			{
				EnhancedInputComponent->BindAction(Binding.InputAction, ETriggerEvent::Triggered, this, &UCombatForgeActionInput::HandleInputMove);
			}
			else if (Binding.EventTag == CombatForgeTags::CombatForge_Input_Look)
			{
				EnhancedInputComponent->BindAction(Binding.InputAction, ETriggerEvent::Triggered, this, &UCombatForgeActionInput::HandleInputLook);
			}
		}
	}
	
	// Action inputs
	for (const FCombatForgeInputTagBinding& Binding : InputActionBindings)
	{
		if (Binding.InputAction != nullptr && Binding.EventTag.IsValid())
		{
			EnhancedInputComponent->BindAction(Binding.InputAction.Get(), ETriggerEvent::Started, this, &UCombatForgeActionInput::HandleInputAction);
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

void UCombatForgeActionInput::HandleInputMove(const FInputActionValue& Value)
{
	AController* Controller = OwnerCharacter->GetController();
	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		FVector2D MovementVector = Value.Get<FVector2D>();
		OwnerCharacter->AddMovementInput(ForwardDirection, MovementVector.Y);
		OwnerCharacter->AddMovementInput(RightDirection, MovementVector.X);
	}
}

void UCombatForgeActionInput::HandleInputLook(const FInputActionValue& Value)
{
	AController* Controller = OwnerCharacter->GetController();
	if (Controller != nullptr)
	{
		// input is a Vector2D
		FVector2D LookAxisVector = Value.Get<FVector2D>();

		// add yaw and pitch input to controller
		OwnerCharacter->AddControllerYawInput(LookAxisVector.X);
		OwnerCharacter->AddControllerPitchInput(LookAxisVector.Y);
	}
}

void UCombatForgeActionInput::HandleInputAction(const FInputActionInstance& ActionInstance)
{
	EmitBoundInputEvent(ActionInstance.GetSourceAction());
}

