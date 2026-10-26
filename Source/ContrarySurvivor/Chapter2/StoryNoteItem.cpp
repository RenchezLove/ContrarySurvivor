// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Chapter2/StoryNoteItem.h"
#include "ContrarySurvivor/Chapter2/Chapter2Settings.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"

namespace StoryNotes
{
	bool Describe(FName NoteId, FStoryNoteView& OutView)
	{
		const UChapter2Settings* S = UChapter2Settings::Get();
		OutView = FStoryNoteView();
		OutView.Id = NoteId;
		if (NoteId == StoryNoteIds::MercenaryNote)
		{
			OutView.Title = S->MercenaryNoteTitleText;
			OutView.Body = S->MercenaryNoteBodyText;
			OutView.Image = UChapter2Settings::ResolveTexture(S->MercenaryNoteTexture, UChapter2Settings::ExpectedMercenaryNotePath);
			OutView.FallbackAspect = S->NoteFallbackAspect;
			OutView.bPaper = (OutView.Image == nullptr);
			return true;
		}
		if (NoteId == StoryNoteIds::VeraPhoto)
		{
			OutView.Title = S->VeraPhotoNoteTitleText;
			OutView.Image = UChapter2Settings::ResolveTexture(S->PhotoTexture, UChapter2Settings::ExpectedPhotoPath);
			OutView.FallbackAspect = 1.5f;
			return true;
		}
		return false;
	}
}

AStoryNoteItem::AStoryNoteItem()
{
	StackCount = 1;
	MaxStackCount = 1;
	bStoryItem = true;
}

FString AStoryNoteItem::MakeItemKey(FName InNoteId)
{
	return FString::Printf(TEXT("StoryNote_%s"), *InNoteId.ToString());
}

void AStoryNoteItem::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ApplyNoteDefaults();
}

void AStoryNoteItem::ApplyNoteDefaults()
{
	if (NoteId.IsNone())
	{
		return;
	}
	if (ItemName.IsEmpty())
	{
		ItemName = MakeItemKey(NoteId);
	}
	const UChapter2Settings* S = UChapter2Settings::Get();
	if (NoteId == StoryNoteIds::MercenaryNote)
	{
		ItemDisplayText = S->MercenaryNoteTitleText;
		// Значок плитки в окне обыска — сама картинка записки, если она уже есть в проекте.
		if (ItemIcon.IsNull())
		{
			if (!S->MercenaryNoteTexture.IsNull())
			{
				ItemIcon = S->MercenaryNoteTexture;
			}
			else if (FPackageName::DoesPackageExist(UChapter2Settings::ExpectedMercenaryNotePath))
			{
				const FString Package(UChapter2Settings::ExpectedMercenaryNotePath);
				ItemIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(FString::Printf(TEXT("%s.%s"), *Package, *FPackageName::GetShortName(Package))));
			}
		}
	}
	else if (NoteId == StoryNoteIds::VeraPhoto)
	{
		ItemDisplayText = S->VeraPhotoNoteTitleText;
	}
}
