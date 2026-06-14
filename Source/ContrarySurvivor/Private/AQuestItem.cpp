// Fill out your copyright notice in the Description page of Project Settings.

#include "AQuestItem.h"

AQuestItem::AQuestItem()
{
	// Категория Quest: предмет не теряется при смерти и не используется как расходник.
	ItemCategory = EItemCategory::Quest;
}

void AQuestItem::Use()
{
	// Квест-предмет нельзя применить из рюкзака — намеренно пусто.
}
