// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Chapter2/AnomalyFormationItem.h"
#include "ContrarySurvivor/Chapter2/Chapter2Rules.h"
#include "ContrarySurvivor/Chapter2/Chapter2Settings.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"

AAnomalyFormationItem::AAnomalyFormationItem()
{
	// Все образования — одна плитка с числом; больше, чем бывает в зоне (до 20), не соберётся.
	ItemName = Chapter2Ids::FormationItemKey;
	StackCount = 1;
	MaxStackCount = 99;
	bStoryItem = true;
}

bool AAnomalyFormationItem::IsFormationKey(const FString& Key)
{
	return Key.Equals(Chapter2Ids::FormationItemKey, ESearchCase::CaseSensitive);
}

void AAnomalyFormationItem::PostInitializeComponents()
{
	// Сначала база: строка таблицы предметов, если её когда-нибудь добавят с этим ключом.
	Super::PostInitializeComponents();
	ApplyFormationDefaults();
}

void AAnomalyFormationItem::ApplyFormationDefaults()
{
	if (!IsFormationKey(ItemName))
	{
		return;
	}
	const UChapter2Settings* S = UChapter2Settings::Get();
	// Название — всегда из настроек: правка текста доходит и до предмета из сохранения.
	ItemDisplayText = S->FormationNameText;
	if (ItemDescription.IsEmpty())
	{
		ItemDescription = S->FormationDescriptionText.ToString();
	}
	if (ItemIcon.IsNull())
	{
		if (!S->FormationIconTexture.IsNull())
		{
			ItemIcon = S->FormationIconTexture;
		}
		else
		{
			// Мягкая ссылка: значка может ещё не быть в проекте — плитка тогда с названием.
			const FString Package(UChapter2Settings::ExpectedFormationIconPath);
			ItemIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(FString::Printf(TEXT("%s.%s"), *Package, *FPackageName::GetShortName(Package))));
		}
	}
}
