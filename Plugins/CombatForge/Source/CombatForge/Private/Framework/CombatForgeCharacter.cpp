// Copyright Epic Games, Inc. All Rights Reserved.

#include "Framework/CombatForgeCharacter.h"

#include "CombatForge.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Input/CombatForgeInputComponent.h"

ACombatForgeCharacter::ACombatForgeCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Let the camera drive facing indirectly while movement controls actual turn direction.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	MovementComponent->bOrientRotationToMovement = true;
	MovementComponent->bUseControllerDesiredRotation = false;
	MovementComponent->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	MovementComponent->MaxAcceleration = 1400.0f;
	MovementComponent->BrakingFrictionFactor = 1.0f;
	MovementComponent->bUseSeparateBrakingFriction = true;
	MovementComponent->GroundFriction = 8.0f;
	MovementComponent->MaxWalkSpeed = 420.0f;
	MovementComponent->MinAnalogWalkSpeed = 20.0f;
	MovementComponent->BrakingDecelerationWalking = 1400.0f;
	MovementComponent->BrakingDecelerationFalling = 800.0f;
	MovementComponent->AirControl = 0.2f;
	MovementComponent->JumpZVelocity = 450.0f;
}

void ACombatForgeCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateCharacterState();
}

void ACombatForgeCharacter::BeginPlay()
{
	Super::BeginPlay();
	UpdateCharacterState();
}

void ACombatForgeCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	CombatInput = FindComponentByClass<UCombatForgeInputComponent>();
	if (EnhancedInputComponent != nullptr && CombatInput != nullptr)
	{
		CombatInput->BindEnhancedInput(EnhancedInputComponent);
	}
}

const FCombatForgeCharacterState& ACombatForgeCharacter::GetCharacterState() const
{
	return CharacterState;
}

void ACombatForgeCharacter::SetCurrentStateTag(FGameplayTag InStateTag)
{
	CharacterState.CurrentStateTag = InStateTag;
}

void ACombatForgeCharacter::SetWantsJump(bool bInWantsJump)
{
	CharacterState.bWantsJump = bInWantsJump;
}

void ACombatForgeCharacter::SetCharacterStateCrouching(bool bInIsCrouching)
{
	CharacterState.bIsCrouching = bInIsCrouching;
}

void ACombatForgeCharacter::UpdateCharacterState()
{
	CharacterState.Velocity = GetVelocity();
	CharacterState.HorizontalVelocity = FVector(CharacterState.Velocity.X, CharacterState.Velocity.Y, 0.0f);
	CharacterState.Speed = CharacterState.Velocity.Size();
	CharacterState.HorizontalSpeed = CharacterState.HorizontalVelocity.Size();

	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (MovementComponent != nullptr)
	{
		CharacterState.Acceleration = MovementComponent->GetCurrentAcceleration();
		CharacterState.bIsFalling = MovementComponent->IsFalling();
		CharacterState.bIsCrouching = bIsCrouched;
	}
	else
	{
		CharacterState.Acceleration = FVector::ZeroVector;
		CharacterState.bIsFalling = false;
		CharacterState.bIsCrouching = bIsCrouched;
	}

	const FVector LastMovementInput = GetLastMovementInputVector();
	CharacterState.LastMovementInput = LastMovementInput;
	CharacterState.bHasMovementInput = !LastMovementInput.IsNearlyZero();
	
	bool PreMoving = CharacterState.bIsMoving;
	CharacterState.bIsMoving = CharacterState.HorizontalSpeed > KINDA_SMALL_NUMBER;
	
	if (PreMoving != CharacterState.bIsMoving)
	{
		if (CharacterState.bIsMoving)
		{
			UE_LOG(LogCombatForge, Verbose, TEXT("Started moving"));
		}
		else
		{
			UE_LOG(LogCombatForge, Verbose, TEXT("Stopped moving"));
		}
	}

	if (CharacterState.bIsMoving)
	{
		UE_LOG(LogCombatForge, Verbose, TEXT("Moving: %s"), *CharacterState.Velocity.ToString());
		CharacterState.FacingDirection = CharacterState.HorizontalVelocity.GetSafeNormal();
	}
	else
	{
		CharacterState.FacingDirection = GetActorForwardVector();
	}
}
