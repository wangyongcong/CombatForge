// Copyright Epic Games, Inc. All Rights Reserved.

#include "Framework/CombatForgeCharacter.h"

#include "EnhancedInputComponent.h"
#include "Input/CombatForgeInputComponent.h"

ACombatForgeCharacter::ACombatForgeCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ACombatForgeCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	UCombatForgeInputComponent* CombatForgeInput = FindComponentByClass<UCombatForgeInputComponent>();
	if (EnhancedInputComponent != nullptr && CombatForgeInput != nullptr)
	{
		CombatForgeInput->BindEnhancedInput(EnhancedInputComponent);
	}
}
