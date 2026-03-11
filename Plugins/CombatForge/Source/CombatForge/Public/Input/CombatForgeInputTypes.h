// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputAction.h"
#include "CombatForgeInputTypes.generated.h"

enum class ECombatForgeInputToken : uint16
{
	None = 0,

	Up = 1 << 0,
	Down = 1 << 1,
	Forward = 1 << 2,
	Back = 1 << 3,

	X = 1 << 8,
	Y = 1 << 9,
	Z = 1 << 10,
	A = 1 << 11,
	B = 1 << 12,
	C = 1 << 13
};
ENUM_CLASS_FLAGS(ECombatForgeInputToken);

UENUM(BlueprintType)
enum class ECombatForgeInputButton : uint8
{
	None = 0,
	X,
	Y,
	Z,
	A,
	B,
	C
};

namespace CombatForgeInput
{
	static constexpr uint16 DirectionMask =
		static_cast<uint16>(ECombatForgeInputToken::Up) |
		static_cast<uint16>(ECombatForgeInputToken::Down) |
		static_cast<uint16>(ECombatForgeInputToken::Forward) |
		static_cast<uint16>(ECombatForgeInputToken::Back);
}

UENUM()
enum class ECombatForgeCommandElementMatchKind : uint8
{
	Press = 0,
	Held,
	Release
};

USTRUCT()
struct FCombatForgeCommandElement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Combat|Input")
	uint16 RequiredMask = 0;

	UPROPERTY(EditAnywhere, Category = "Combat|Input")
	uint16 AcceptedMask = 0;

	UPROPERTY(EditAnywhere, Category = "Combat|Input")
	ECombatForgeCommandElementMatchKind MatchKind = ECombatForgeCommandElementMatchKind::Press;

	UPROPERTY(EditAnywhere, Category = "Combat|Input")
	int32 MinHeldFrames = 0;

	UPROPERTY(EditAnywhere, Category = "Combat|Input")
	bool bStrictAfterPrevious = false;
};

USTRUCT()
struct FCombatForgeCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Combat|Input")
	int32 Id = 0;

	UPROPERTY(EditAnywhere, Category = "Combat|Input")
	FString CommandString;

	UPROPERTY(EditAnywhere, Category = "Combat|Input", meta = (ClampMin = 1))
	int32 InputWindowFrames = 20;

	UPROPERTY(EditAnywhere, Category = "Combat|Input")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, Category = "Combat|Input")
	TArray<FCombatForgeCommandElement> Elements;

	UPROPERTY(VisibleAnywhere, Category = "Combat|Input|Compile")
	int32 CompiledVersion = 0;

	UPROPERTY(VisibleAnywhere, Category = "Combat|Input|Compile")
	uint32 CompiledSourceHash = 0;
};

USTRUCT()
struct FCombatForgeCompileMessage
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Combat|Input")
	bool bIsError = true;

	UPROPERTY(VisibleAnywhere, Category = "Combat|Input")
	int32 CommandId = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category = "Combat|Input")
	FString Message;
};

USTRUCT(BlueprintType)
struct FCombatForgeInputActionBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Input")
	ECombatForgeInputButton Button = ECombatForgeInputButton::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Input")
	TObjectPtr<UInputAction> InputAction = nullptr;
};

USTRUCT(BlueprintType)
struct FCombatForgeInputRuntimeSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Input", meta = (ClampMin = 8, ClampMax = 256))
	int32 BufferCapacity = 64;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Input", meta = (ClampMin = 1, ClampMax = 500))
	int32 MaxBufferFrames = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Input", meta = (ClampMin = 1, ClampMax = 300))
	int32 DefaultInputWindowFrames = 20;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Input", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DirectionDeadZone = 0.35f;
};
