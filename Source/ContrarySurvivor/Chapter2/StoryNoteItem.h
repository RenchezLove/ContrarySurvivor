// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AQuestItem.h"
#include "StoryNoteItem.generated.h"

class UTexture2D;

/** Заметки смартфона (правка Рината 26.10.2026): какие бывают и что в них показывать. */
namespace StoryNoteIds
{
	// Записка наёмника Куратора [238]/[239]: лежит в добыче побеждённого наёмника.
	inline const FName MercenaryNote(TEXT("MercenaryNote"));

	// Фото из ноутбука [246]: появляется после показа фотографии в разговоре с Верой [250]–[252].
	inline const FName VeraPhoto(TEXT("VeraPhoto"));
}

/** Что показать во вкладке «Заметки» для одной заметки. */
struct FStoryNoteView
{
	FName Id = NAME_None;
	FText Title;                  // строка списка и заголовок
	FText Body;                   // текст поверх картинки (пусто — только картинка)
	UTexture2D* Image = nullptr;  // картинка заметки (нет файла — null, игра не падает)
	float FallbackAspect = 1.0f;  // ширина к высоте, пока размер картинки неизвестен
	bool bPaper = false;          // нет картинки, а текст есть — рисовать светлую бумагу
};

namespace StoryNotes
{
	// Описание заметки по её имени. false — такой заметки игра не знает (старое сохранение
	// из будущей версии) — в списке её нет.
	CONTRARYSURVIVOR_API bool Describe(FName NoteId, FStoryNoteView& OutView);
}

/**
 * Записка-предмет в добыче (правка Рината 26.10.2026: «Добавь в инвентарь наемника куратора
 * какую-нибудь записку… Саму записку можно будет почитать открыв смартфон»).
 *
 * Лежит в окне обыска тела как обычный предмет, но в сумку не попадает: при подборе она
 * сразу уходит во вкладку «Заметки» смартфона (сообщение [245]) — так её нельзя ни продать,
 * ни выбросить, ни потерять. Если всё же окажется в сумке (будущие пути), она сюжетная:
 * продать и выбросить нельзя.
 */
UCLASS()
class CONTRARYSURVIVOR_API AStoryNoteItem : public AQuestItem
{
	GENERATED_BODY()

public:
	AStoryNoteItem();

	// Какая это заметка (StoryNoteIds). Ставить до появления в мире.
	FName NoteId = NAME_None;

	// Название и картинка по имени заметки (пустые поля заполняются).
	void ApplyNoteDefaults();

	// Служебный ключ предмета для этой заметки (уезжает в сохранение, если предмет в сумке).
	static FString MakeItemKey(FName InNoteId);

protected:
	virtual void PostInitializeComponents() override;
};
