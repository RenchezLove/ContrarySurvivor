// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ContrarySurvivor/Actors/TalkerNPC.h"
#include "MercenaryEncounter.generated.h"

class AEnemyCharacter;
class UMaterialInterface;

/**
 * Встреча с наёмником Куратора (поправка Рината 25.10.2026: «По дороге назад после квеста
 * "образцы аномалий" на героя нападает наемник куратора и называет его по имени»).
 *
 * Ставится на уровень на дороге из низины к лагерю учёных. Когда все образцы собраны и герой
 * вышел из низины, здесь появляется наёмник — бандит со своими здоровьем, уроном и окраской
 * (решение лида 23.10: «модель бандита с другой окраской снаряжения, сильнее обычного
 * бандита»). Пока они не поговорили, наёмник стоит и ждёт; герой подходит — открывается
 * разговор [241]–[244], после него (или если герой выстрелил первым) начинается бой.
 *
 * Сама точка — невидимый собеседник: разговор идёт в том же окне, что с остальными
 * персонажами. Подсказки «поговорить» у неё нет, разговор открывается сам.
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AMercenaryEncounter : public ATalkerNPC
{
	GENERATED_BODY()

public:
	AMercenaryEncounter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BuildTalkView(APlayerCharacter* Player, FNpcTalkView& OutView) const override;
	virtual bool HandleTalkPrimary(APlayerCharacter* Player) override;
	virtual void OnTalkOpened(APlayerCharacter* Player) override;
	virtual void OnTalkClosed(APlayerCharacter* Player) override;

	// Один шаг встречи (зовёт Tick; публичный — его же зовёт автопроверка).
	void UpdateEncounter(APlayerCharacter* Player);

	AEnemyCharacter* GetMercenary() const { return Mercenary.Get(); }
	bool IsTalkDone() const { return bTalkDone; }
	bool IsFightStarted() const { return bFightStarted; }

	// Начать бой: наёмник оживает (его мозги до этого стоят).
	void StartFight();

	// Класс наёмника на время автопроверки (иначе берётся чертёж бандита из поля).
	void SetMercenaryClassForTest(TSubclassOf<AEnemyCharacter> InClass) { TestClass = InClass; }

protected:
	// Чертёж бандита, из которого делается наёмник.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Наёмник", meta = (DisplayPriority = "1",
		DisplayName = "Чертёж наёмника (бандит)"))
	TSoftClassPtr<AEnemyCharacter> MercenaryClass;

	// Обычный бандит — 80 здоровья.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Наёмник", meta = (DisplayPriority = "2", ClampMin = "1.0",
		DisplayName = "Здоровье наёмника"))
	float MercenaryHealth = 200.0f;

	// Обычный бандит — 10 вблизи и 12 выстрелом.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Наёмник", meta = (DisplayPriority = "3", ClampMin = "0.0",
		DisplayName = "Урон удара вблизи"))
	float MercenaryMeleeDamage = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Наёмник", meta = (DisplayPriority = "4", ClampMin = "0.0",
		DisplayName = "Урон выстрела"))
	float MercenaryRangedDamage = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Наёмник", meta = (DisplayPriority = "5", ClampMin = "0.0", ClampMax = "1.0",
		DisplayName = "Шанс попадания"))
	float MercenaryHitChance = 0.8f;

	// Другая окраска снаряжения: материал на всех частях тела. Пусто — как у бандита.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Наёмник", meta = (DisplayPriority = "6",
		DisplayName = "Материал окраски наёмника"))
	TObjectPtr<UMaterialInterface> MercenaryOutfitMaterial;

	// Герой ближе этого (см) — открывается разговор.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Наёмник", meta = (DisplayPriority = "7", ClampMin = "100.0",
		DisplayName = "Расстояние начала разговора (см)"))
	float TalkDistance = 700.0f;

	// --- Реплики (дословно из РЕПЛИКИ.txt) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Наёмник", meta = (DisplayPriority = "8", MultiLine = "true",
		DisplayName = "[241] Встреча на дороге"))
	FText MeetText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Наёмник", meta = (DisplayPriority = "9",
		DisplayName = "[242] Ответ героя"))
	FText MeetReplyText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Наёмник", meta = (DisplayPriority = "10", MultiLine = "true",
		DisplayName = "[243] Перед боем"))
	FText ThreatText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Наёмник", meta = (DisplayPriority = "11",
		DisplayName = "[244] Ответ героя: начать бой"))
	FText FightReplyText;

private:
	void SpawnMercenary();
	void FreezeMercenary(bool bFrozen);

	TWeakObjectPtr<AEnemyCharacter> Mercenary;
	TSubclassOf<AEnemyCharacter> TestClass;
	bool bMercenarySpawned = false;
	bool bTalkDone = false;
	bool bFightStarted = false;
	int32 TalkStep = 0;
};
