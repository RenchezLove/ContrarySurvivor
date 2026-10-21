// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ContrarySurvivor/Components/QuestComponent.h" // FQuest, идентификаторы заданий
#include "QuestBarrier.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class APlayerCharacter;

/**
 * Шлагбаум, который открывают задания (10.10.2026: въезд на базу учёных; слова Рината —
 * «шлагбаум закрыт, когда герой несёт ноутбук», без прохода сюжет не проходится).
 *
 * Правило открытия — одно, чистая функция ShouldBeOpen (решение лида 10.10.2026): открыт, если
 * задание «открывает взятием» принято (идёт, выполнено или сдано) ИЛИ задание «открывает сдачей»
 * сдано. Иначе закрыт.
 * Кроме того, закрытый шлагбаум выпускает героя изнутри (21.10.2026, отзыв игрока «заперт на
 * базе учёных»): ShouldLetHeroOut — снаружи по-прежнему не пускает.
 *
 * Состояние не хранит и в сохранение ничего не пишет — выводит его из журнала заданий героя:
 * подписан на события журнала (смена задания и замена журнала целиком) и, как раскладчик
 * предмета задания, сверяется по таймеру — так он верен после «Продолжить», «Новой игры»,
 * кнопок сюжета меню отладки и появления героя позже самого шлагбаума.
 *
 * Закрыт: невидимая коробка на всю ширину проезда не пускает персонажей (героя и врагов) и
 * вырезает проезд из навигационной сетки. Открыт: коробка выключена и на навигацию не влияет,
 * стрела поднята. Пули, обзор и камера сквозь коробку проходят всегда.
 *
 * Расстановка: актор ставится в середину проезда на землю, ось Y актора — поперёк проезда
 * (вдоль линии забора). Стойка стоит на краю проезда со стороны −Y, стрела тянется от неё в +Y.
 * Своя сетка стрелы: начало координат — у оси поворота, длина — вдоль +Y.
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AQuestBarrier : public AActor
{
	GENERATED_BODY()

public:
	AQuestBarrier();

	// Правило открытия — чистая функция, проверяется автотестом без мира.
	static bool ShouldBeOpen(const TArray<FQuest>& Journal, FName TakenQuestId, FName TurnedInQuestId);

	// Выпуск героя изнутри (21.10.2026, отзыв «заперт на базе учёных»): закрытый шлагбаум не
	// впускает, но и не запирает. InsideDepth — насколько герой зашёл внутрь от оси проезда
	// (см, отрицательно — снаружи), Distance — расстояние до шлагбаума по земле. Открыть, если
	// герой внутри дальше половины преграды и не дальше радиуса; уже открытый для выпуска
	// держится, пока герой не отошёл наружу на запас (не закрывается у него на голове).
	static bool ShouldLetHeroOut(float InsideDepth, float Distance, bool bWasLettingOut,
		float HalfThickness, float Radius, float OutsideClearance);

	// Сверить состояние с журналом героя прямо сейчас (её же зовут события журнала и таймер).
	void RefreshNow();

	bool IsOpen() const { return bOpen; }

	// Насколько стрела поднята: 0 — лежит, 1 — поднята полностью.
	float GetOpenAlpha() const { return OpenAlpha; }

	UBoxComponent* GetBlocker() const { return Blocker; }
	UStaticMeshComponent* GetBoomMesh() const { return BoomMesh; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// --- Правило ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barrier|Правило", meta = (DisplayPriority = "1",
		DisplayName = "Открывает взятием: задание (идентификатор)",
		ToolTip = "Шлагбаум открыт, как только это задание принято, и остаётся открытым после его сдачи. Пусто — не действует."))
	FName OpenWhenTakenQuestId = ContraryQuestIds::DeliverLaptopToScientists;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barrier|Правило", meta = (DisplayPriority = "2",
		DisplayName = "Открывает сдачей: задание (идентификатор)",
		ToolTip = "Шлагбаум открыт, когда это задание сдано. Пусто — не действует."))
	FName OpenWhenTurnedInQuestId = ContraryQuestIds::FetchGuardRadio;

	// --- Размеры (см) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barrier|Размеры", meta = (DisplayPriority = "1", ClampMin = "10.0",
		DisplayName = "Ширина проезда"))
	float PassageWidth = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barrier|Размеры", meta = (DisplayPriority = "2", ClampMin = "1.0",
		DisplayName = "Толщина преграды (вдоль проезда)"))
	float BlockerThickness = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barrier|Размеры", meta = (DisplayPriority = "3", ClampMin = "10.0",
		DisplayName = "Высота преграды"))
	float BlockerHeight = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barrier|Размеры", meta = (DisplayPriority = "4", ClampMin = "0.0",
		DisplayName = "Высота стрелы над землёй"))
	float BoomHeight = 105.0f;

	// Действует только на сетки по умолчанию (движковый куб): свою сетку код не растягивает.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barrier|Размеры", meta = (DisplayPriority = "5", ClampMin = "1.0",
		DisplayName = "Толщина бруска по умолчанию"))
	float DefaultBarThickness = 12.0f;

	// --- Движение стрелы ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barrier|Стрела", meta = (DisplayPriority = "1", ClampMin = "0.0", ClampMax = "120.0",
		DisplayName = "Угол подъёма (градусы)"))
	float OpenAngle = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barrier|Стрела", meta = (DisplayPriority = "2", ClampMin = "0.0",
		DisplayName = "Время подъёма и опускания (сек)"))
	float OpenTime = 1.5f;

	// Как часто сверяться с журналом героя помимо событий журнала (сек).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barrier|Правило", meta = (DisplayPriority = "3", ClampMin = "0.1"))
	float CheckInterval = 0.5f;

	// --- Выпуск изнутри (21.10.2026: герой, возродившийся у костра базы, не мог выйти) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barrier|Выпуск", meta = (DisplayPriority = "1",
		DisplayName = "Выпускать героя изнутри",
		ToolTip = "Закрытый шлагбаум открывается, когда герой подходит к нему изнутри, и закрывается, когда герой вышел. Снаружи по-прежнему не пускает."))
	bool bLetHeroOutFromInside = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barrier|Выпуск", meta = (DisplayPriority = "2",
		DisplayName = "Внутри — по направлению актора",
		ToolTip = "Включено: внутренняя сторона там, куда смотрит стрелка актора (+X). Выключено: с обратной стороны."))
	bool bInsideIsForward = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barrier|Выпуск", meta = (DisplayPriority = "3", ClampMin = "100.0",
		DisplayName = "Радиус выпуска (см)",
		ToolTip = "Шлагбаум открывается для выпуска, когда герой изнутри ближе этого расстояния."))
	float LetOutRadius = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Barrier|Выпуск", meta = (DisplayPriority = "4", ClampMin = "0.0",
		DisplayName = "Запас снаружи до закрытия (см)",
		ToolTip = "Открытый для выпуска шлагбаум закрывается, когда герой отошёл наружу дальше половины преграды плюс этот запас."))
	float LetOutClearance = 200.0f;

	// --- Части ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Barrier")
	USceneComponent* SceneRoot;

	// Стойка: сетку назначает Ринат, по умолчанию — движковый куб, растянутый в столбик.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Barrier")
	UStaticMeshComponent* PostMesh;

	// Ось поворота стрелы: на стойке, на высоте стрелы.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Barrier")
	USceneComponent* BoomPivot;

	// Стрела: сетку назначает Ринат, по умолчанию — движковый куб, растянутый в брусок.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Barrier")
	UStaticMeshComponent* BoomMesh;

	// Преграда закрытого шлагбаума: персонажей не пускает, из навигационной сетки проезд вырезает.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Barrier")
	UBoxComponent* Blocker;

private:
	UFUNCTION()
	void HandleQuestChanged(const FQuest& Quest);

	void HandleJournalRestored();

	// Найти героя и подписаться на его журнал (герой может появиться позже шлагбаума).
	void BindToPlayerJournal(APlayerCharacter* Player);
	void UnbindFromJournal();

	// bInstant — без движения стрелы (первая сверка: после загрузки стрела уже стоит как надо).
	void SetOpen(bool bNewOpen, bool bInstant);
	void ApplyBlocker();
	void ApplyBoomPose();

	// Разложить части по размерам (редактор и игра).
	void LayoutParts();

	bool bOpen = false;
	float OpenAlpha = 0.0f;

	// Первая сверка с журналом уже была — дальше смена состояния идёт с движением стрелы.
	bool bResolvedOnce = false;

	// Шлагбаум сейчас открыт ради выпуска героя изнутри (задания его не открывают).
	bool bLettingHeroOut = false;

	// Пересчитать выпуск по месту героя; возвращает новое значение bLettingHeroOut.
	bool UpdateLetHeroOut(const APlayerCharacter* Player);

	TWeakObjectPtr<UQuestComponent> BoundJournal;
	FDelegateHandle JournalRestoredHandle;
	FTimerHandle CheckTimer;
};
