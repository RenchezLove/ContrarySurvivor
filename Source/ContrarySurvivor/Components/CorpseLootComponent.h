// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CorpseLootComponent.generated.h"

class AMasterInventoryItem;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;
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
 *
 * ОТЧЁТ РИНАТА 23.08.2026 (п.5, растворение): «после обыска трупы сразу становились бы на
 * 35% более прозрачными и на 35% более серыми, а затем постепенно исчезали, становясь более
 * прозрачными, пока вовсе не будут удалены из сцены». Растворение — новый ОСНОВНОЙ способ
 * убирания обысканного тела (bDissolveWhenSearched, по умолчанию включён); уход в землю
 * остался запасным путём — на него тело откатывается, если материал растворения не задан
 * или не загрузился. Оба пути живут под общим выключателем bSinkWhenSearched, поэтому
 * мешок-пикап (у него выключено) ни в землю не уходит, ни не растворяется.
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

	// ВЫБРОСИТЬ содержимое БЕЗ обыска («Новая игра» посреди живого мира, баг Рината 23.08):
	// скрытые акторы-данные уничтожаются (паттерн EndPlay), деньги в ноль. Сам контейнер
	// живёт дальше — база наполнит его заново при следующей зачистке.
	void ClearLoot();

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

	// ADR-076 п.2: ОТДЕЛЬНОЕ ХРАНИЛИЩЕ — группу вокруг себя не собирает (якорь-хранилище =
	// своё окно один на один). Ставят база (Report1 п.11 — лут в меше логова/сарая) и машина
	// (Report1 п.9). В ЧУЖУЮ группу не входит ТОЛЬКО пока StashLevel = 0 (машина): хранилище
	// БАЗЫ (StashLevel > 0) с 24.08 подтягивается в группу якоря-трупа/мешка — Report1 баг 1,
	// решение лида по ADR-077 п.14 (награда базы в одном окне с трупами, строка базы выделена).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot", meta = (DisplayPriority = "3",
		DisplayName = "Отдельное хранилище (своё окно обыска)",
		ToolTip = "Группу вокруг себя не собирает: при прямом обыске открывается своё окно. Если это база («Уровень базы» больше нуля) — её содержимое дополнительно попадает в общее окно обыска трупов рядом."))
	bool bStandaloneStash = false;

	// Report1 пп.11+14: уровень базы-хранилища (0 = обычный контейнер). Больше нуля — окно
	// обыска пишет название этого контейнера ОТДЕЛЬНЫМ выделенным кубиком с припиской
	// «Ур. N» (стиль выделения — в ассете окна, формат приписки — в его Class Defaults).
	// Ставит база при наполнении хранилища (зачищенная ступень).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CorpseLoot", meta = (DisplayPriority = "4",
		DisplayName = "Уровень базы (0 = не база)"))
	int32 StashLevel = 0;

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

	// --- Пауза исчезновения на время открытого окна обыска (ADR-082 п.6, лид 29.08:
	// «связка окон не должна ронять тело по таймеру, пока игрок стоит с открытым окном») ---

	// Запоминает остаток LifeSpan владельца (0 = таймера и не было) и ставит LifeSpan в 0
	// (движок: 0 = не исчезать). Повторный вызов, пока пауза уже активна, ничего не
	// перезаписывает — так несколько вложенных открытий окна не теряют исходный остаток.
	void PauseLifeSpanForSearchWindow();

	// Возвращает LifeSpan к запомненному остатку (окно закрылось). Остаток был <= 0
	// (таймера не было) или паузы не было вовсе — ничего не делает.
	void ResumeLifeSpanAfterSearchWindow();

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
	// «отдельное хранилище» (bStandaloneStash) в группу не входят — КРОМЕ хранилищ БАЗ
	// (StashLevel > 0, Report1 баг 1 + решение лида 24.08 по ADR-077 п.14: награда базы в
	// одном окне с трупами вокруг). Якорь-хранилище (база/машина) по-прежнему возвращает
	// группу из одного себя. Функция чистая (реестры и расстояния) — проверяется
	// автотестом без живой сцены.
	static TArray<UCorpseLootComponent*> CollectSearchableGroup(const AActor* AnchorActor, float GroupRadius);

	// --- Обысканное тело убирается из мира: растворение (Ринат 23.08, п.5) или уход в
	//     землю (издатель 11.08.2026, п.3.2) ---

	// Общий выключатель убирания. Для контейнеров, которые телами не являются: мешок-пикап
	// исчезает своим путём (APickup::HandleLootChanged), ему ни растворение, ни погружение
	// не нужны. Имя поля историческое (когда путь был один — в землю); не переименовывать:
	// на нём сидят сохранённые значения BP.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Исчезновение", meta = (DisplayPriority = "1",
		DisplayName = "Обысканное тело убирается из мира",
		ToolTip = "Только для ТЕЛ. У мешка-пикапа выключено: он исчезает сам, когда его обчистили. Способ убирания выбирает галочка «Растворение вместо ухода в землю»."))
	bool bSinkWhenSearched = true;

	// Отчёт Рината 23.08 (п.5). Включено — тело растворяется (нужен материал растворения
	// ниже; не задан/не загрузился — тело уйдёт в землю по-старому). Выключено — уход в землю.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Исчезновение", meta = (DisplayPriority = "2",
		DisplayName = "Растворение вместо ухода в землю",
		ToolTip = "Тело сразу становится прозрачнее и серее, затем плавно растворяется до полного исчезновения. Требует назначенный «Материал растворения»."))
	bool bDissolveWhenSearched = true;

	// Материал, которым тело рисуется во время растворения (подменяет ВСЕ материалы мешей
	// тела и привязанного к нему оружия). Контракт: скалярные параметры Opacity (множитель
	// непрозрачности, 1 → 0) и Desaturation (0..1, серость), цвет — из вершинной покраски
	// (как у волка и бандита). По умолчанию — M_CorpseDissolve из Content/Characters/Shared.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Исчезновение", meta = (DisplayPriority = "3",
		DisplayName = "Материал растворения",
		ToolTip = "Материал с параметрами Opacity и Desaturation, которым тело рисуется, пока растворяется. Пусто или не загрузился — тело уходит в землю по-старому."))
	TSoftObjectPtr<UMaterialInterface> DissolveMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Исчезновение", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayPriority = "4",
		DisplayName = "Сразу после обыска: прозрачность (доля)",
		ToolTip = "Насколько тело становится прозрачнее СРАЗУ после обыска. 0.35 = на 35% (значение Рината)."))
	float DissolveInstantTransparency = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Исчезновение", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayPriority = "5",
		DisplayName = "Сразу после обыска: серость (доля)",
		ToolTip = "Насколько тело становится серее СРАЗУ после обыска. 0.35 = на 35% (значение Рината)."))
	float DissolveInstantDesaturation = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Исчезновение", meta = (ClampMin = "0.0", DisplayPriority = "6",
		DisplayName = "Пауза до начала растворения (сек)",
		ToolTip = "Сколько тело лежит полупрозрачно-серым, прежде чем начнёт плавно растворяться."))
	float DissolveDelay = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Исчезновение", meta = (ClampMin = "0.0", DisplayPriority = "7",
		DisplayName = "Длительность растворения (сек)",
		ToolTip = "За сколько секунд тело плавно доходит до полной прозрачности и удаляется из сцены."))
	float DissolveDuration = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Исчезновение", meta = (ClampMin = "0.0", DisplayPriority = "8",
		DisplayName = "Пауза перед уходом в землю (сек)",
		ToolTip = "Запасной путь без растворения: сколько тело лежит обысканным, прежде чем начнёт опускаться."))
	float SearchedSinkDelay = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Исчезновение", meta = (ClampMin = "0.0", DisplayPriority = "9",
		DisplayName = "Длительность ухода в землю (сек)"))
	float SearchedSinkDuration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Исчезновение", meta = (ClampMin = "0.0", DisplayPriority = "10",
		DisplayName = "Глубина ухода в землю (см)"))
	float SearchedSinkDepth = 150.0f;

	// Тело обыскали до конца — убрать его из мира: растворение (Ринат 23.08 п.5), а без
	// материала или галочки — уход в землю. Зовётся само из TakeMoney/TakeItem; публично —
	// для автотестов и для случая, когда лут вычерпали в обход окна.
	void StartSearchedSink();

	bool IsSinking() const { return bSinking; }
	bool IsDissolving() const { return bDissolving; }

	// Чистая математика растворения: множитель непрозрачности тела к моменту Elapsed.
	// В нулевой момент — 1-InstantTransparency (мгновенный сдвиг Рината «сразу на 35%»),
	// до Delay держится, дальше равномерно до нуля за Duration. Проверяется автотестом
	// без живой сцены (паттерн GetSinkDepthAtTime).
	static float GetDissolveOpacityAtTime(float Elapsed, float InstantTransparency,
		float Delay, float Duration);

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

	// Растворение (Ринат 23.08 п.5): идёт ли и сколько секунд прошло. Живой материал с
	// параметрами Opacity/Desaturation один на все слоты всех мешей трупа (вершинный цвет
	// каждый меш даёт свой); UPROPERTY — защита от GC на время растворения.
	bool bDissolving = false;
	float DissolveElapsed = 0.0f;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DissolveMID;

	// Кэш ОРИГИНАЛЬНЫХ материалов до подмены на DissolveMID (ADR-082 п.7, лид 29.08: «если
	// игрок положил вещь обратно ПОСЛЕ начала растворения, добыча не должна оказаться в
	// исчезающем теле»). Три массива идут ПАРАЛЛЕЛЬНО (индекс i — один слот одного меша),
	// заполняются в StartSearchedDissolve, возвращаются на место и чистятся в
	// CancelSearchedRemoval. UPROPERTY на мешах/материалах — защита от GC, пока кэш жив.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMeshComponent>> DissolveOriginalMeshes;
	TArray<int32> DissolveOriginalSlotIndices;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> DissolveOriginalMaterials;

	// Запуск растворения; false — материал не задан или не загрузился (тело уйдёт в землю).
	bool StartSearchedDissolve();

	// Отменяет начатое убирание (ADR-082 п.7): возвращает оригинальные материалы (растворение)
	// или высоту и гравитацию рэгдолла (уход в землю), сбрасывает bSinking/bDissolving и
	// выключает тик. Зовётся из AddLoot, когда в контейнер, уже начавший исчезать, снова
	// попал предмет или деньги. Ничего не начато — тихо выходит.
	void CancelSearchedRemoval();

	// Меш, упавший рэгдоллом: его физические тела живут в МИРОВЫХ координатах и за актором
	// не едут, поэтому такому мешу двигаем сами тела. Пусто — меш обычный, хватит переноса актора.
	TWeakObjectPtr<USkeletalMeshComponent> SinkRagdollMesh;

	// Конец погружения: убрать тело и то, что к нему привязано (оружие в руке).
	void FinishSearchedSink();

	// Остаток LifeSpan владельца на момент постановки на паузу окном обыска (ADR-082 п.6).
	// -1 = паузы сейчас нет; 0 = пауза была, но таймера и так не было (восстанавливать нечего);
	// >0 = сколько секунд оставалось — вернуть при ResumeLifeSpanAfterSearchWindow.
	float SavedLifeSpanOnWindowOpen = -1.0f;

	static TArray<TWeakObjectPtr<UCorpseLootComponent>> SearchableCorpses;
};
