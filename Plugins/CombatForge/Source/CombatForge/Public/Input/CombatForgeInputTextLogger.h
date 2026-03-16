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

	virtual void Reset() override;
	virtual void AddCommandEntry(int32 Sequence, uint16 StateBits, const TArray<const FCombatForgeCommand*>& Commands) override;
	virtual void AddEventEntry(int32 Sequence, const TArray<FGameplayTag>& InputEvents) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UCombatForgeInputLoggerWidget> OutputWidget;
};
