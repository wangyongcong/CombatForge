// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "CombatForgeCharacter.generated.h"

class UCombatForgeInputComponent;
class UInputComponent;

USTRUCT(BlueprintType)
struct COMBATFORGE_API FCombatForgeCharacterState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	FVector HorizontalVelocity = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	FVector Acceleration = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	FVector FacingDirection = FVector::ForwardVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	FVector LastMovementInput = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	float Speed = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	float HorizontalSpeed = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	bool bHasMovementInput = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	bool bIsMoving = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	bool bIsFalling = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	bool bIsCrouching = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	bool bWantsJump = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	FGameplayTag CurrentStateTag;
};

UCLASS(Abstract)
class COMBATFORGE_API ACombatForgeCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ACombatForgeCharacter();
	virtual void Tick(float DeltaSeconds) override;

	const FCombatForgeCharacterState& GetCharacterState() const;

	UFUNCTION(BlueprintCallable, Category = "Combat|State")
	void SetCurrentStateTag(FGameplayTag InStateTag);

	UFUNCTION(BlueprintCallable, Category = "Combat|State")
	void SetWantsJump(bool bInWantsJump);

	UFUNCTION(BlueprintCallable, Category = "Combat|State")
	void SetCharacterStateCrouching(bool bInIsCrouching);

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	void UpdateCharacterState();
	
	UPROPERTY(BlueprintReadWrite, Category = "Combat|Input")
	TObjectPtr<UCombatForgeInputComponent> CombatInput;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|State")
	FCombatForgeCharacterState CharacterState;
};
