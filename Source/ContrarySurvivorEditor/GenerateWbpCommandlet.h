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
 *   UnrealEditor-Cmd.exe <проект.uproject> -run=GenerateWbp -verify  — прогон-проверка: загрузить
 *     каждый сгенерированный ассет, распечатать родителя и дерево, сверить имена кубиков
 *   UnrealEditor-Cmd.exe <проект.uproject> -run=GenerateWbp -augment — ТОЧЕЧНО дополнить
 *     СУЩЕСТВУЮЩИЙ ассет новыми кубиками (стилизация Рината сохраняется: меняется только
 *     добавляемое; кубик уже есть — пропуск). Сейчас: WeaponIconImage в WBP_TouchControls.
 *   UnrealEditor-Cmd.exe <проект.uproject> -run=GenerateWbp -rebuild — ПЕРЕСОБРАТЬ дерево
 *     существующих WBP_Shop/WBP_Inventory канвас-первой раскладкой (ADR-051 п.1: кнопки,
 *     подложки и списки получают ручки мышкой в дизайнере). Только эти два ассета; в них
 *     нет ручной стилизации владельца (git-история — прогоны коммандлета), запускать при
 *     чистом git status обоих файлов.
 *   Флаг -force разрешает перезапись существующего ассета (по умолчанию — пропуск, чтобы
 *   не затереть правки Рината).
 *
 * Генерируются ТОЛЬКО ассеты из таблицы (готовые WBP Рината — магазин/строка/диалог/статы —
 * в таблице отсутствуют и не трогаются). Дефолтная раскладка повторяет текущий вид игры:
 * тач-слой — по значениям с CDO BP-контроллера (та же геометрия и стиль, что строил код),
 * экраны — по геометрии Canvas-отрисовки HUD (требование лида 07-18).
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
	// и дерево, сверяется наличие всех ожидаемых кубиков. 0 — всё на месте.
	int32 VerifyAll();

	// Точечное дополнение существующих ассетов новыми кубиками (идемпотентно). 0 — успех.
	int32 AugmentAll();

	// Пересборка деревьев WBP_Shop/WBP_Inventory канвас-первой раскладкой (-rebuild,
	// ADR-051 п.1). Blueprint не пересоздаётся — меняется только WidgetTree. 0 — успех.
	int32 RebuildWindows();

	// Рекурсивная печать дерева виджетов (имя + класс) в лог.
	void DumpWidgetTree(class UWidget* Widget, int32 Depth);
};
