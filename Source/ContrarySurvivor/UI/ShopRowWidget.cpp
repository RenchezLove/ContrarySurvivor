// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/ShopRowWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA

void UShopRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (ActionButton)
	{
		ActionButton->OnClicked.AddDynamic(this, &UShopRowWidget::HandleActionClicked);
	}
	else
	{
		// BindWidgetOptional: Ринат мог ещё не положить кнопку в WBP_ShopRow — строка
		// покажет текст, но действие будет недоступно. Предупреждаем, не крашим (ADR-048).
		UE_LOG(LogQA, Warning,
			TEXT("ShopRowWidget '%s': кубик ActionButton не найден в WBP — кнопка Buy/Sell работать не будет"),
			*GetName());
	}
}

void UShopRowWidget::SetupRow(const FString& InName, const FString& InPrice,
	const FString& InActionCaption, bool bActionEnabled)
{
	if (NameText)
	{
		// Кубика цены нет — дописываем цену к имени, чтобы игрок её всё равно видел.
		NameText->SetText(FText::FromString(
			PriceText ? InName : FString::Printf(TEXT("%s  -  %s"), *InName, *InPrice)));
	}
	if (PriceText)
	{
		PriceText->SetText(FText::FromString(InPrice));
	}
	if (ActionText)
	{
		ActionText->SetText(FText::FromString(InActionCaption));
	}
	if (ActionButton)
	{
		ActionButton->SetIsEnabled(bActionEnabled);
	}
}

void UShopRowWidget::HandleActionClicked()
{
	OnActionClicked.Broadcast(this);
}
