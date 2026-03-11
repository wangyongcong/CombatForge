// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/CombatForgeInputLogger.h"
#include "CombatForgeInputTextLogger.generated.h"

class UCombatForgeInputLoggerWidget;

UCLASS()
class COMBATFORGE_API UCombatForgeInputTextLogger : public UObject, public ICombatForgeInputLogger
{
	GENERATED_BODY()

public:
	void SetOutputWidget(UCombatForgeInputLoggerWidget* InOutputWidget);
	static void BuildDisplayTokens(uint16 StateBits, TArray<FCombatForgeInputDisplayToken>& OutTokens);
	static FString FormatStateBitsForDisplay(uint16 StateBits);

	virtual void ResetInputLog() override;
	virtual void AddInputLogEntry(int32 Sequence, uint16 StateBits, const TArray<const FCombatForgeCommand*>& Commands) override;

private:
	void AppendInputLogEntry(int32 Sequence, uint16 StateBits, const TArray<const FCombatForgeCommand*>& Commands);

private:
	UPROPERTY(Transient)
	TObjectPtr<UCombatForgeInputLoggerWidget> OutputWidget;
};
