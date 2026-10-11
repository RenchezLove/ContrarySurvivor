// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ContrarySurvivor/UI/SelfHidingWidget.h"
#include "QuestTrackerWidget.generated.h"

class UTextBlock;
class APlayerCharacter;
enum class EElderNotebookWaitPhase : uint8;

/**
 * Трекер активного квеста на UMG (ADR-048, этап 3): строка «Квест: Шкуры волков —
 * Шкура волка 1/3» в углу экрана. Раскладку WBP_QuestTracker строит Ринат. Живёт на
 * экране всю игру (создаёт HUD в BeginPlay); сам прячется, когда квеста нет или
 * открыт модальный экран (как Canvas-путь: на модалках квест виден в самом диалоге) —
 * через SetContentVisible базы (Collapsed на себе остановил бы тик навсегда, баг 07-18).
 */
UCLASS()
class CONTRARYSURVIVOR_API UQuestTrackerWidget : public USelfHidingWidget
{
	GENERATED_BODY()

public:
	// --- Настройки (Class Defaults WBP_QuestTracker; владение переехало из HUD — ADR-048) ---

	// Строка трекера собирается кодом целиком: слово «Квест» меняется на «Квест выполнен»
	// по состоянию, поэтому в отдельную статичную подпись оно уйти не может (ADR-050).
	// Подстановки: {Title} — название квеста, {Objectives} — строка целей.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker|Texts", meta = (DisplayPriority = "1"))
	FText TrackerFormat = NSLOCTEXT("QuestTracker", "TrackerFormat", "Квест: {Objectives}");

	// У выполненного квеста прогресс не показываем: он уже 3 из 3 и только удлиняет строку.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker|Texts", meta = (DisplayPriority = "2"))
	FText DoneFormat = NSLOCTEXT("QuestTracker", "DoneFormat",
		"Квест выполнен: {Title} — вернись к старосте");

	// Готовое задание, которое сдаётся НЕ старосте (10.10.2026: ноутбук — учёным, рация —
	// охраннику): {Hint} — подсказка из самого задания («отнеси ноутбук учёным»).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker|Texts", meta = (DisplayPriority = "2"))
	FText DoneWithHintFormat = NSLOCTEXT("QuestTracker", "DoneWithHintFormat", "Квест: {Title} — {Hint}");

	// Одна цель квеста: {Objective} — что сделать, {Done} — сделано, {Total} — сколько надо.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker|Texts", meta = (DisplayPriority = "3"))
	FText ObjectiveFormat = NSLOCTEXT("Quest", "ObjectiveFormat", "{Objective} {Done} из {Total}");

	// Между двумя целями, когда их у квеста две.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker|Texts", meta = (DisplayPriority = "4"))
	FText ObjectiveSeparator = NSLOCTEXT("Quest", "ObjectiveSeparator", ", ");

	// --- Ожидание после сдачи ноутбука старосте (Ринат 10.10.2026) ---
	// Пока староста «разбирается с ноутбуком», в этом же блоке стоит строка с оставшимся
	// временем: {Minutes} — сколько минут осталось (вверх до целой, не меньше одной).
	// Черновик, правится здесь. Перенос строки внутри текста — намеренный: строка длинная.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker|Texts", meta = (DisplayPriority = "5", MultiLine = "true"))
	FText ElderWaitFormat = NSLOCTEXT("QuestTracker", "ElderWaitFormat",
		"Староста разбирается с ноутбуком — осталось около {Minutes} мин.\nПока можно заработать на волчьих шкурах.");

	// Время вышло — строка держится, пока следующее задание у старосты не взято. Черновик.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker|Texts", meta = (DisplayPriority = "6", MultiLine = "true"))
	FText ElderWaitReadyText = NSLOCTEXT("QuestTracker", "ElderWaitReady",
		"Староста зовёт: есть новости про ноутбук.");

	// Строка ожидания по её состоянию (пусто — показывать нечего). Чистая функция: её же
	// зовёт запасная отрисовка экрана игры и проверяют автотесты.
	static FText BuildElderWaitLine(EElderNotebookWaitPhase Phase, float RemainingSeconds,
		const FText& WaitFormat, const FText& ReadyText);

	// Цвета строки. С 10.10.2026 идущее задание красится цветом своего вида (основное или
	// побочное — UContraryPdaSettings), поэтому ActiveColor больше не применяется; выполненное
	// по-прежнему зелёное.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker", meta = (DisplayPriority = "4"))
	FLinearColor ActiveColor = FLinearColor(1.0f, 0.85f, 0.3f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker", meta = (DisplayPriority = "5"))
	FLinearColor DoneColor = FLinearColor(0.4f, 1.0f, 0.4f, 1.0f);

	// --- Метка побочного задания в мире (Ринат 11.10.2026: «у игрока на экране 2 маркера. Один -
	// маркер основного квеста (оранжевый/желтый как сейчас), второй - маркер второстепенного
	// квеста - синий»). Значок, подпись и краевая стрелка лежат в файле окна (SideQuestMarker,
	// SideQuestMarkerText, SideQuestEdgeArrow): вид, размер, цвет и шрифт правятся там. Код
	// только показывает их и ставит на место цели на экране. ---

	// Подъём метки над целью, как у метки основного задания.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker|SideMarker", meta = (DisplayPriority = "1",
		DisplayName = "Подъём метки над целью, см"))
	float SideMarkerZOffset = 300.0f;

	// Цель за кадром: стрелка стоит на таком расстоянии от края экрана.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker|SideMarker", meta = (ClampMin = "0.0", DisplayPriority = "2",
		DisplayName = "Отступ краевой стрелки от края экрана"))
	float SideMarkerEdgeMargin = 56.0f;

	// Начало подписи метки, когда задание готово и ведёт к тому, кто его принимает.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestTracker|SideMarker", meta = (DisplayPriority = "3",
		DisplayName = "Начало подписи готового задания"))
	FText SideMarkerTurnInPrefix = NSLOCTEXT("QuestTracker", "SideTurnIn", "Сдать: ");

	// Показывать метку второстепенного задания на точке мира с такой подписью. Действует, пока
	// точку не сменят или метку не спрячут.
	void ShowSideQuestMarker(const FVector& WorldPoint, const FText& Label);
	void HideSideQuestMarker();
	bool IsSideQuestMarkerShown() const { return bSideMarkerWanted; }
	FVector GetSideQuestMarkerPoint() const { return SideMarkerWorldPoint; }
	FText GetSideQuestMarkerLabel() const { return SideMarkerLabel; }

	// Перечитать журнал героя: есть идущее побочное задание с целью на уровне — показать метку
	// на ней, нет — спрятать. В игре зовёт кадр окна; автопроверки — напрямую.
	void RefreshSideQuestMarker(APlayerCharacter* Player);

	// Где на экране стоит метка. Цель в кадре — сама метка в точке цели; цель за кадром или за
	// спиной камеры — стрелка у края экрана на луче из середины, повёрнутая к цели.
	struct FMarkerPlacement
	{
		bool bInView = false;
		FVector2D Position = FVector2D::ZeroVector;
		float ArrowAngle = 0.0f; // градусы по часовой; 0 — стрелка смотрит вверх
	};
	// Projected — точка цели на экране (единицы раскладки), bInFront — цель перед камерой;
	// BehindDir — направление на цель на экране, когда она за спиной камеры.
	static FMarkerPlacement ComputeMarkerPlacement(bool bInFront, const FVector2D& Projected,
		const FVector2D& BehindDir, const FVector2D& Area, float Margin);

protected:
	// Каждый кадр: текст/цвет/видимость (дёшево; модалки и прогресс меняются на лету).
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// --- Кубики WBP_QuestTracker (имена ТОЧНЫЕ — см. umg-layout-guide.md) ---

	// Строка трекера. Видимость и цвет ставит код.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TrackerText;

	// Метка побочного задания: значок с подписью (середина блока встаёт на цель) и стрелка у
	// края экрана. Место на экране ставит код, всё остальное — файл окна.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SideQuestMarker;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SideQuestMarkerText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SideQuestEdgeArrow;

private:
	// Поставить метку или стрелку на экран по нынешнему виду камеры.
	void PlaceSideQuestMarker(const FGeometry& MyGeometry);

	bool bSideMarkerWanted = false;
	FVector SideMarkerWorldPoint = FVector::ZeroVector;
	FText SideMarkerLabel;

	// Цель побочного задания ищется по уровню не каждый кадр: пока задание, его метка и стадия
	// те же, берётся найденная; неудачный поиск повторяется раз в полсекунды.
	TWeakObjectPtr<AActor> SideTargetCache;
	uint32 SideTargetKey = 0;
	float SideTargetNextSearchTime = 0.0f;
};
