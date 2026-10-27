// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Chapter2Component.generated.h"

class APlayerCharacter;
class UContrarySaveGame;
class UQuestComponent;
struct FQuest;

/**
 * Глава 2 «Вера», задание «Образцы из аномалий» — состояние героя (25.10.2026).
 * Живёт на герое рядом с журналом заданий и пишется в то же сохранение:
 *  - дыхательный аппарат (выдаёт Вера при взятии задания), надет ли он, сколько воздуха;
 *  - урон в отравленной низине: без аппарата и с пустым баллоном — каждую секунду;
 *  - заправка баллона на базе учёных;
 *  - детектор-локатор (правка Рината 25.10 вечер): левый стик превращается в локатор, пока
 *    задание идёт и герой в низине или рядом; писк тем чаще, чем ближе образец; через время
 *    без находки — стрелка на ближайший образец;
 *  - какие образцы подобраны, побеждён ли наёмник, показан ли конец главы;
 *  - фотография из ноутбука и экран конца главы (что показывать на экране прямо сейчас).
 *
 * Сюжет вокруг задания (правки Рината 25.10 вечер) держит SyncStoryState, раз в тик:
 *  - ноутбук сдан учёному, а задание Веры не взято — цель «Поговорить с Верой» [230] в журнале
 *    (с меткой на Вере на экране и на карте смартфона); взято — цель уходит;
 *  - пока задание идёт, в сумке лежат «Баллон с воздухом» и «Детектор» (сюжетные предметы);
 *    их нет в сохранении, записанном раньше, — они появляются сами; задание сдано — Вера
 *    забирает снаряжение обратно (выбросить его нельзя, иначе оно лежало бы в сумке вечно).
 *
 * Все правила — в Step(Dt, Env): автопроверка подаёт своё окружение и тикает без живого мира.
 * Числа и надписи — UChapter2Settings.
 */
UCLASS(ClassGroup = (Custom))
class CONTRARYSURVIVOR_API UChapter2Component : public UActorComponent
{
	GENERATED_BODY()

public:
	UChapter2Component();

	/** Где герой относительно низины, базы и образцов (собирается из мира раз в тик). */
	struct FEnv
	{
		bool bInZone = false;          // внутри отравленной низины
		bool bNearZone = false;        // у края низины (кнопка аппарата)
		bool bInRefill = false;        // на базе учёных (заправка баллона)
		float NearestSampleDistance = -1.0f; // до ближайшего не подобранного образца; < 0 — нет
		float ZoneDistance = -1.0f;    // до края ближайшей низины (0 — внутри); < 0 — низины нет
		FVector HeroLocation = FVector::ZeroVector;
		TArray<FVector> SampleLocations; // не подобранные образцы, которые сейчас видны
		// Свои числа низины, в которой (или у которой) стоит герой (настройки экземпляра на уровне);
		// меньше нуля — как в настройках проекта.
		float DamageNoMaskPerSecond = -1.0f;
		float DamageNoAirPerSecond = -1.0f;
		float DetectorRange = -1.0f;
	};

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	FEnv GatherEnv() const;
	void Step(float Dt, const FEnv& Env);

	// --- Сюжет вокруг задания: цель «Поговорить с Верой» и снаряжение в сумке ---

	void SyncStoryState();

	// Задание-цель «Поговорить с Верой» [230]: ведёт к Вере на экране и на карте смартфона.
	static FQuest MakeTalkToVeraQuest();

	// Сколько предметов снаряжения учёных лежит в сумке (0..2).
	int32 CountGearInBag() const;

	// Аномальные образования в сумке (правка Рината 27.10.2026: «Сами аномальные образования
	// сделай предметом. Этот предмет попадает в инвентарь, когда игрок берёт его»): пока задание
	// идёт или готово к сдаче — столько, сколько собрано (счётчик цели [216]); не взято или
	// сдано Вере — ни одного. SyncFormationsInBag приводит сумку к этому числу (кладёт
	// недостающие, убирает лишние) — так и старое сохранение получает предметы само.
	int32 GetWantedFormationsInBag() const;
	int32 CountFormationsInBag() const;
	void SyncFormationsInBag();

	// --- Аппарат и воздух ---

	void GiveApparatus();
	bool HasApparatus() const { return bApparatusGiven; }
	bool IsMaskOn() const { return bMaskOn; }
	float GetAirSeconds() const { return AirSeconds; }
	float GetAirFraction() const;

	// Есть ли сейчас кнопка аппарата: надеть — у края или в низине; снять — вне низины.
	bool CanToggleMask() const;
	FText GetMaskActionText() const;
	bool ToggleMask();

	// Заправить баллон. bAnnounce — показать [224], если баллон был не полон.
	void RefillAir(bool bAnnounce);

	// Есть ли на уровне хоть одна зона заправки (иначе баллон заправляет Вера в разговоре).
	bool HasRefillZoneInWorld() const;

	// --- Образцы ---

	bool CollectSample(FName SampleId);
	bool IsSamplePicked(FName SampleId) const { return PickedSamples.Contains(SampleId); }
	int32 GetPickedCount() const { return PickedSamples.Num(); }

	// Ход задания по журналу героя.
	const FQuest* FindSamplesQuest() const;
	bool IsSamplesQuestActive() const;
	int32 GetSamplesDone() const;
	int32 GetSamplesTotal() const;

	// Показать образцы, пока задание идёт, и спрятать подобранные.
	void RefreshSampleActors();

	// Сколько образцов ждут низины уровня (сумма настройки «Сколько образцов» всех низин).
	// 0 — низин на уровне нет (тогда число берётся из самого задания Веры).
	static int32 GetZoneSampleTotal(const UWorld* World);

	// Цель задания [216] «({Done} из {Total})» берёт число из низины (правка Рината 26.10):
	// задание ещё не сдано и не собрано целиком — его цель приводится к числу образцов низин.
	void SyncSamplesTarget();

	// --- Заметки смартфона (правка Рината 26.10.2026) ---

	// Положить заметку в список (StoryNoteIds). bAnnounce — сообщение [245] о находке.
	// false — такая заметка уже есть.
	bool AddNote(FName NoteId, bool bAnnounce);
	bool HasNote(FName NoteId) const { return Notes.Contains(NoteId); }
	// В порядке находки (первая найденная — первая в списке).
	const TArray<FName>& GetNotes() const { return Notes; }

	// --- Наёмник ---

	bool ShouldSpawnMercenary() const;
	bool IsMercenaryDefeated() const { return bMercenaryDefeated; }
	void NotifyMercenaryDefeated();

	// --- Детектор-локатор ---

	// Локатор на месте левого стика (и писк детектора).
	bool IsDetectorShown() const { return bDetectorShown; }
	float GetSignalStrength() const { return SignalStrength; }
	int32 GetBeepCount() const { return BeepCount; }

	// Сколько секунд локатор на экране без подобранного образца и видна ли уже стрелка.
	float GetLocatorSecondsWithoutPickup() const { return LocatorSecondsWithoutPickup; }
	bool IsHintArrowShown() const;

	// Ближайший не подобранный образец (по последнему окружению). false — образцов нет.
	bool GetNearestSampleLocation(FVector& OutLocation) const;

	// --- Фотография и конец главы ---

	void SetPhotoShown(bool bShown) { bPhotoShown = bShown; }

	// Герой увидел фотографию из ноутбука в разговоре с Верой — она уходит в «Заметки» [246].
	void NotePhotoSeen();
	bool IsPhotoShown() const { return bPhotoShown; }

	// Последний разговор с Верой закрыт: экран «Конец первой главы» и запись в сохранение.
	void StartChapterEnd();
	bool IsChapterEndShown() const { return bChapterEndShown; }
	float GetChapterEndCardRemaining() const { return ChapterEndCardRemaining; }

	const FEnv& GetLastEnv() const { return LastEnv; }

	// --- Сохранение ---

	void WriteToSave(UContrarySaveGame* Save) const;
	void RestoreFromSave(const UContrarySaveGame* Save);
	void ResetForNewGame();

private:
	APlayerCharacter* GetPlayer() const;
	UQuestComponent* GetQuests() const;
	void ShowMessage(const FText& Text) const;
	void PlayBeep() const;
	void FinishChapterEndCard();

	// Положить в сумку недостающее снаряжение (bWant) либо забрать его оттуда.
	void SyncGearInBag(bool bWant);

	// Встречи с наёмником на уровне — к исходному состоянию (новая игра, точки сюжета).
	void ResetMercenaryEncounters();

	// Сохраняемое.
	bool bApparatusGiven = false;
	bool bMaskOn = false;
	float AirSeconds = 0.0f;
	TArray<FName> PickedSamples;
	bool bMercenaryDefeated = false;
	bool bChapterEndShown = false;
	TArray<FName> Notes;

	// Живое (в сохранение не пишется).
	FEnv LastEnv;
	bool bWasInZone = false;
	bool bWasInRefill = false;
	bool bLowAirWarned = false;
	float DamageAccum = 0.0f;
	float BeepTimer = 0.0f;
	float SignalStrength = 0.0f;
	bool bDetectorShown = false;
	int32 BeepCount = 0;
	float LocatorSecondsWithoutPickup = 0.0f;
	bool bPhotoShown = false;
	float ChapterEndCardRemaining = 0.0f;
};
