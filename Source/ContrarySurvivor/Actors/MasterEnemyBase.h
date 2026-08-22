// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"
#include "Misc/DateTime.h"
#include "ContrarySurvivor/Actors/Pickup.h" // FPlacedLootEntry — состав награды ступени (единый формат DT_Items/класс)
#include "MasterEnemyBase.generated.h"

class ACharacter;
class AMasterInventoryItem;
class USceneComponent;
class USphereComponent;
class USoundBase;
class UAudioComponent;
class UEnemySpawnPointComponent;
class UBaseEntryAnnounceWidget;

/**
 * Занятость базы (ТЗ издателя 22.08.2026 «возрождение базы и её уровни»):
 *  Dormant  — взведена, врагов нет: спавн произойдёт при входе игрока в радиус активации;
 *  Occupied — враги заспавнены, хотя бы один жив;
 *  Cleared  — полная зачистка: тикает пауза возрождения ПО РЕАЛЬНЫМ ЧАСАМ; истекла и игрок
 *             ушёл из радиуса — база снова Dormant (враги не появляются на глазах).
 */
UENUM()
enum class EEnemyBaseOccupancy : uint8
{
	Dormant,
	Occupied,
	Cleared
};

/**
 * Награда за ПОЛНУЮ зачистку одной ступени (ТЗ 22.08 §4): деньги + предметы в едином
 * формате мешка (строка таблицы предметов DT_Items ЛИБО класс + количество — тот же
 * FPlacedLootEntry, что у размещаемого пикапа; читает единый конфиг, своих чисел рядом
 * не заводит). По зачистке падает мешком-пикапом в центре базы.
 */
USTRUCT(BlueprintType)
struct FEnemyBaseTierReward
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward", meta = (ClampMin = "0.0", DisplayPriority = "1",
		DisplayName = "Деньги в награде"))
	float Money = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reward", meta = (DisplayPriority = "2", TitleProperty = "ItemRow",
		DisplayName = "Предметы награды"))
	TArray<FPlacedLootEntry> Items;

	// Есть ли в награде хоть что-то (ТЗ: «пусто не бывает никогда» — пустая запись
	// откатывается на награду первой ступени, см. PickRewardTierIndex).
	bool IsEmpty() const { return Money <= 0.0f && Items.Num() == 0; }
};

/**
 * Размещаемая зона спавна врагов «по приближению» (A5: замена UWolfSpawnSubsystem +
 * UBanditSpawnSubsystem на один параметризуемый АКТОР, ADR-024 «BP-наследник C++»).
 *
 * ДИЗАЙН (как у прежних сабсистем, поведение НЕ меняем): деревня — мирная зона, враги ждут
 * в своей зоне (логово/база) и спавнятся ОДНОРАЗОВО, когда игрок ВПЕРВЫЕ подходит на
 * ActivationRadius (горизонтальная XY-дистанция до АКТОРА). Центр зоны = GetActorLocation()
 * размещённого актора (хардкод координат ±3500 убран — место задаёт оператор расстановкой).
 *
 * Активация: повторяющийся таймер (ActivationCheckPeriod) проверяет дистанцию игрока; при
 * входе в радиус — пауза SpawnDelay (Nav Invoker достраивает тайлы), затем спавн врагов,
 * навмеш-проекция + floor-trace Z (SpawnPlacement). Опц. квест-предмет (ноутбук кв.2) кладётся
 * пикапом в центре.
 *
 * ТОЧКИ СПАВНА (задача Рината): позиции врагов берутся из компонентов-маркеров
 * UEnemySpawnPointComponent, которые дизайнер расставляет/двигает во вьюпорте BP (видимые
 * перемещаемые стрелки = позиция + направление врага).
 *
 * ЧИСЛО ВРАГОВ НЕЗАВИСИМО ОТ ЧИСЛА ТОЧЕК (ADR-029, фидбек Рината 2026-06-25): дизайнер держит
 * в BP БОЛЬШЕ точек, чем нужно (ёмкость, ориентир ~7), а отдельный параметр NumToSpawn задаёт,
 * сколько врагов реально спавнить. Спавнятся min(NumToSpawn, число точек) врагов в СЛУЧАЙНОМ
 * подмножестве размещённых точек. Так число врагов меняется на КАЖДОМ размещённом экземпляре
 * базы одним числом, без добавления/удаления точек. Если в BP не размещено ни одной точки —
 * fallback на старое поведение: NumToSpawn врагов по кругу (SpreadRadius) вокруг центра актора.
 * C++ создаёт одну дефолтную точку, чтобы базовый актор уже имел видимый перемещаемый маркер.
 *
 * BP-наследники (editor): BP_WolfDen (EnemyClass=BP_Wolf, 4 точки, запад),
 * BP_BanditBase (EnemyClass=BP_EnemyBandit, 3 точки, bSpawnQuestItem, север). Классы и точки
 * задаёт BP — в C++ нейтральные дефолты (EnemyClass пуст, одна точка-образец).
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AMasterEnemyBase : public AActor
{
	GENERATED_BODY()

public:
	AMasterEnemyBase();

	// Тег цели квеста этой базы (читает HUD для метки на цель активного квеста).
	FName GetQuestMarkerTag() const { return QuestMarkerTag; }

	// --- Чистые правила ТЗ 22.08 (статики — их гоняет headless-тест, база зовёт их же) ---

	// Текст надписи при входе (§5): 1-я ступень — только название; 2-я — название + фраза
	// «более опытные»; 3-я и выше — название + фраза «ещё более опытные». Пустая фраза или
	// пустое название — что есть, то и показываем (без мусора из разделителей).
	static FText BuildAnnounceText(const FText& BaseName, const FText& Separator,
		const FText& Tier2Suffix, const FText& Tier3PlusSuffix, int32 Tier);

	// Индекс записи награды для зачищенной ступени Tier (§4 «пусто не бывает»): запись
	// ступени; пустая — запись ПЕРВОЙ ступени; и она пустая/массива нет — INDEX_NONE
	// (вызывающий громко предупреждает и мешок не спавнит).
	static int32 PickRewardTierIndex(const TArray<FEnemyBaseTierReward>& Rewards, int32 Tier);

	// Текущая ступень — для автотестов/отладки.
	int32 GetCurrentTierForQA() const { return CurrentTier; }
	EEnemyBaseOccupancy GetOccupancyForQA() const { return Occupancy; }

protected:
	virtual void BeginPlay() override;

	// Подгоняет радиусы сфер-визуализаторов под текущие значения полей — чтобы границы
	// активации (ActivationRadius) и поводка (LeashRadius) обновлялись во вьюпорте сразу при
	// правке в Details (и в превью BP, и на размещённом акторе). См. ActivationVisualizer /
	// LeashVisualizer.
	virtual void OnConstruction(const FTransform& Transform) override;

	// Корень-трансформ (placeable). Меш/иконку задаёт BP при желании.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "EnemyBase")
	USceneComponent* SceneRoot;

	// Визуализатор радиуса активации (ADR-029, фидбек Рината): сфера-каркас радиусом
	// ActivationRadius, видимая во вьюпорте редактора (превью BP И размещённый на уровне актор),
	// скрытая в игре (bHiddenInGame у UShapeComponent = true по умолчанию). Коллизии/навмеша нет —
	// чистая визуальная подсказка «где граница, при пересечении которой спавнятся враги». Радиус
	// синхронизируется с ActivationRadius в OnConstruction.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "EnemyBase")
	USphereComponent* ActivationVisualizer;

	// Визуализатор радиуса поводка (фидбек Рината 07-05): вторая каркас-сфера, радиус = LeashRadius,
	// цвет фиолетовый (отличается от оранжевой границы активации). Как ActivationVisualizer:
	// видна только в редакторе (bHiddenInGame), без коллизии/навмеша, ОТДЕЛЬНОГО параметра радиуса
	// НЕТ — радиус синхронизируется с LeashRadius в OnConstruction. LeashRadius=0 — сфера в точку.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "EnemyBase")
	USphereComponent* LeashVisualizer;

	// Дефолтная точка спавна — образец, чтобы у базового актора уже был видимый перемещаемый
	// маркер. Дизайнер двигает её и/или добавляет ещё точек (Enemy Spawn Point) в дереве BP.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "EnemyBase")
	UEnemySpawnPointComponent* DefaultSpawnPoint;

	// Класс врага для спавна (BP_Wolf / BP_EnemyBandit назначает оператор в BP-наследнике).
	// Нейтральный дефолт — пусто (без BP спавна не будет; класс не хардкодим в C++).
	// meta DisplayPriority — поднять наши тюнинг-настройки наверх Details (фидбек Рината), сразу после Transform.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase", meta = (DisplayPriority = "1"))
	TSubclassOf<ACharacter> EnemyClass;

	// Дистанция спавна (см): первый вход игрока (XY) в этот радиус от актора → спавн. Чем больше,
	// тем дальше игрок, когда враги появляются (чтобы не видеть спавн вблизи). Тюнинг per-instance
	// в BP. 3500 ≈ 35 м (спавн вне зоны видимости, до подхода игрока к базе). Граница видна во
	// вьюпорте сферой ActivationVisualizer.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase", meta = (ClampMin = "100.0", UIMin = "100.0", DisplayPriority = "2"))
	float ActivationRadius = 3500.0f;

	// Сколько врагов реально спавнить при активации (ADR-029). НЕЗАВИСИМО от числа размещённых
	// точек: берётся min(NumToSpawn, число точек) случайных точек. Если точек нет вовсе — fallback:
	// NumToSpawn врагов по кругу (SpreadRadius). Дефолт C++ = 3; в BP_WolfDen=4, BP_BanditBase=3.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase", meta = (ClampMin = "1", UIMin = "1", DisplayPriority = "3"))
	int32 NumToSpawn = 3;

	// D7 (ADR-036): радиус поводка (см) — как далеко враги ЭТОЙ базы гонятся за игроком от её
	// центра; дальше — разворачиваются и возвращаются. Передаётся контроллеру каждого
	// заспавненного врага (SetLeash). 0 = поводок выключен. «Часто граница поводка и граница
	// деревни совпадают» — совмещение подбирается ЭТИМ числом на размещённом экземпляре.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase", meta = (ClampMin = "0.0", DisplayPriority = "4"))
	float LeashRadius = 2500.0f;

	// D6/квест-метка (Этап D): тег цели квеста. HUD находит базу по совпадению с
	// FQuest::MapMarkerTag активного квеста и рисует метку/краевую стрелку на неё.
	// Выставляется в BP-наследниках: BP_WolfDen="WolfDen", BP_BanditBase="BanditBase".
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase", meta = (DisplayPriority = "5"))
	FName QuestMarkerTag;

	// FALLBACK: радиус круговой раскладки врагов вокруг центра (см). Используется ТОЛЬКО если в BP
	// не размещено ни одной точки спавна (иначе позиции берутся из точек, см. NumToSpawn).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Fallback")
	float SpreadRadius = 400.0f;

	// Задержка (сек) между входом игрока в радиус и фактическим спавном — Nav Invoker на игроке
	// успевает достроить навмеш-тайлы вокруг зоны (бандиты/волки садятся navmesh=yes). Тюнинг в BP.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase")
	float SpawnDelay = 0.5f;

	// Период проверки дистанции игрока до зоны (сек).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase")
	float ActivationCheckPeriod = 0.5f;

	// --- Опц. квест-предмет (напр. «Ноутбук» на базе бандитов, кв.2 старосты) ---

	// Класть ли квест-предмет в центре зоны при активации (BP_BanditBase ставит true).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Quest")
	bool bSpawnQuestItem = false;

	// Класс квест-предмета (дефолт AQuestItem — категория Quest, не теряется при смерти).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Quest")
	TSubclassOf<AMasterInventoryItem> QuestItemClass;

	// Класс пикапа-носителя (дефолт APickup).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Quest")
	TSubclassOf<APickup> PickupClass;

	// СЛУЖЕБНЫЙ КЛЮЧ квест-предмета. ОБЯЗАН посимвольно совпадать с RequiredItemName квеста
	// старосты (ElderNPC.cpp:72) — иначе ноутбук не засчитается. НЕ переводить (ADR-050).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Quest")
	FString QuestItemName = TEXT("Ноутбук");

	// ПЕРЕВОДИМОЕ название того же предмета, которое видит игрок в рюкзаке.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Quest")
	FText QuestItemText = NSLOCTEXT("Items", "QuestLaptop", "Ноутбук");

	// === ВОЗРОЖДЕНИЕ И СТУПЕНИ (ТЗ издателя 22.08.2026, бэклог №31/№35). Всё EditAnywhere —
	// настраивается и в BP базы, и на размещённом экземпляре (требование ТЗ дословно). ===

	// Начальная ступень базы (1..5). Память зачисток в сейве главнее этого поля.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Возрождение", meta = (ClampMin = "1", ClampMax = "5", DisplayPriority = "1",
		DisplayName = "Начальная ступень (1..5)"))
	int32 InitialTier = 1;

	// Паузы до возрождения ПО ЗАЧИЩЕННОЙ ступени, минуты РЕАЛЬНОГО времени (ТЗ §1:
	// 15/30/45/55/60). Массив короче пяти — старшие ступени живут по последнему значению.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Возрождение", meta = (DisplayPriority = "2",
		DisplayName = "Паузы возрождения по ступеням, мин"))
	TArray<float> RespawnPauseMinutesPerTier = { 15.0f, 30.0f, 45.0f, 55.0f, 60.0f };

	// Прибавка здоровья врагам по ступеням (ТЗ §3: +0/12/19/22/27). Приходит ОТ БАЗЫ:
	// применяется к UStatsComponent заспавненного врага, во враге ничего не зашито.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Возрождение", meta = (DisplayPriority = "3",
		DisplayName = "Прибавка здоровья врагам по ступеням"))
	TArray<float> TierHealthBonus = { 0.0f, 12.0f, 19.0f, 22.0f, 27.0f };

	// Идентификатор базы в сейве (пусто = имя размещённого актора — стабильно для акторов
	// с карты). Задать руками, если базу когда-нибудь переименуют, чтобы память не потерялась.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Возрождение", meta = (DisplayPriority = "4",
		DisplayName = "Идентификатор базы в сейве (пусто = имя актора)"))
	FName BaseSaveId;

	// === НАГРАДА ЗА ЗАЧИСТКУ (ТЗ §4): мешок в центре базы. Дефолты — конструктор
	// (1-2 ступени расходники, 3+ броня по строкам DT_Items). ===

	// Награда по ступеням (запись 1 = ступень 1 …). Пустая запись ступени — фолбэк на
	// первую («пусто не бывает никогда»).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Награда", meta = (DisplayPriority = "1",
		DisplayName = "Награда по ступеням (мешок в центре)"))
	TArray<FEnemyBaseTierReward> TierRewards;

	// Класс мешка награды (пусто = класс пикапа базы PickupClass).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Награда", meta = (DisplayPriority = "2",
		DisplayName = "Класс мешка награды"))
	TSubclassOf<APickup> RewardPickupClass;

	// === НАДПИСЬ ПРИ ВХОДЕ (ТЗ §5): показывается при входе игрока в радиус активации.
	// Тексты настраиваемые — «их будут править». Виджет кодовый, ассета нет. ===

	// Название места («Лагерь бандитов» / «Логово волков») — ставится в BP наследника.
	// Пусто (голый C++-актор) — надпись не показывается вовсе.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Надпись", meta = (DisplayPriority = "1",
		DisplayName = "Название места"))
	FText BaseDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Надпись", meta = (DisplayPriority = "2",
		DisplayName = "Фраза 2-й ступени"))
	FText Tier2Suffix = NSLOCTEXT("EnemyBase", "Tier2Suffix", "Эти выглядят более опытными");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Надпись", meta = (DisplayPriority = "3",
		DisplayName = "Фраза 3-й ступени и выше"))
	FText Tier3PlusSuffix = NSLOCTEXT("EnemyBase", "Tier3PlusSuffix", "Эти выглядят ещё более опытными");

	// Разделитель между названием и фразой («Лагерь бандитов. Эти выглядят…»).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Надпись", meta = (DisplayPriority = "4",
		DisplayName = "Разделитель"))
	FText AnnounceSeparator = NSLOCTEXT("EnemyBase", "AnnounceSeparator", ". ");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Надпись", meta = (ClampMin = "1.0", DisplayPriority = "5",
		DisplayName = "Сколько секунд видна надпись"))
	float AnnounceDuration = 5.0f;

	// Антиспам повторных входов: не чаще раза в столько секунд.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Надпись", meta = (ClampMin = "0.0", DisplayPriority = "6",
		DisplayName = "Антиспам надписи, сек"))
	float AnnounceCooldown = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Надпись", meta = (ClampMin = "8", DisplayPriority = "7",
		DisplayName = "Кегль строки"))
	int32 AnnounceFontSize = 22;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Надпись", meta = (DisplayPriority = "8",
		DisplayName = "Цвет строки"))
	FLinearColor AnnounceColor = FLinearColor(0.95f, 0.95f, 0.95f, 1.0f);

	// Номер ступени «полупрозрачной цифрой, без скобок, вторым планом» (ТЗ §5) — с 3-й ступени.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Надпись", meta = (ClampMin = "16", DisplayPriority = "9",
		DisplayName = "Кегль цифры ступени"))
	int32 AnnounceDigitFontSize = 96;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Надпись", meta = (DisplayPriority = "10",
		DisplayName = "Цвет цифры ступени (полупрозрачный)"))
	FLinearColor AnnounceDigitColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.18f);

	// === ЗВУК БАЗЫ (ТЗ §5 «звук раньше картинки»): зацикленный шум занятой базы, громче
	// со ступенью. Ассета «голоса/лай» в проекте нет — поле пустое, назначит оператор;
	// пусто = базы не слышно (без крашей). ===

	// Зацикленный звук занятой базы (мягкая ссылка, паттерн звуков проекта).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Звук", meta = (DisplayPriority = "1",
		DisplayName = "Звук занятой базы (зацикленный)"))
	TSoftObjectPtr<USoundBase> AmbientSound;

	// Радиус слышимости, см (ТЗ: настраивается в BP и на экземпляре). Применяется
	// перекрытием затухания аудио-компонента, живой в OnConstruction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Звук", meta = (ClampMin = "100.0", DisplayPriority = "2",
		DisplayName = "Радиус слышимости, см"))
	float AmbientHearRadius = 4000.0f;

	// Громкость по ступеням («чем выше ступень, тем больше голосов слышно»).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Звук", meta = (DisplayPriority = "3",
		DisplayName = "Громкость по ступеням"))
	TArray<float> TierAmbientVolume = { 0.4f, 0.55f, 0.7f, 0.85f, 1.0f };

	// Аудио-компонент шума базы (звук и радиус — поля выше).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "EnemyBase|Звук")
	UAudioComponent* AmbientAudio;

private:
	FTimerHandle ActivationTimerHandle;
	FTimerHandle SpawnDelayTimerHandle;

	// --- Рантайм возрождения (ТЗ 22.08). Таймер ActivationTimerHandle теперь НЕ гасится
	// после активации: он же следит за зачисткой и за истечением паузы возрождения. ---

	// Занятость базы (см. EEnemyBaseOccupancy).
	EEnemyBaseOccupancy Occupancy = EEnemyBaseOccupancy::Dormant;

	// Текущая ступень (1..5). Инициализируется InitialTier, перекрывается памятью сейва.
	int32 CurrentTier = 1;

	// Какая ступень была зачищена последней (по ней считается пауза) и когда (UTC).
	// Ticks == 0 — зачисток ещё не было.
	int32 LastClearedTier = 0;
	FDateTime LastClearUtc;

	// Спавн уже запланирован (SpawnDelay тикает) — не планировать второй.
	bool bSpawnScheduled = false;

	// Игрок был внутри радиуса на прошлом тике (ловим ГРАНЬ входа для надписи).
	bool bPlayerInsideRadius = false;

	// Мировое время последнего показа надписи (антиспам AnnounceCooldown).
	float LastAnnounceTime = -1.0e6f;

	// Заспавненные этой базой враги (слабые ссылки: труп исчез — ссылка отмерла = «мёртв»).
	TArray<TWeakObjectPtr<ACharacter>> SpawnedEnemies;

	// Виджет надписи входа (создаётся при первом показе, переиспользуется).
	TWeakObjectPtr<UBaseEntryAnnounceWidget> AnnounceWidget;

	// Тик машины состояний (по таймеру): вход игрока/спавн, зачистка, истечение паузы.
	void CheckActivation();

	// Действующий идентификатор базы в сейве (BaseSaveId, пусто — имя актора).
	FName ResolveBaseSaveId() const;

	// Прочитать ступень/время зачистки из слота сейва (BeginPlay, до взведения).
	void LoadTierStateFromSave();

	// Записать ступень/время зачистки в слот (retention-паттерн: загрузил → правлю ТОЛЬКО
	// свою запись → сохранил; поля игрока/удержания не трогаются).
	void WriteTierStateToSave() const;

	// Все заспавненные враги мертвы/исчезли (и был хотя бы один)?
	bool AreAllSpawnedEnemiesDead() const;

	// Полная зачистка: ступень вверх (5 → 3), время в сейв, мешок-награда, тишина.
	void HandleBaseCleared();

	// Мешок-награда зачищенной ступени в центре базы (ТЗ §4; «пусто не бывает»).
	void SpawnRewardBag(int32 ClearedTier);

	// Надпись при входе (ТЗ §5): строка по ступени + полупрозрачная цифра с 3-й.
	void ShowEntryAnnounce();

	// Звук базы под текущее состояние: занята — играет с громкостью ступени, иначе тихо.
	void UpdateAmbientForState();

	// Отложенный спавн (после паузы на построение навмеша): враги + опц. квест-предмет.
	void DoSpawn();

	// Спавнит врагов (ADR-029): min(NumToSpawn, число точек) врагов в случайном подмножестве
	// размещённых точек спавна (UEnemySpawnPointComponent); если точек нет — fallback на круговую
	// раскладку NumToSpawn вокруг центра.
	void SpawnEnemies();

	// Собирает мировые трансформы всех размещённых точек спавна (UEnemySpawnPointComponent).
	// Пусто, если дизайнер не оставил ни одной точки в BP (тогда работает fallback).
	void CollectSpawnPointTransforms(TArray<FTransform>& OutTransforms) const;

	// Спавнит одного врага в позиции SpawnTransform (XY на навмеш + floor-trace Z, Yaw из точки).
	void SpawnOneEnemy(const FTransform& SpawnTransform);

	// Спавнит пикап с квест-предметом в центре зоны (если bSpawnQuestItem).
	void SpawnQuestItem();
};
