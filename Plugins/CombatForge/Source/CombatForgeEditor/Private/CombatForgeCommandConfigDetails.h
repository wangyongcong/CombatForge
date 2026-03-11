// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class UCombatForgeCommandConfig;
class IPropertyUtilities;

class FCombatForgeCommandConfigDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	FReply OnCompileClicked() const;
	FText GetCompileStatusText() const;

private:
	TWeakObjectPtr<UCombatForgeCommandConfig> CommandConfig;
	TSharedPtr<IPropertyUtilities> PropertyUtilities;
};
