// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/InventoryRowWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA

void UInventoryRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (UseButton)
	{
		UseButton->OnClicked.AddDynamic(this, &UInventoryRowWidget::HandleUseClicked);
	}
	if (DropButton)
	{
		DropButton->OnClicked.AddDynamic(this, &UInventoryRowWidget::HandleDropClicked);
	}
	else
	{
		UE_LOG(LogQA, Warning,
			TEXT("InventoryRowWidget '%s': кубик DropButton не найден в WBP — выброс из строки недоступен"),
			*GetName());
	}
}

void UInventoryRowWidget::SetupRow(const FString& InName, const FString& InUseCaption)
{
	if (NameText)
	{
		NameText->SetText(FText::FromString(InName));
	}
	if (UseText)
	{
		UseText->SetText(FText::FromString(InUseCaption));
	}
	if (UseButton)
	{
		// Пустая подпись = предмет из строки не применяется (квест-предметы и т.п.) — кнопку прячем.
		UseButton->SetVisibility(InUseCaption.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
}

void UInventoryRowWidget::HandleUseClicked()
{
	OnUseClicked.Broadcast(this);
}

void UInventoryRowWidget::HandleDropClicked()
{
	OnDropClicked.Broadcast(this);
}
