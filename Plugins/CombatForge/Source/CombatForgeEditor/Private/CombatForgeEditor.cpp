// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatForgeEditor.h"

#include "CombatForgeCommandConfigDetails.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

IMPLEMENT_MODULE(FCombatForgeEditorModule, CombatForgeEditor)

void FCombatForgeEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditorModule.RegisterCustomClassLayout(
		"CombatForgeCommandConfig",
		FOnGetDetailCustomizationInstance::CreateStatic(&FCombatForgeCommandConfigDetails::MakeInstance));
	PropertyEditorModule.NotifyCustomizationModuleChanged();
}

void FCombatForgeEditorModule::ShutdownModule()
{
	if (!FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		return;
	}

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditorModule.UnregisterCustomClassLayout("CombatForgeCommandConfig");
	PropertyEditorModule.NotifyCustomizationModuleChanged();
}
