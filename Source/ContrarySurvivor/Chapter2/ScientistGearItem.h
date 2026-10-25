// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AQuestItem.h"
#include "ScientistGearItem.generated.h"

class UTexture2D;

/**
 * Снаряжение учёных главы 2 (правка Рината 25.10.2026 вечер: «Вера выдает нам эти предметы
 * (продать и выкинуть их нельзя)»): «Баллон с воздухом» [231]/[232] и «Детектор» [233]/[234].
 *
 * Какой из двух предметов — решает служебный ключ ItemName (Chapter2Ids::AirTankKey или
 * DetectorKey): с ним предмет и уезжает в сохранение, по нему же при загрузке получает своё
 * название, описание и картинку. Тексты и картинки — в настройках проекта «Contrary Survivor:
 * глава 2»; строка в таблице предметов не нужна, но если её туда добавят с тем же ключом —
 * данные строки главнее.
 *
 * Сюжетный предмет (bStoryItem): продать, выбросить и переложить в чужое хранилище нельзя,
 * при смерти не теряется (категория задания). Выдаёт и забирает его UChapter2Component.
 */
UCLASS()
class CONTRARYSURVIVOR_API AScientistGearItem : public AQuestItem
{
	GENERATED_BODY()

public:
	AScientistGearItem();

	// Название, описание и картинка для этого ключа — из настроек главы 2 (пустые поля
	// предмета заполняются, заполненные не трогаются). Публичная — её зовёт автопроверка.
	void ApplyGearDefaults();

	// Один из двух ключей снаряжения?
	static bool IsGearKey(const FString& Key);

	// Картинка снаряжения по ключу (баллон — значок воздуха, детектор — значок детектора).
	static UTexture2D* ResolveGearIcon(const FString& Key);

protected:
	virtual void PostInitializeComponents() override;
};
