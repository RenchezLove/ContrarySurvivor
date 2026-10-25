// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ContrarySurvivor/Actors/TalkerNPC.h"
#include "VeraNPC.generated.h"

/** На каком месте разговора с Верой сейчас окно (внутри одного разговора). */
UENUM()
enum class EVeraTalkStage : uint8
{
	Idle,        // ноутбук ещё не сдан учёным — Вера занята
	Deal,        // [212] предлагает сделку, кнопка [213]
	Offer,       // [214] задание, кнопка [215]
	Active,      // [227] пока герой ищет образцы
	WaitMerc,    // образцы собраны, а наёмник на дороге ещё жив (черновая реплика)
	Samples,     // [228] образцы принесены, кнопка [229]
	LastFile,    // [250] и фотография на экране, кнопка [251]
	ChapterEnd,  // [252] — после закрытия экран конца главы
	After        // глава пройдена, следующие разговоры
};

/**
 * Вера — руководитель лагеря учёных (глава 2, утверждено Ринатом 23.10.2026, поправка 25.10:
 * «игрок приносит ноут, вера говорит, что покопается, но в обмен на помощь (квест "образцы
 * аномалий")… В конце главы в ноутбуке находится фотография героя рядом с Верой»).
 *
 * Говорит только после того, как ноутбук сдан квестовому учёному ([037], [038]). Задание
 * «Образцы из аномалий» — обычное задание журнала героя; при взятии Вера выдаёт дыхательный
 * аппарат. Сдача: [228] → ответ героя [229] → [250] с фотографией → ответ [251] → [252] →
 * экран «Конец второй главы» ([253]).
 *
 * Все реплики — дословно из РЕПЛИКИ.txt, в полях чертежа (правятся без кода).
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AVeraNPC : public ATalkerNPC
{
	GENERATED_BODY()

public:
	AVeraNPC();

	virtual void BuildTalkView(APlayerCharacter* Player, FNpcTalkView& OutView) const override;
	virtual bool HandleTalkPrimary(APlayerCharacter* Player) override;
	virtual void OnTalkOpened(APlayerCharacter* Player) override;
	virtual void OnTalkClosed(APlayerCharacter* Player) override;

	// С какого места начинается разговор при таком положении дел (чистое правило для проверок).
	static EVeraTalkStage ComputeOpeningStage(bool bLaptopTurnedIn, const FQuest* InSamplesQuest,
		bool bMercenaryDefeated, bool bChapterEndShown);

	EVeraTalkStage GetStage() const { return Stage; }
	const FQuest& GetSamplesQuest() const { return SamplesQuest; }

	// Сдано ли задание учёным, после которого с героем говорит Вера.
	static bool IsLaptopTurnedIn(const APlayerCharacter* Player);

protected:
	// --- Задание «Образцы из аномалий» ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Вера", meta = (DisplayPriority = "1",
		DisplayName = "Задание «Образцы из аномалий»"))
	FQuest SamplesQuest;

	// --- Реплики (дословно из РЕПЛИКИ.txt) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Вера", meta = (DisplayPriority = "2", MultiLine = "true",
		DisplayName = "[212] Первая встреча: предлагает сделку"))
	FText DealText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Вера", meta = (DisplayPriority = "3",
		DisplayName = "[213] Ответ героя: согласиться"))
	FText DealReplyText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Вера", meta = (DisplayPriority = "4", MultiLine = "true",
		DisplayName = "[227] Пока герой ищет образцы"))
	FText ActiveText;

	// Образцы собраны, но наёмник на дороге ещё жив (герой погиб в бою и вернулся в лагерь).
	// ЧЕРНОВИК — такой реплики в РЕПЛИКИ.txt нет. Пусто — Вера принимает образцы сразу.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Вера", meta = (DisplayPriority = "5", MultiLine = "true",
		DisplayName = "Реплика: образцы собраны, наёмник ещё жив (черновик)"))
	FText WaitMercenaryText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Вера", meta = (DisplayPriority = "6", MultiLine = "true",
		DisplayName = "[228] Герой принёс образцы"))
	FText SamplesText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Вера", meta = (DisplayPriority = "7", MultiLine = "true",
		DisplayName = "[250] Последний файл"))
	FText LastFileText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Вера", meta = (DisplayPriority = "8",
		DisplayName = "[251] Ответ героя"))
	FText LastFileReplyText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Вера", meta = (DisplayPriority = "9", MultiLine = "true",
		DisplayName = "[252] Конец главы"))
	FText ChapterEndText;

	// Следующие разговоры после конца главы (конец [252]).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Talker|Вера", meta = (DisplayPriority = "10", MultiLine = "true",
		DisplayName = "Реплика: после конца главы"))
	FText AfterText;

private:
	EVeraTalkStage Stage = EVeraTalkStage::Idle;
};
