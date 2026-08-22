// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ContrarySurvivor/Data/ContraryItemRow.h"

class UDataTable;
class UWorld;
class AMasterInventoryItem;

/**
 * Доступ к таблице-реестру предметов DT_Items и спавн предмета из строки (ADR-075, спека
 * spec-datatables-phase2.md §а). Чистые статики без UObject (по образцу DailyRewardLogic):
 * вызовы только из C++ (покупка, пикап, лут), BP-обёртки не нужны — логика остаётся в C++.
 *
 * Все функции живут на мягкой деградации: таблица не назначена в настройках проекта
 * (UContraryDataSettings) или строки нет — возвращается ноль, вызывающий идёт прежним
 * путём (конструкторные дефолты классов). Игра обязана работать и без таблиц.
 */
namespace ContraryItems
{
	// Таблица предметов из настроек проекта (загружает мягкую ссылку). nullptr = не назначена.
	UDataTable* GetItemTable();

	// Строка таблицы по имени. nullptr = таблицы нет или строки нет (без warning-спама:
	// отсутствие таблицы — штатный режим).
	const FContraryItemRow* FindRow(FName RowName);

	// Обратный поиск: имя строки по СТАРОМУ служебному ключу (LegacyKey, а при пустом —
	// само имя строки). Нужен выкупу торговца (у предмета в руках есть только ItemName) и
	// стыковке старых сейвов. NAME_None = не найдено.
	FName FindRowNameByLegacyKey(const FString& Key);

	// Накладывает данные строки на УЖЕ созданный предмет: служебный ключ (LegacyKey/имя
	// строки), переводимое название, иконку, меш, лимит стака, тип расходника, слот/защиту/
	// меш брони. StackCount > 0 — выставить стак (обрезается лимитом). Категорию строки
	// применяет ТОЛЬКО поверх стандартной: заполняющий таблицу отвечает за сходство
	// категории с классом (комментарий у FContraryItemRow::Category).
	void ApplyRowToItem(AMasterInventoryItem& Item, const FContraryItemRow& Row, FName RowName, int32 StackCount = 0);

	// Спавнит предмет по строке таблицы: класс из Row.ItemClass + ApplyRowToItem.
	// nullptr = нет мира/строки/класса (в лог — одно предупреждение по месту).
	AMasterInventoryItem* SpawnItemFromRow(UWorld* World, FName RowName, int32 StackCount = 0,
		const FTransform& Transform = FTransform::Identity);
}
