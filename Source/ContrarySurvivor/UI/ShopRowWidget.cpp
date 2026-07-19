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

void UShopRowWidget::SetupRow(const FText& InName, const FText& InPrice,
	const FText& InActionCaption, bool bActionEnabled)
{
	if (NameText)
	{
		if (PriceText)
		{
			NameText->SetText(InName);
		}
		else
		{
			// Кубика цены нет — дописываем цену к названию, чтобы игрок её всё равно видел.
			FFormatNamedArguments Args;
			Args.Add(TEXT("Name"), InName);
			Args.Add(TEXT("Price"), InPrice);
			NameText->SetText(FText::Format(NamePriceFormat, Args));
		}
	}
	if (PriceText)
	{
		PriceText->SetText(InPrice);
	}
	if (ActionText)
	{
		ActionText->SetText(InActionCaption);
	}
	if (ActionButton)
	{
		ActionButton->SetIsEnabled(bActionEnabled);
	}
	if (NoMoneyText)
	{
		// Текстом, а не только цветом погашенной кнопки (ADR-049, ревью издателя).
		NoMoneyText->SetText(NotEnoughMoneyText);
		NoMoneyText->SetVisibility(bActionEnabled
			? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}

void UShopRowWidget::HandleActionClicked()
{
	OnActionClicked.Broadcast(this);
}
