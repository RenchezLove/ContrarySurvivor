// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "PatchWbpDialogCommandlet.generated.h"

class UWidgetTree;

/**
 * Точечная починка Content/UI/WBP_Dialog.uasset (доводка Build 1, 07-24): в ассете нет
 * текстовых кубиков подписей кнопок AcceptText/DeclineText/CloseText (BindWidgetOptional-поля
 * UDialogScreenWidget), из-за чего реплики героя на кнопках никогда не показывались — кнопки
 * оставались со статичными подписями.
 *
 * Запуск (редактор НЕ нужен, headless):
 *   UnrealEditor-Cmd.exe <проект.uproject> -run=PatchWbpDialog
 *
 * Гарантии сохранности (в ассете лежат незакоммиченные правки Рината):
 *  - НЕ перегенерация: дерево не пересобирается, корень и раскладка не трогаются; правится
 *    ТОЛЬКО содержимое кнопок AcceptButton/DeclineButton/CloseButton (и контрольно TurnInButton).
 *  - Кнопка уже содержит ровно один текстовый кубик (обычный случай: статичная подпись из
 *    генерации, возможно перестилизованная Ринатом) — кубик ПЕРЕИМЕНОВЫВАЕТСЯ в точное имя
 *    привязки, его шрифт/цвет/стиль не меняются вовсе. Добавить ВТОРОЙ текст внутрь кнопки
 *    нельзя физически: у UButton один дочерний слот. Привязка BindWidgetOptional работает по
 *    имени объекта виджета (UE 5.5, WidgetBlueprintGeneratedClass.cpp:263-271), а компилятор
 *    WBP сам сопоставляет виджет нативному BindWidget-полю по имени без создания дубль-переменной
 *    (WidgetBlueprintCompiler.cpp:587-595) — поэтому переименования достаточно.
 *  - Кнопка пуста — создаётся новый UTextBlock в стиле кнопок диалога из GenerateWbpCommandlet
 *    (Roboto 15 Regular, белый).
 *  - Идемпотентность: кубик с нужным именем уже есть — в лог пишется «уже есть», ассет не меняется.
 *  - Непонятная структура (не кнопка / несколько текстов внутри) — предупреждение и пропуск,
 *    ничего не угадываем.
 *  - Ассет сохраняется ТОЛЬКО если что-то реально изменилось; компиляция Blueprint с ошибкой —
 *    не сохраняем, файл на диске остаётся прежним.
 */
UCLASS()
class UPatchWbpDialogCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
