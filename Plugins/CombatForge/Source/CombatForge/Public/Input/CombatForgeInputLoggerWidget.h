// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatForgeInputLoggerWidget.generated.h"

UCLASS(Blueprintable)
class COMBATFORGE_API UCombatForgeInputLoggerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Combat|Input|Debug")
	void ResetLog();

	UFUNCTION(BlueprintNativeEvent, Category = "Combat|Input|Debug")
	void AppendLogLine(const FString& Line);

protected:
	virtual void ResetLog_Implementation();
	virtual void AppendLogLine_Implementation(const FString& Line);
};
