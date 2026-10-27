// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Chapter2/Chapter2Settings.h"
#include "ContrarySurvivor/Chapter2/Chapter2Rules.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"

UTexture2D* UChapter2Settings::ResolveTexture(const TSoftObjectPtr<UTexture2D>& Field, const TCHAR* ExpectedPackage)
{
	if (!Field.IsNull())
	{
		if (UTexture2D* Loaded = Field.LoadSynchronous())
		{
			return Loaded;
		}
	}

	// Поле пустое (картинку ещё не сделали или не вписали): берём файл по ожидаемому пути,
	// но только если он правда лежит в проекте — иначе движок писал бы в журнал жалобу на
	// каждую попытку загрузки.
	if (!ExpectedPackage || !FPackageName::DoesPackageExist(ExpectedPackage))
	{
		return nullptr;
	}
	const FString ObjectPath = FString::Printf(TEXT("%s.%s"), ExpectedPackage, *FPackageName::GetShortName(ExpectedPackage));
	return LoadObject<UTexture2D>(nullptr, *ObjectPath);
}

const FText& UChapter2Settings::StoryItemLockedTextFor(const FString& ItemKey)
{
	const UChapter2Settings* S = Get();
	return ItemKey.Equals(Chapter2Ids::FormationItemKey, ESearchCase::CaseSensitive) ? S->FormationLockedText : S->StoryItemLockedText;
}
