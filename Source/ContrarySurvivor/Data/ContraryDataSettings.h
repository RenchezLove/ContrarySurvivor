// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"
#include "ContraryDataSettings.generated.h"

class UDataTable;

/**
 * Ссылки на таблицы данных проекта (ADR-075: таблица предметов DT_Items и таблица квестов
 * DT_Quests). Страница в Project Settings → Game → «Contrary Survivor: таблицы данных»;
 * значения пишутся в Config/DefaultGame.ini (config=Game, defaultconfig) — задаются один
 * раз, без синглтонов и без BP.
 *
 * ПУСТАЯ ссылка = мягкая деградация: потребители (торговец, староста, спавн предметов)
 * откатываются на прежние конструкторные дефолты C++ — игра обязана работать и без таблиц
 * (общий паттерн проекта: мягкие ссылки не роняют игру).
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Contrary Survivor: таблицы данных"))
class CONTRARYSURVIVOR_API UContraryDataSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// Раздел Project Settings, в котором лежит страница (штатная категория Game).
	virtual FName GetCategoryName() const override { return FName(TEXT("Game")); }

	// Таблица-реестр предметов (строки FContraryItemRow). Заполняет unreal-operator.
	UPROPERTY(config, EditAnywhere, Category = "Таблицы данных", meta = (
		DisplayName = "Таблица предметов (DT_Items)",
		ToolTip = "Реестр предметов: ключ, название, иконка, меш, цена, категория, стак, броня. Пусто = игра живёт на прежних значениях из кода."))
	TSoftObjectPtr<UDataTable> ItemTable;

	// Таблица квестов (строки FContraryQuestRow). Заполняет unreal-operator.
	UPROPERTY(config, EditAnywhere, Category = "Таблицы данных", meta = (
		DisplayName = "Таблица квестов (DT_Quests)",
		ToolTip = "Реестр квестов: цели, количества, награды, тексты. Пусто = староста выдаёт квесты из прежних значений в коде."))
	TSoftObjectPtr<UDataTable> QuestTable;

	// Доступ по месту использования (CDO настроек; всегда валиден).
	static const UContraryDataSettings* Get() { return GetDefault<UContraryDataSettings>(); }
};
