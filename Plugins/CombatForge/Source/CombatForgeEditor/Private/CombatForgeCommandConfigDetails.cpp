// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatForgeCommandConfigDetails.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IPropertyUtilities.h"
#include "Input/CombatForgeCommandConfig.h"
#include "SlateOptMacros.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<IDetailCustomization> FCombatForgeCommandConfigDetails::MakeInstance()
{
	return MakeShared<FCombatForgeCommandConfigDetails>();
}

void FCombatForgeCommandConfigDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	PropertyUtilities = DetailBuilder.GetPropertyUtilities();

	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() > 0)
	{
		CommandConfig = Cast<UCombatForgeCommandConfig>(Objects[0].Get());
	}

	IDetailCategoryBuilder& CompileCategory = DetailBuilder.EditCategory("Combat|Input|Compile");
	CompileCategory.AddCustomRow(FText::FromString(TEXT("CompileOffline")))
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("Offline Compile")))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(280.0f)
	[
		SNew(SButton)
		.Text(FText::FromString(TEXT("Compile")))
		.OnClicked(FOnClicked::CreateSP(this, &FCombatForgeCommandConfigDetails::OnCompileClicked))
	];

	CompileCategory.AddCustomRow(FText::FromString(TEXT("CompileStatus")))
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString(TEXT("Status")))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(280.0f)
	[
		SNew(STextBlock)
		.Text(this, &FCombatForgeCommandConfigDetails::GetCompileStatusText)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	];
}

FReply FCombatForgeCommandConfigDetails::OnCompileClicked() const
{
	if (UCombatForgeCommandConfig* Config = CommandConfig.Get())
	{
		Config->CompileOffline();
		if (PropertyUtilities.IsValid())
		{
			PropertyUtilities->ForceRefresh();
		}
	}

	return FReply::Handled();
}

FText FCombatForgeCommandConfigDetails::GetCompileStatusText() const
{
	const UCombatForgeCommandConfig* Config = CommandConfig.Get();
	if (Config == nullptr)
	{
		return FText::FromString(TEXT("No asset selected"));
	}

	if (Config->IsCompileCurrent())
	{
		return FText::FromString(FString::Printf(TEXT("Compiled (%d command(s))"), Config->Commands.Num()));
	}

	if (!Config->CompileErrorMessage.IsEmpty())
	{
		return FText::FromString(Config->CompileErrorMessage);
	}

	return FText::FromString(TEXT("Not compiled or stale"));
}
