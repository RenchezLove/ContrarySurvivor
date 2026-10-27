// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AQuestItem.h"
#include "AnomalyFormationItem.generated.h"

/**
 * Предмет «Аномальное образование» в сумке (правка Рината 27.10.2026: «Сами аномальные
 * образования сделай предметом. Этот предмет попадает в инвентарь, когда игрок берёт его»).
 *
 * Сколько образований лежит в сумке, держит UChapter2Component (SyncFormationsInBag): пока
 * задание «Образцы из аномалий» идёт или готово к сдаче — ровно столько, сколько собрано
 * (счётчик цели [216]); задание сдано — Вера их забирает. Так и сохранение, записанное до
 * 27.10.2026 (образцы подобраны, а в сумке пусто), получает предметы само.
 *
 * Все образования — одна стопка (одна плитка с числом). Сюжетный предмет (bStoryItem):
 * продать, выбросить и переложить в чужое хранилище нельзя — сообщение
 * UChapter2Settings::FormationLockedText; при смерти не теряется (категория задания).
 * Название [226], описание [247], значок T_IconFormation — из настроек «глава 2».
 */
UCLASS()
class CONTRARYSURVIVOR_API AAnomalyFormationItem : public AQuestItem
{
	GENERATED_BODY()

public:
	AAnomalyFormationItem();

	// Название, описание и значок из настроек главы 2 (название — всегда, описание и значок —
	// если пусто). Публичная — её зовёт автопроверка.
	void ApplyFormationDefaults();

	// Это ключ аномального образования?
	static bool IsFormationKey(const FString& Key);

protected:
	virtual void PostInitializeComponents() override;
};
