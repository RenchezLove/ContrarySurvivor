// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CorpseLootComponent.generated.h"

class AMasterInventoryItem;
class USkeletalMeshComponent;

/**
 * Контейнер обыскиваемого лута (Build 1.2.1, ТЗ А1 «Обыск трупов»; Build 1.2.2 — ещё и
 * мешок-пикап, см. APickup).
 *
 * При смерти враг больше не роняет мешок-пикап: деньги и предметы складываются СЮДА
 * (InitLoot из HandleDeath/DropLoot), труп становится «обыскиваемым» — контроллер
 * показывает подсказку «Обыскать [E]» и по E открывает окно UCorpseLootWidget.
 * Частичный обыск штатен: забранное уходит игроку, остаток живёт в компоненте до
 * исчезновения трупа (SetLifeSpan владельца, CorpseLifeSpan врага).
 *
 * Компонент создаётся в КОНСТРУКТОРАХ мастер-классов врагов (AEnemyCharacter,
 * AWolfCharacter) — BP-наследники получают механизм автоматически (ТЗ А1). Build 1.2.2
 * (Ринат: «нужно, чтобы был BP_Picup с механикой, похожей на обыск ящика или трупа»):
 * такой же контейнер несёт и APickup, поэтому мешок на земле открывает ТО ЖЕ окно.
 *
 * Находимость для контроллера — статический реестр «обыскиваемых трупов» (паттерн
 * AEnemyAIController::GetActiveControllers): регистрация в InitLoot (труп появился),
 * снятие в EndPlay (труп исчез по таймеру / конец мира). ПИКАП в реестр НЕ встаёт
 * (bRegisterSearchable=false): его контроллер и так находит перебором APickup, а вторая
 * регистрация дала бы один и тот же мешок двумя интерактивами. Предметы-лут — скрытые
 * акторы-данные; не забранные к EndPlay уничтожаются.
 *
 * ЗАДАНИЕ ИЗДАТЕЛЯ 11.08.2026 (групповой обыск тел) добавило сюда две вещи:
 *  - п.3.1 «обыск всей кучи одним нажатием»: CollectSearchableGroup — тела рядом с
 *    подсвеченным телом, которые уйдут в одно окно обыска. Реестр обыскиваемых и есть
 *    граница «только тела»: ящики, схроны и мешки в нём не стоят;
 *  - п.3.2 «обысканное тело меняется на вид»: опустевшее ТЕЛО уходит в землю и исчезает
 *    (StartSearchedSink), а мешок-пикап — нет (у него bSinkWhenSearched выключен, он и
 *    так уничтожает себя сам по OnLootChanged).
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CONTRARYSURVIVOR_API UCorpseLootComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCorpseLootComponent();

	// Положить лут в труп (зовётся ОДИН раз из HandleDeath врага): содержимое ЗАМЕЩАЕТСЯ.
	// По умолчанию регистрирует владельца в реестре обыскиваемых трупов; пикап зовёт с
	// bRegisterSearchable=false (его контроллер находит своим перебором APickup).
	// Items — скрытые акторы-данные (владение переходит сюда).
	void InitLoot(float InMoney, const TArray<AMasterInventoryItem*>& InItems,
		bool bRegisterSearchable = true);

	// ДОБАВИТЬ лут к уже лежащему (Build 1.2.2): пикап наполняется порциями — сначала
	// поля, расставленные дизайнером на карте (BeginPlay), потом рантайм-дроп
	// (InitLoot/InitLootBag пикапа зовутся уже ПОСЛЕ BeginPlay).
	void AddLoot(float InMoney, const TArray<AMasterInventoryItem*>& InItems,
		bool bRegisterSearchable = true);

	// Содержимое изменилось — из контейнера что-то забрали (Build 1.2.2). Слушает владелец:
	// мешок-пикап исчезает, когда его обчистили до конца. Труп не подписан — он лежит до
	// своего таймера, как и раньше.
	FSimpleMulticastDelegate OnLootChanged;

	// Заголовок окна обыска для ЭТОГО контейнера (пусто = окно берёт свой заголовок
	// «Обыск трупа»). Пикап ставит сюда своё название, чтобы над мешком с вещами не
	// висела надпись про труп.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot", meta = (DisplayPriority = "1",
		DisplayName = "Заголовок окна обыска",
		ToolTip = "Что написано вверху окна обыска для этого контейнера. Пусто = окно возьмёт свой заголовок по умолчанию."))
	FText SearchTitle;

	// ADR-076 п.2: название ЭТОГО объекта в перечне обыскиваемых («Труп волка; Труп волка;
	// Мешок» над окном). Волк/бандит ставят своё в конструкторах (правится в BP врага),
	// мешок-пикап — «Мешок». Пусто = запасное название окна («Труп»).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot", meta = (DisplayPriority = "2",
		DisplayName = "Название для перечня обыскиваемых",
		ToolTip = "Как этот объект называется в строке-перечне над окном обыска (например «Труп волка» или «Мешок»). Пусто = запасное название окна."))
	FText SearchObjectName;

	// ADR-076 п.2 (задел под базы, сам обыск баз не делается): ОТДЕЛЬНОЕ ХРАНИЛИЩЕ — в
	// групповой обыск НЕ входит и группу вокруг себя не собирает: у него своё окно со своим
	// заголовком. Будущий сундук логова/базы ставит эту галочку — дорога открыта.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot", meta = (DisplayPriority = "3",
		DisplayName = "Отдельное хранилище (в групповой обыск не входит)"))
	bool bStandaloneStash = false;

	// Есть ли что забирать (деньги или хоть один живой предмет).
	UFUNCTION(BlueprintPure, Category = "CorpseLoot")
	bool HasLoot() const;

	UFUNCTION(BlueprintPure, Category = "CorpseLoot")
	float GetMoney() const { return Money; }

	// Живые (валидные) предметы трупа — для списка окна обыска.
	TArray<AMasterInventoryItem*> GetLootItems() const;

	// Забрать деньги: возвращает сумму и обнуляет её в трупе.
	float TakeMoney();

	// Забрать предмет: убирает его из списка трупа (true — предмет был здесь).
	// Сам актор НЕ уничтожается — его дальше несёт инвентарь игрока.
	bool TakeItem(AMasterInventoryItem* Item);

	// Реестр обыскиваемых трупов (для UpdateNearbyInteractable контроллера).
	// Слабые ссылки: труп исчезает по таймеру — запись отмирает сама, но чистим в EndPlay.
	static const TArray<TWeakObjectPtr<UCorpseLootComponent>>& GetSearchableCorpses()
	{
		return SearchableCorpses;
	}

	// --- Группа тел под одно нажатие «Обыскать» (издатель 11.08.2026, п.3.1) ---

	// Все НЕобысканные тела в радиусе GroupRadius ОТ ЯКОРЯ (тела ИЛИ мешка, которое
	// подсветила подсказка) — включая сам якорь, он идёт первым. Расстояние меряется от
	// якоря один раз, цепочек «от тела к телу» НЕТ (решение game-lead): иначе одно нажатие
	// выгребало бы половину локации.
	// ADR-076 п.2: МЕШКИ-ПИКАПЫ входят в группу наравне с телами (кейс Рината «в лагере два
	// мешка, обыскиваются по очереди»; якорь-мешок тоже собирает группу — решение лида
	// 22.08). Мешки добираются вторым проходом по реестру живых пикапов (НЕ через реестр
	// тел — там мешок двоил бы интерактив, см. комментарий класса). Контейнеры с галочкой
	// «отдельное хранилище» (bStandaloneStash) в группу не входят, а якорь-хранилище
	// возвращает группу из одного себя. Функция чистая (реестры и расстояния) — проверяется
	// автотестом без живой сцены.
	static TArray<UCorpseLootComponent*> CollectSearchableGroup(const AActor* AnchorActor, float GroupRadius);

	// --- Обысканное тело уходит в землю (издатель 11.08.2026, п.3.2) ---

	// Выключатель для контейнеров, которые телами не являются: мешок-пикап исчезает своим
	// путём (APickup::HandleLootChanged), ему погружение не нужно.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Исчезновение", meta = (DisplayPriority = "1",
		DisplayName = "Обысканное тело уходит в землю",
		ToolTip = "Только для ТЕЛ. У мешка-пикапа выключено: он исчезает сам, когда его обчистили."))
	bool bSinkWhenSearched = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Исчезновение", meta = (ClampMin = "0.0", DisplayPriority = "2",
		DisplayName = "Пауза перед уходом в землю (сек)",
		ToolTip = "Сколько тело лежит обысканным, прежде чем начнёт опускаться."))
	float SearchedSinkDelay = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Исчезновение", meta = (ClampMin = "0.0", DisplayPriority = "3",
		DisplayName = "Длительность ухода в землю (сек)"))
	float SearchedSinkDuration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Исчезновение", meta = (ClampMin = "0.0", DisplayPriority = "4",
		DisplayName = "Глубина ухода в землю (см)"))
	float SearchedSinkDepth = 150.0f;

	// Тело обыскали до конца — запустить погружение. Зовётся само из TakeMoney/TakeItem;
	// публично — для автотестов и для случая, когда лут вычерпали в обход окна.
	void StartSearchedSink();

	bool IsSinking() const { return bSinking; }

	// Чистая математика погружения: на сколько сантиметров тело опустилось к моменту
	// Elapsed. До Delay — ноль, дальше равномерно до Depth за Duration. Проверяется
	// автотестом отдельно от живого мира.
	static float GetSinkDepthAtTime(float Elapsed, float Delay, float Duration, float Depth);

protected:
	// Двигает уходящее в землю тело (тик включается только на время погружения).
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// Труп исчез (LifeSpan/конец мира): снять с реестра, уничтожить не забранные предметы.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// Деньги в трупе (забираются одной кнопкой строки «Деньги»).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CorpseLoot", meta = (AllowPrivateAccess = "true"))
	float Money = 0.0f;

	// Предметы в трупе (скрытые акторы-данные; UPROPERTY — защита от GC до забора).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CorpseLoot", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<AMasterInventoryItem>> Items;

	// Тело уходит в землю: идёт ли погружение, сколько прошло секунд и сколько сантиметров
	// уже отработано (шаг считаем разницей — так тело не «дёргается» при просадке кадров).
	bool bSinking = false;
	float SinkElapsed = 0.0f;
	float SinkAppliedDepth = 0.0f;

	// Меш, упавший рэгдоллом: его физические тела живут в МИРОВЫХ координатах и за актором
	// не едут, поэтому такому мешу двигаем сами тела. Пусто — меш обычный, хватит переноса актора.
	TWeakObjectPtr<USkeletalMeshComponent> SinkRagdollMesh;

	// Конец погружения: убрать тело и то, что к нему привязано (оружие в руке).
	void FinishSearchedSink();

	static TArray<TWeakObjectPtr<UCorpseLootComponent>> SearchableCorpses;
};
