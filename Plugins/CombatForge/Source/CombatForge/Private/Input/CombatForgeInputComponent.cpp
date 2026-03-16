// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/CombatForgeInputComponent.h"

#include "CombatForgeInputUtility.h"
#include "EnhancedInputComponent.h"

UCombatForgeInputComponent::UCombatForgeInputComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCombatForgeInputComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UCombatForgeInputComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                               FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UCombatForgeInputComponent::SetInputLogger(ICombatForgeInputLogger* InInputLogger)
{
	if (InInputLogger == nullptr)
	{
		InputLogger = nullptr;
		DebugSequenceCounter = 0;
		return;
	}

	InputLogger.SetInterface(InInputLogger);
	ResetInputLogger();
}

// void UCombatForgeInputComponent::ResetCurrentInputEvents()
// {
// }

void UCombatForgeInputComponent::EmitInputEvent(FGameplayTag EventTag) const
{
	OnInputEvent.Broadcast(EventTag);
}

// void UCombatForgeInputComponent::BroadcastInputEvents(const TArray<FCombatForgeInputEvent>& InputEvents) const
// {
// 	if (!OnInputEvent.IsBound())
// 	{
// 		return;
// 	}
// 	for (const FCombatForgeInputEvent& InputEvent : InputEvents)
// 	{
// 		OnInputEvent.Broadcast(InputEvent);
// 	}
// }

void UCombatForgeInputComponent::LogInputCommands(uint16 StateBits, const TArray<const FCombatForgeCommand*>& Commands)
{
	if (InputLogger.GetInterface() != nullptr && Commands.Num() > 0)
	{
		StateBits = CombatForgeInput::NormalizeStateBits(StateBits);
		InputLogger->AddCommandEntry(NextDebugSequence(), StateBits, Commands);
	}
}

void UCombatForgeInputComponent::LogInputEvents(const TArray<FGameplayTag>& InputEvents)
{
	if (InputLogger.GetInterface() != nullptr && InputEvents.Num() > 0)
	{
		InputLogger->AddEventEntry(NextDebugSequence(), InputEvents);
	}
}

void UCombatForgeInputComponent::ResetInputLogger()
{
	DebugSequenceCounter = 0;
	if (InputLogger.GetInterface() != nullptr)
	{
		InputLogger->Reset();
	}
}

int32 UCombatForgeInputComponent::NextDebugSequence()
{
	return ++DebugSequenceCounter;
}

