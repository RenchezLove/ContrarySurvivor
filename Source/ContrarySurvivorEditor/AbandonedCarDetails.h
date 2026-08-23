// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

/**
 * Панель Details брошенной машины (Report1 Рината 23.08, п.9, дословно: «У нас будет как бы
 * таблица (на подобие таблицы прессетов коллизии). Первый столбец - название детали, второй
 * столбец это галки наличия детали, третий столбец это галки битая/не битая»).
 *
 * Галочки живут обычными bool-полями на AAbandonedCar (данные и совместимость с уже
 * расставленными экземплярами), а здесь они прячутся из штатной раскладки и рисуются
 * таблицей: строка = деталь, в значении два флажка «есть» и «битая». Заодно категория
 * машины поднимается наверх панели («Все именно Наши настройки подними вверх»).
 *
 * Регистрируется на класс AAbandonedCar в FContrarySurvivorEditorModule::StartupModule —
 * действует и на BP_AbandonedCar (кастомизации класса наследуются потомками).
 */
class FAbandonedCarDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
