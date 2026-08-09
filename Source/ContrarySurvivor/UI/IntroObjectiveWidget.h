// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ContrarySurvivor/UI/SelfHidingWidget.h"
#include "IntroObjectiveWidget.generated.h"

class UBorder;
class UTextBlock;

/**
 * Строка задачи вступления вверху по центру («Впереди деревня. Дойти до неё.»).
 * Переезд с холста на живое окно (просьба Рината 08-09: «все интерфейсы должны переехать
 * на WBP»); раньше её рисовал код HUD прямо на холсте (DrawIntroObjective).
 *
 * Два пути, как у остальных постоянных панелей (ADR-048):
 *  - в слоте HUD стоит WBP_IntroObjective → дерево владельца из дизайнера, кубики по
 *    BindWidgetOptional-именам;
 *  - слот пуст → окно создаётся прямо из этого C++-класса, дерево строит BuildCodeTree
 *    (приём окна обыска трупа: пустой слот не оставляет игрока без строки задачи).
 *
 * ⛔ ПРЯЧЕТ СЕБЯ ТОЛЬКО ЧЕРЕЗ БАЗУ USelfHidingWidget (SetContentVisible). Прямой
 * SetVisibility(Collapsed) на самом окне запрещён: Slate тикает лишь отрисовываемые
 * виджеты, свернувшее себя окно теряет ежекадровый вызов и обратно не разворачивается
 * (баг смоука 07-18, повтор 08-09 на полосах состояния — панель статов пропала совсем).
 *
 * ЧТО ГДЕ ПРАВИТСЯ. Сами формулировки задач — поля контроллера (IntroObjectiveGoToVillage,
 * IntroObjectiveFindElder): это данные хода игры, их ставит код вступления. Здесь — вид
 * строки (шрифт, цвет, плашка) и обрамление текста (ObjectiveFormat).
 */
UCLASS()
class CONTRARYSURVIVOR_API UIntroObjectiveWidget : public USelfHidingWidget
{
	GENERATED_BODY()

public:
	// Показывать ли строку задачи. Чистое правило, гоняется автотестом: пустая задача —
	// строки нет вовсе; при открытом главном меню игровой интерфейс тоже молчит (урок 08-09).
	static bool ShouldShowObjective(const FText& Objective, bool bMainMenuOnScreen);

	// Обрамление задачи. {Objective} — сама задача из вступления; остальное — твой текст
	// (например «Задача: {Objective}»). Пустое обрамление недопустимо — тогда игрок не
	// увидит задачу вовсе, поэтому подстановка обязана в строке присутствовать.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Objective|Текст",
		meta = (DisplayName = "Обрамление задачи", DisplayPriority = "1"))
	FText ObjectiveFormat = NSLOCTEXT("IntroObjectiveWidget", "ObjectiveFormat", "{Objective}");

	// Вид строки. Действует ТОЛЬКО для кодового дерева-запаски: если окно пришло из
	// WBP_IntroObjective, шрифт и цвет правятся мышкой в дизайнере (правило ADR-048).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Objective|Вид",
		meta = (DisplayName = "Кегль строки", ClampMin = "8", DisplayPriority = "1"))
	int32 ObjectiveFontSize = 34;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Objective|Вид",
		meta = (DisplayName = "Цвет строки", DisplayPriority = "2"))
	FLinearColor ObjectiveColor = FLinearColor(1.0f, 0.85f, 0.3f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Objective|Вид",
		meta = (DisplayName = "Цвет плашки под строкой", DisplayPriority = "3"))
	FLinearColor PlateColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.55f);

	// Отступ строки от ВЕРХНЕГО края экрана, точек (кодовое дерево-запаска).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro Objective|Вид",
		meta = (DisplayName = "Отступ сверху", ClampMin = "0.0", DisplayPriority = "4"))
	float TopMargin = 64.0f;

protected:
	virtual void NativeOnInitialized() override;

	// Каждый кадр: задача берётся у HUD (перенос поведения Canvas DrawIntroObjective).
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// --- Кубики WBP_IntroObjective (имена ТОЧНЫЕ) ---

	// Плашка под строкой: без неё светлый текст теряется на светлом дне.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ObjectivePlate;

	// Сама строка задачи. Текст ставит код, брать его в дизайнере не нужно.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ObjectiveText;

private:
	// Кодовое дерево-запаска (имена кубиков совпадают с генерируемым WBP_IntroObjective).
	void BuildCodeTree();

	// Дерево пришло из WBP-ассета — вид его, код не перекрашивает (ADR-048).
	bool bDesignerTree = false;
};
