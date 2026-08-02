// Fill out your copyright notice in the Description page of Project Settings.

#include "AQuestItem.h"

AQuestItem::AQuestItem()
{
	// Категория Quest: предмет не теряется при смерти и не используется как расходник.
	ItemCategory = EItemCategory::Quest;

	// Build 1.2.1 (ТЗ Г): «Шкура волка» стакается по механизму патронов, лимит как у
	// патронов (999). Уникальным предметам (ноутбук) стак не мешает: второго экземпляра
	// в мире нет, а различает стаки служебный ключ ItemName (CanStackWith).
	StackCount = 1;
	MaxStackCount = 999;
}

void AQuestItem::Use()
{
	// Квест-предмет нельзя применить из рюкзака — намеренно пусто.
}

TSoftObjectPtr<UTexture2D> AQuestItem::GetItemIcon() const
{
	if (!ItemIcon.IsNull())
	{
		return ItemIcon; // явно заданная иконка главнее вычисленной
	}
	// Ключи — ДОСЛОВНО значения RequiredItemName квестов старосты (ElderNPC.cpp) и
	// спавнеров (WolfCharacter/MasterEnemyBase). По контракту ADR-050 не меняются.
	if (ItemName == TEXT("Шкура волка"))
	{
		return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
			TEXT("/Game/UI/Icons/Items/T_Item_WolfHide.T_Item_WolfHide")));
	}
	if (ItemName == TEXT("Ноутбук"))
	{
		return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
			TEXT("/Game/UI/Icons/Items/T_Item_Laptop.T_Item_Laptop")));
	}
	return TSoftObjectPtr<UTexture2D>(); // незнакомый ключ — без иконки
}
