// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatForgeCharacter.h"

#include "EnhancedInputComponent.h"
#include "Input/CombatForgeInputComponent.h"

ACombatForgeCharacter::ACombatForgeCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	CombatForgeInput = CreateDefaultSubobject<UCombatForgeInputComponent>(TEXT("CombatForgeInput"));
}

void ACombatForgeCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (CombatForgeInput == nullptr)
	{
		return;
	}

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		CombatForgeInput->BindEnhancedInput(EnhancedInputComponent);
	}
}
