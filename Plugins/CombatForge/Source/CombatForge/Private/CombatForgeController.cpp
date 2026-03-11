// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatForgeController.h"

#include "CombatForgeCharacter.h"
#include "Blueprint/UserWidget.h"
#include "Input/CombatForgeInputComponent.h"
#include "Input/CombatForgeInputLoggerWidget.h"
#include "Input/CombatForgeInputTextLogger.h"

void ACombatForgeController::BeginPlay()
{
	Super::BeginPlay();
	RefreshInputLoggerBinding();
}

void ACombatForgeController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	RefreshInputLoggerBinding();
}

ACombatForgeCharacter* ACombatForgeController::GetCombatForgeCharacterForLogging() const
{
	return Cast<ACombatForgeCharacter>(GetPawn());
}

void ACombatForgeController::EnsureInputLoggerWidget()
{
	if (InputLoggerWidget != nullptr || !IsLocalPlayerController())
	{
		return;
	}

	UClass* WidgetClass = InputLoggerWidgetClass != nullptr
		? InputLoggerWidgetClass.Get()
		: UCombatForgeInputLoggerWidget::StaticClass();
	InputLoggerWidget = CreateWidget<UCombatForgeInputLoggerWidget>(this, WidgetClass);
	if (InputLoggerWidget != nullptr)
	{
		if (InputTextLogger == nullptr)
		{
			InputTextLogger = NewObject<UCombatForgeInputTextLogger>(this);
		}
		if (InputTextLogger != nullptr)
		{
			InputTextLogger->SetOutputWidget(InputLoggerWidget);
		}

		InputLoggerWidget->AddToViewport(100);
		InputLoggerWidget->SetDesiredSizeInViewport(FVector2D(520.0f, 220.0f));
		InputLoggerWidget->SetPositionInViewport(FVector2D(32.0f, 32.0f), false);
		InputLoggerWidget->SetAlignmentInViewport(FVector2D(0.0f, 0.0f));
		InputLoggerWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void ACombatForgeController::RefreshInputLoggerBinding()
{
	ACombatForgeCharacter* CombatCharacter = GetCombatForgeCharacterForLogging();
	UCombatForgeInputComponent* CombatInput = CombatCharacter != nullptr ? CombatCharacter->GetCombatInput() : nullptr;
	const bool bShouldShowLogger = CombatInput != nullptr && CombatInput->bShowInputLogger;

	if (!bShouldShowLogger)
	{
		if (CombatInput != nullptr)
		{
			CombatInput->SetInputLogger(nullptr);
		}
		if (InputLoggerWidget != nullptr)
		{
			InputLoggerWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	EnsureInputLoggerWidget();
	if (InputLoggerWidget == nullptr)
	{
		return;
	}

	CombatInput->SetInputLogger(InputTextLogger);
	InputLoggerWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (InputTextLogger != nullptr)
	{
		InputTextLogger->ResetInputLog();
	}
}
