// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CombatForgeCharacter.generated.h"

class UCombatForgeInputComponent;
class UInputComponent;

UCLASS(Abstract)
class COMBATFORGE_API ACombatForgeCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ACombatForgeCharacter();

	UCombatForgeInputComponent* GetCombatInput() const { return CombatForgeInput; }

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatForgeInputComponent> CombatForgeInput;
};
