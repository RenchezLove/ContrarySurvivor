// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Chapter2/ScientistGearItem.h"
#include "ContrarySurvivor/Chapter2/Chapter2Rules.h"
#include "ContrarySurvivor/Chapter2/Chapter2Settings.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"

AScientistGearItem::AScientistGearItem()
{
	// Один баллон и один детектор: не стакаются, при смерти не теряются (категория задания).
	StackCount = 1;
	MaxStackCount = 1;
	bStoryItem = true;
}

void AScientistGearItem::PostInitializeComponents()
{
	// Сначала база: строка таблицы предметов, если её когда-нибудь добавят с этим ключом.
	Super::PostInitializeComponents();
	ApplyGearDefaults();
}

bool AScientistGearItem::IsGearKey(const FString& Key)
{
	return Key.Equals(Chapter2Ids::AirTankKey, ESearchCase::CaseSensitive)
		|| Key.Equals(Chapter2Ids::DetectorKey, ESearchCase::CaseSensitive);
}

UTexture2D* AScientistGearItem::ResolveGearIcon(const FString& Key)
{
	const UChapter2Settings* S = UChapter2Settings::Get();
	if (Key.Equals(Chapter2Ids::AirTankKey, ESearchCase::CaseSensitive))
	{
		return UChapter2Settings::ResolveTexture(S->AirIconTexture, UChapter2Settings::ExpectedAirIconPath);
	}
	if (Key.Equals(Chapter2Ids::DetectorKey, ESearchCase::CaseSensitive))
	{
		return UChapter2Settings::ResolveTexture(S->DetectorIconTexture, UChapter2Settings::ExpectedDetectorIconPath);
	}
	return nullptr;
}

void AScientistGearItem::ApplyGearDefaults()
{
	const UChapter2Settings* S = UChapter2Settings::Get();
	const bool bAir = ItemName.Equals(Chapter2Ids::AirTankKey, ESearchCase::CaseSensitive);
	const bool bDetector = ItemName.Equals(Chapter2Ids::DetectorKey, ESearchCase::CaseSensitive);
	if (!bAir && !bDetector)
	{
		return;
	}
	// Название — всегда из настроек: правка текста в настройках доходит и до предмета из
	// сохранения, записанного раньше (сохранение помнит и название).
	ItemDisplayText = bAir ? S->AirTankNameText : S->DetectorNameText;
	if (ItemDescription.IsEmpty())
	{
		ItemDescription = (bAir ? S->AirTankDescriptionText : S->DetectorDescriptionText).ToString();
	}
	if (ItemIcon.IsNull())
	{
		const TSoftObjectPtr<UTexture2D>& Field = bAir ? S->AirIconTexture : S->DetectorIconTexture;
		if (!Field.IsNull())
		{
			ItemIcon = Field;
		}
		else
		{
			const TCHAR* Expected = bAir ? UChapter2Settings::ExpectedAirIconPath : UChapter2Settings::ExpectedDetectorIconPath;
			const FString Package(Expected);
			ItemIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(FString::Printf(TEXT("%s.%s"), *Package, *FPackageName::GetShortName(Package))));
		}
	}
}
