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
 *     замкнута, верхнеуровневые элементы свободны — GLockContracts) и назначенный класс
 *     плитки WBP_ItemTile_C у WBP_Inventory/WBP_Shop/WBP_CorpseLoot (Build 1.2.2)
 *   UnrealEditor-Cmd.exe <проект.uproject> -run=GenerateWbp -augment — ТОЧЕЧНО дополнить
 *     СУЩЕСТВУЮЩИЕ ассеты новыми кубиками и текстами (стилизация Рината сохраняется:
 *     меняется только добавляемое; кубик уже есть — пропуск). Список правок — таблица Specs
 *     в AugmentAll (.cpp): WBP_Shop (надписи, строка пересчёта патронов, золотая кнопка —
 *     отдельные правки), WBP_Inventory, WBP_PlayerStats, WBP_QuestTracker, WBP_TouchControls.
 *   UnrealEditor-Cmd.exe <проект.uproject> -run=GenerateWbp -rebuild — ПЕРЕСОБРАТЬ дерево
 *     существующих окон канвас-первой раскладкой (ADR-051 п.1: кнопки, подложки и списки
 *     получают ручки мышкой в дизайнере; волна разлочки Build 1.2.1 — без рядов-коробок
 *     вовсе: каждый текст в своём канвас-слоте). Список — RebuildAssets в .cpp (плитка
 *     предмета, магазин, инвентарь, окно обыска, панель статов, диалог, экран смерти);
 *     всем ассетам включён перенос ручной стилизации владельца на новое дерево
 *     (TransferOwnerStyle). Запускать при чистом git status пересобираемых .uasset.
 *   UnrealEditor-Cmd.exe <проект.uproject> -run=GenerateWbp -dumpslots — печать геометрии
 *     слота каждого виджета всех ассетов таблицы. Ассеты не меняются; формат строк
 *     стабильный, поэтому логи двух прогонов (до и после -rebuild) сравниваются диффом —
 *     это артефакт «расстановка владельца сохранилась» вместо осмотра мышкой.
 *   UnrealEditor-Cmd.exe <проект.uproject> -run=GenerateWbp -pickupfix — Build 1.2.1
 *     (ТЗ А2/А4): M_VColor в слоты мешей лута + эмиссив-параметры свечения в M_VColor
 *     (см. FixPickupAssets ниже).
 *   Флаг -force разрешает перезапись существующего ассета (по умолчанию — пропуск, чтобы
 *   не затереть правки Рината). Режимы взаимоисключающие, проверяются в порядке
 *   -verify, -augment, -rebuild, -dumpslots, -adicon, -pickupfix; без них — обычная генерация.
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
	// и дерево, сверяются ожидаемые кубики, замки дизайнера и класс плитки окон с сетками.
	// 0 — всё на месте.
	int32 VerifyAll();

	// Точечное дополнение существующих ассетов новыми кубиками (идемпотентно). 0 — успех.
	int32 AugmentAll();

	// Пересборка деревьев окон из списка RebuildAssets канвас-первой раскладкой (-rebuild,
	// ADR-051 п.1). Blueprint не пересоздаётся — меняется только WidgetTree. 0 — успех.
	// AssetFilter (флаг -asset=WBP_Death) непустой — пересобирается ТОЛЬКО этот ассет
	// (Build 1.2: точечная пересборка экрана смерти, остальные окна не трогаются).
	int32 RebuildWindows(const FString& AssetFilter);

	// Build 1.2 (-adicon): процедурная генерация текстуры единой иконки видео
	// (треугольник воспроизведения в скруглённом квадрате, ТЗ раздел 0 п.8) в
	// /Game/UI/Icons/T_Icon_AdVideo. Белая с альфой; тинт задаёт кнопка. 0 — успех.
	int32 GenerateAdIcon();

	// Build 1.2.1 (-pickupfix, ТЗ А2/А4): чинит ассеты пикапов — прописывает M_VColor в
	// слоты мешей лута (SM_LootSack/SM_HidePickup/SM_LootBackpack: импортированы без
	// материала, в слоте WorldGridMaterial — «серая шахматка») и добавляет в M_VColor
	// эмиссив-параметры свечения GlowColor/GlowIntensity с нулевыми дефолтами (живые
	// значения ставит MID пикапа). Идемпотентно; печатает срез-пруф (слоты, вершинные
	// цвета, габариты). 0 — успех.
	int32 FixPickupAssets();

	// Печать геометрии слота каждого виджета всех ассетов таблицы (-dumpslots): стабильный
	// лог для посвойственного сравнения расстановки владельца до/после пересборки диффом.
	int32 DumpSlotsAll();

	// Рекурсивная печать дерева виджетов (имя + класс) в лог.
	void DumpWidgetTree(class UWidget* Widget, int32 Depth);
};
