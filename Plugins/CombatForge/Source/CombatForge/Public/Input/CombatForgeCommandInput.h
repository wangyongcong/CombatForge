// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Input/CombatForgeInputBuffer.h"
#include "Input/CombatForgeInputComponent.h"
#include "CombatForgeCommandInput.generated.h"

class UCombatForgeCommandConfig;
class UEnhancedInputComponent;
struct FInputActionInstance;

DECLARE_MULTICAST_DELEGATE_OneParam(FCombatForgeCommandDelegate, const TArray<const FCombatForgeCommand*>&);

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class COMBATFORGE_API UCombatForgeCommandInput : public UCombatForgeInputComponent
{
	GENERATED_BODY()

public:
	UCombatForgeCommandInput();
	static uint16 QuantizeDirectionalValue(const FVector2D& DirectionalValue, float DeadZone, float FacingSign = 1.0f);

	virtual void BindEnhancedInput(UEnhancedInputComponent* EnhancedInputComponent) override;

	void SetInputActionBinding(const UInputAction* InputAction, ECombatForgeInputButton Button);
	void SetDirectionalInputAction(const UInputAction* InputAction);
	void ClearInputActionBindings();
	void GetBufferedStates(TArray<uint16>& OutStates) const;

	UFUNCTION(BlueprintCallable, Category = "Combat|Input")
	void GetDebugRejections(TArray<FString>& OutReasons) const;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Input")
	TObjectPtr<UCombatForgeCommandConfig> CommandConfig;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Input")
	TObjectPtr<const UInputAction> DirectionalInputAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Input")
	TArray<FCombatForgeInputButtonBinding> InputActionBindings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Input", meta = (ClampMin = 1, ClampMax = 100))
	int32 FixedStepMs = 16;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Input|Debug")
	bool bShowInputLogger = false;

	FCombatForgeCommandDelegate OnCommand;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void InitializeRuntime();
	void HandleInputStarted(const FInputActionInstance& ActionInstance);
	void HandleInputCompleted(const FInputActionInstance& ActionInstance);
	void HandleDirectionalInputTriggered(const FInputActionInstance& ActionInstance);
	void HandleDirectionalInputCompleted(const FInputActionInstance& ActionInstance);
	bool TryResolveTokenForAction(const UInputAction* InputAction, ECombatForgeInputToken& OutToken) const;
	void StepSimulation();
	uint16 QuantizeDirectionBits() const;

private:
	FCombatForgetInputBuffer InputBuffer;
	double AccumulatorMs = 0.0;
	FVector2D CurrentDirectionalValue = FVector2D::ZeroVector;
	uint16 CurrentButtonStateBits = 0;
	TArray<const FCombatForgeCommand*> CurrentInputCommands;
};
