// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CombatForgeInputComponent.h"
#include "CombatForgeActionInput.generated.h"

class UEnhancedInputComponent;
struct FInputActionInstance;
class ACombatForgeCharacter;

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class COMBATFORGE_API UCombatForgeActionInput : public UCombatForgeInputComponent
{
	GENERATED_BODY()

public:
	UCombatForgeActionInput();
	
	void virtual BindEnhancedInput(UEnhancedInputComponent* EnhancedInputComponent) override;
	
	void SetInputActionBinding(const UInputAction* InputAction, const FGameplayTag& EventTag);
	void ClearInputActionBindings();
	bool TriggerInputAction(const UInputAction* InputAction);

protected:
	virtual void BeginPlay() override;

	bool EmitBoundInputEvent(const UInputAction* InputAction);

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Input")
	TObjectPtr<ACombatForgeCharacter> OwnerCharacter;
	
public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Input")
	TArray<FCombatForgeInputTagBinding> InputDirectionBindings;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Input")
	TArray<FCombatForgeInputTagBinding> InputActionBindings;

private:
	void HandleInputMove(const FInputActionValue& Value);
	void HandleInputLook(const FInputActionValue& Value);
	void HandleInputAction(const FInputActionInstance& ActionInstance);

private:
	double AccumulatorMs = 0.0;
	uint32 CurrentFrame = 0;
};
