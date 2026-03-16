// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CombatForgeCharacter.generated.h"

class UCombatForgeCommandInput;
class UInputComponent;

UCLASS(Abstract)
class COMBATFORGE_API ACombatForgeCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ACombatForgeCharacter();

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
};
