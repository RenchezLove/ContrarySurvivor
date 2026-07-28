// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GenerateWbpCommandlet.generated.h"

class UWidgetBlueprint;
class UWidgetTree;

/**
 * Генерация WBP-ассетов кодом (ADR-048, команда Рината 07-18: «собери WBP сам, я подредактирую»).
 * Python дерево WBP строить не может (проверенное ограничение UE 5.5, ContrarySurvivorHUD.h),
 * editor-C++ — может: WidgetTree Widget Blueprint'а редактируется напрямую, дальше штатные
 * FKismetEditorUtilities::CompileBlueprint + UPackage::SavePackage.
 *
 * Запуск (редактор НЕ нужен, headless):
 *   UnrealEditor-Cmd.exe <проект.uproject> -run=GenerateWbp          — создать недостающие WBP
 *   UnrealEditor-Cmd.exe <проект.uproject> -run=GenerateWbp -verify  — прогон-проверка: каждый
 *     ассет таблицы грузится с диска, печатается родитель и дерево, сверяются родительский
 *     класс, наличие ожидаемых кубиков, контракт замков дизайнера (начинка кнопок и рядов
 *     замкнута, верхнеуровневые элементы свободны — GLockContracts) и назначенные классы
 *     строк списков у WBP_Shop и WBP_Inventory
 *   UnrealEditor-Cmd.exe <проект.uproject> -run=GenerateWbp -augment — ТОЧЕЧНО дополнить
 *     СУЩЕСТВУЮЩИЕ ассеты новыми кубиками и текстами (стилизация Рината сохраняется:
 *     меняется только добавляемое; кубик уже есть — пропуск). Список правок — таблица Specs
 *     в AugmentAll (.cpp): WBP_Shop (надписи и строка пересчёта патронов — две отдельные
 *     правки), WBP_Inventory, WBP_PlayerStats, WBP_ShopRow, WBP_QuestTracker, WBP_TouchControls.
 *   UnrealEditor-Cmd.exe <проект.uproject> -run=GenerateWbp -rebuild — ПЕРЕСОБРАТЬ дерево
 *     существующих окон канвас-первой раскладкой (ADR-051 п.1: кнопки, подложки и списки
 *     получают ручки мышкой в дизайнере). Список — RebuildAssets в .cpp (магазин,
 *     инвентарь, панель статов, диалог, экран смерти); ассетам с ручной стилизацией
 *     владельца включён её перенос на новое дерево (TransferOwnerStyle). Запускать при
 *     чистом git status пересобираемых .uasset.
 *   UnrealEditor-Cmd.exe <проект.uproject> -run=GenerateWbp -dumpslots — печать геометрии
 *     слота каждого виджета всех ассетов таблицы. Ассеты не меняются; формат строк
 *     стабильный, поэтому логи двух прогонов (до и после -rebuild) сравниваются диффом —
 *     это артефакт «расстановка владельца сохранилась» вместо осмотра мышкой.
 *   Флаг -force разрешает перезапись существующего ассета (по умолчанию — пропуск, чтобы
 *   не затереть правки Рината). Режимы взаимоисключающие, проверяются в порядке
 *   -verify, -augment, -rebuild, -dumpslots; без них — обычная генерация.
 *
 * Генерируются ТОЛЬКО ассеты из таблицы GAssets. Дефолтная раскладка повторяет текущий
 * вид игры: тач-слой — по значениям с CDO BP-контроллера (та же геометрия и стиль, что
 * строил код), экраны — по геометрии Canvas-отрисовки HUD (требование лида 07-18).
 */
UCLASS()
class UGenerateWbpCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;

private:
	// Создаёт, наполняет, компилирует и сохраняет все недостающие ассеты таблицы. 0 — успех.
	int32 GenerateAll(bool bForce);

	// Отдельный прогон-проверка: каждый ассет таблицы грузится с диска, печатается родитель
	// и дерево, сверяются ожидаемые кубики, замки дизайнера и классы строк списков.
	// 0 — всё на месте.
	int32 VerifyAll();

	// Точечное дополнение существующих ассетов новыми кубиками (идемпотентно). 0 — успех.
	int32 AugmentAll();

	// Пересборка деревьев окон из списка RebuildAssets канвас-первой раскладкой (-rebuild,
	// ADR-051 п.1). Blueprint не пересоздаётся — меняется только WidgetTree. 0 — успех.
	int32 RebuildWindows();

	// Печать геометрии слота каждого виджета всех ассетов таблицы (-dumpslots): стабильный
	// лог для посвойственного сравнения расстановки владельца до/после пересборки диффом.
	int32 DumpSlotsAll();

	// Рекурсивная печать дерева виджетов (имя + класс) в лог.
	void DumpWidgetTree(class UWidget* Widget, int32 Depth);
};
