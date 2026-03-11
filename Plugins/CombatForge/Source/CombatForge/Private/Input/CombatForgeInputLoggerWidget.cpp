// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/CombatForgeInputLoggerWidget.h"

#include "Engine/Engine.h"

void UCombatForgeInputLoggerWidget::ResetLog_Implementation()
{
	if (GEngine != nullptr)
	{
		GEngine->ClearOnScreenDebugMessages();
	}
}

void UCombatForgeInputLoggerWidget::AppendLogLine_Implementation(const FString& Line)
{
	if (GEngine != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			5.0f,
			FColor::Green,
			Line);
	}
}
