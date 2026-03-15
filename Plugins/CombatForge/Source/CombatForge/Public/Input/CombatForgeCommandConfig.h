// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatForgeInputTypes.h"
#include "CombatForgeCommandConfig.generated.h"

UCLASS(BlueprintType)
class COMBATFORGE_API UCombatForgeCommandConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UCombatForgeCommandConfig();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Input")
	FCombatForgeInputRuntimeSettings RuntimeSettings;

	UPROPERTY(EditAnywhere, Category = "Combat|Input")
	TArray<FCombatForgeCommand> Commands;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Input|Compile")
	FString CompileErrorMessage;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Input|Compile")
	bool bCompileSucceeded = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Input|Compile")
	int32 CompileVersion = 0;

	UPROPERTY(VisibleAnywhere, Category = "Combat|Input|Compile")
	uint32 CompiledSourceHash = 0;

	UFUNCTION(CallInEditor, Category = "Combat|Input|Compile")
	bool CompileOffline();

	bool IsCompileCurrent() const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
#if WITH_EDITOR
	void EnsureUniqueCommandIds();
#endif
};
