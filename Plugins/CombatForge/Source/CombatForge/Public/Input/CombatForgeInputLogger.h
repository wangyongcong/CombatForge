// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Input/CombatForgeInputTypes.h"
#include "CombatForgeInputLogger.generated.h"

UINTERFACE()
class COMBATFORGE_API UCombatForgeInputLogger : public UInterface
{
	GENERATED_BODY()
};

class COMBATFORGE_API ICombatForgeInputLogger
{
	GENERATED_BODY()

public:
	virtual void Reset() = 0;
	virtual void AddCommandEntry(int32 Sequence, uint16 StateBits, const TArray<const FCombatForgeCommand*>& Commands) = 0;
	virtual void AddEventEntry(int32 Sequence, FGameplayTag InputEvent) = 0;
};
