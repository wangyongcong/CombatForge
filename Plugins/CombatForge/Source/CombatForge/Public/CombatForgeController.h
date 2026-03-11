// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CombatForgeController.generated.h"

class ACombatForgeCharacter;
class UCombatForgeInputLoggerWidget;
class UCombatForgeInputTextLogger;

UCLASS(Abstract, Config="Game")
class COMBATFORGE_API ACombatForgeController : public APlayerController
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category="UI|Debug")
	TSubclassOf<UCombatForgeInputLoggerWidget> InputLoggerWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UCombatForgeInputLoggerWidget> InputLoggerWidget;

	UPROPERTY(Transient)
	TObjectPtr<UCombatForgeInputTextLogger> InputTextLogger;

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

protected:
	virtual ACombatForgeCharacter* GetCombatForgeCharacterForLogging() const;
	void EnsureInputLoggerWidget();
	void RefreshInputLoggerBinding();
};
