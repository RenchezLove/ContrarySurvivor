// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GenerateWbpCommandlet.generated.h"

class UWidgetBlueprint;

/**
 * Генерация WBP-ассетов кодом (ADR-048, команда Рината 07-18: «собери WBP сам, я подредактирую»).
 * Python дерево WBP строить не может (проверенное ограничение UE 5.5, ContrarySurvivorHUD.h),
 * editor-C++ — может: WidgetTree Widget Blueprint'а редактируется напрямую, дальше штатные
 * FKismetEditorUtilities::CompileBlueprint + UPackage::SavePackage.
 *
 * Запуск (редактор НЕ нужен, headless):
 *   UnrealEditor-Cmd.exe <проект.uproject> -run=GenerateWbp            — создать WBP_InteractPrompt
 *   UnrealEditor-Cmd.exe <проект.uproject> -run=GenerateWbp -verify    — загрузить и распечатать дерево
 *   Флаг -force разрешает перезапись существующего ассета (по умолчанию — отказ, чтобы
 *   не затереть правки Рината).
 *
 * Спайк: один ассет Content/UI/WBP_InteractPrompt (родитель InteractPromptWidget, схема
 * umg-layout-guide.md: Border-плашка + Text «PromptText»). Остальные экраны — после «добро».
 */
UCLASS()
class UGenerateWbpCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;

private:
	// Создаёт, компилирует и сохраняет WBP_InteractPrompt. 0 — успех.
	int32 GenerateInteractPrompt(bool bForce);

	// Отдельный прогон-проверка: грузит сохранённый ассет с диска, печатает родительский
	// класс и дерево виджетов, проверяет наличие кубика PromptText. 0 — всё на месте.
	int32 VerifyInteractPrompt();

	// Рекурсивная печать дерева виджетов (имя + класс) в лог.
	void DumpWidgetTree(class UWidget* Widget, int32 Depth);
};
