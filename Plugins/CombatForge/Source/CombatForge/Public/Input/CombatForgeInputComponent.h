// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Input/CombatForgeInputLogger.h"
#include "CombatForgeInputComponent.generated.h"

class UEnhancedInputComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FCombatForgeInputEventDelegate, FGameplayTag EventTag);

UCLASS(Abstract, ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class COMBATFORGE_API UCombatForgeInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatForgeInputComponent();

	virtual void BindEnhancedInput(UEnhancedInputComponent* EnhancedInputComponent) PURE_VIRTUAL(UCombatForgeInputComponent::BindEnhancedInput, );

	void SetInputLogger(ICombatForgeInputLogger* InInputLogger);
	FCombatForgeInputEventDelegate OnInputEvent;

protected:
	virtual void BeginPlay() override;
	// virtual void ResetCurrentInputEvents();
	void EmitInputEvent(FGameplayTag EventTag) const;
	// void BroadcastInputEvents(const TArray<FCombatForgeInputEvent>& InputEvents) const;
	
	void LogInputCommands(uint16 StateBits, const TArray<const FCombatForgeCommand*>& Commands);
	void LogInputEvents(const TArray<FGameplayTag>& InputEvents);
	void ResetInputLogger();
	int32 NextDebugSequence();

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

protected:
	TScriptInterface<ICombatForgeInputLogger> InputLogger;
	int32 DebugSequenceCounter = 0;
};
