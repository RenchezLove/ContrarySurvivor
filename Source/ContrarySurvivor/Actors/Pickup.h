// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"
#include "Pickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class AMasterInventoryItem;
class APlayerCharacter;
class UCorpseLootComponent;

/**
 * Одна запись СПИСКА содержимого размещённого пикапа (ADR-076 п.10, Ринат дословно: «Я хочу
 * что бы число предметов можно было задать, как и сами предметы. В списке предметов...
 * не нашел воду»). Предмет задаётся ЛИБО строкой таблицы предметов DT_Items (вода уже там —
 * water_bottle; раньше воду нельзя было выбрать вовсе: выбор шёл по классам, а вода — тип
 * расходника, не класс), ЛИБО классом по-старому. Строка главнее класса.
 */
USTRUCT(BlueprintType)
struct FPlacedLootEntry
{
	GENERATED_BODY()

	// Строка таблицы предметов (water_bottle, canned_food, bandage, ammo_9mm, wolf_pelt…).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (DisplayPriority = "1",
		DisplayName = "Предмет (строка таблицы предметов)",
		ToolTip = "Имя строки таблицы предметов DT_Items — так задаются и вода, и консервы, и любой новый предмет без кода. Заполнено — поле класса ниже не смотрится."))
	FName ItemRow;

	// Запасной путь по-старому: класс предмета (для предметов вне таблицы).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (DisplayPriority = "2",
		DisplayName = "…или класс предмета",
		ToolTip = "Класс предмета, если строка таблицы не задана. Работает как прежнее поле «Что лежит внутри»."))
	TSubclassOf<AMasterInventoryItem> ItemClass;

	// Сколько штук этого предмета положить.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = "1", DisplayPriority = "3",
		DisplayName = "Сколько штук",
		ToolTip = "Стакаемые предметы (патроны, расходники, шкуры) лягут ОДНИМ предметом с этим счётчиком, нестакаемые (броня, оружие) — столькими копиями."))
	int32 Count = 1;
};

/**
 * Подбираемый лут (Фаза 4, экономика — GDD §7.8: «враги дают деньги/изношенное оружие»).
 *
 * Один актор-пикап может нести ДЕНЬГИ и/или ПРЕДМЕТЫ. Весь лут лежит в контейнере
 * UCorpseLootComponent — том же самом, что у трупа врага.
 *
 * Build 1.2.2 (Ринат дословно: «Нужно, что бы был BP_Picup с механикой похожей на ту, что
 * я обыскиваю ящик или труп и выбираю что себе положить в инвентарь»): по клавише действия
 * контроллер открывает ТО ЖЕ окно обыска (UCorpseLootWidget), что и у трупа, и игрок
 * забирает содержимое поштучно или кнопкой «Забрать всё». Опустевший мешок исчезает — как
 * раньше исчезал сразу после подбора. Прежний мгновенный забор всем содержимым остался
 * переключателем bInstantCollect на классе (по умолчанию ВЫКЛЮЧЕН).
 *
 * Авто-подбор по overlap УБРАН (был нестабилен — BUG2). Editor-независимо: не Pawn и не
 * несёт UStatsComponent, поэтому НИКОГДА не попадает под авто-лок/таргетинг игрока.
 *
 * Дроп с врага: статический хелпер DropLoot (вызывается из HandleDeath бандита/волка).
 */
// Build 1.2.2: PrioritizeCategories поднимает категорию «Pickup» на самый верх Details —
// дизайнер, поставивший пикап на карту, сразу видит поля наполнения (метаданные класса
// наследуются и BP-наследником: KismetCompiler.cpp:1400-1402 копирует их в BP_Pickup_C).
UCLASS(Blueprintable, meta = (PrioritizeCategories = "Pickup"))
class CONTRARYSURVIVOR_API APickup : public AActor
{
	GENERATED_BODY()

public:
	APickup();

	// Инициализирует лут пикапа (вызывается сразу после спавна). Money — сумма денег,
	// InCarriedItem — предмет (уже заспавненный, скрытый, без коллизии) либо nullptr.
	// Лут кладётся в контейнер обыска (LootContainer).
	void InitLoot(float Money, AMasterInventoryItem* InCarriedItem);

	// A4/ADR-027: «мешок» из НЕСКОЛЬКИХ предметов (дроп расходников при смерти). Предметы уже
	// сняты из рюкзака и скрыты/без коллизии. Ложатся в тот же контейнер обыска; не забранные
	// уничтожаются вместе с мешком (EndPlay контейнера).
	// Build 1.2: Money — деньги в мешке (доля потерянных при смерти монет, переопределение
	// Рината); забираются в окне обыска отдельной плиткой «Деньги».
	void InitLootBag(const TArray<AMasterInventoryItem*>& Items, float Money = 0.0f);

	// МГНОВЕННЫЙ подбор всего содержимого (старое поведение, теперь — только при включённом
	// bInstantCollect): начисляет деньги (UStatsComponent), кладёт предметы в рюкзак
	// (UInventoryComponent), затем уничтожает пикап. Возвращает true ТОЛЬКО если весь лут
	// реально начислен — иначе пикап остаётся на земле (повторная попытка), а не «исчезает
	// без начисления» (фикс BUG2).
	bool Collect(APlayerCharacter* Player);

	// Есть ли в пикапе что подбирать (для контекстной подсказки на HUD).
	bool HasLoot() const;

	// Контейнер содержимого — его же показывает окно обыска (общее с трупом).
	UCorpseLootComponent* GetLootContainer() const { return LootContainer; }

	// ADR-076 п.2: реестр ЖИВЫХ пикапов — групповой обыск добирает мешки отдельным проходом
	// (НЕ через реестр тел: там мешок двоил бы интерактив). Регистрация в BeginPlay,
	// снятие в EndPlay; слабые ссылки отмирают сами.
	static const TArray<TWeakObjectPtr<APickup>>& GetActivePickups()
	{
		return ActivePickups;
	}

	// Открывать ли по действию окно обыска (true, дефолт) или забирать всё сразу (false).
	// Спрашивает контроллер: и для выбора действия, и для текста подсказки.
	bool UsesSearchWindow() const { return !bInstantCollect && LootContainer != nullptr; }

	// Build 1.2.2 (ТЗ Рината про BP_Picup): как игрок забирает содержимое. Выключено (дефолт) —
	// открывается окно обыска, игрок выбирает предметы сам; включено — прежний мгновенный
	// подбор всего разом по клавише действия.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (DisplayPriority = "0",
		DisplayName = "Забирать всё сразу, без окна обыска",
		ToolTip = "Выключено (обычный случай): по клавише действия открывается окно обыска, игрок сам выбирает, что забрать. Включено: всё содержимое уходит в рюкзак одним нажатием, как было раньше."))
	bool bInstantCollect = false;

	// Заголовок окна обыска для этого мешка (у трупа окно называется «Обыск трупа»).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (DisplayPriority = "0",
		DisplayName = "Заголовок окна обыска",
		ToolTip = "Надпись вверху окна обыска этого мешка. Пусто = окно возьмёт свой заголовок по умолчанию."))
	FText SearchWindowTitle = NSLOCTEXT("Pickup", "SearchWindowTitle", "Обыск");

	// Создаёт лут на земле: при необходимости спавнит предмет (по ItemDropChance) и пикап,
	// который несёт MoneyAmount + предмет. Удобный путь для дропа с врага одной строкой.
	// ItemDisplayName (опц.): служебный КЛЮЧ предмета (напр. «Шкура волка») — по нему
	// сходится зачёт квеста, НЕ переводится. ItemDisplayText (опц.): переводимое название
	// того же предмета для показа игроку; пусто — откат на ключ (ADR-050, порция 0).
	// QA force-drop (FQADebug::bForceDrop) поднимает фактический шанс выпадения до 100%.
	// Возвращает заспавненный пикап (или nullptr).
	static APickup* DropLoot(UWorld* World, const FVector& Location, float MoneyAmount,
		TSubclassOf<AMasterInventoryItem> ItemClass, float ItemDropChance,
		TSubclassOf<APickup> PickupClass, const FString& ItemDisplayName = FString(),
		const FText& ItemDisplayText = FText::GetEmpty());

protected:
	virtual void BeginPlay() override;

	// ADR-076 п.2: снятие с реестра живых пикапов (регистрация — в BeginPlay).
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Build 1.2.1 (ТЗ А4): тик нужен ТОЛЬКО пульсации свечения — включается в BeginPlay
	// при включённом свечении с периодом > 0, иначе актор не тикает (как раньше).
	virtual void Tick(float DeltaTime) override;

	// Триггер подбора: overlap по Pawn (как у костра-сейва).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	USphereComponent* PickupTrigger;

	// Визуальный плейсхолдер (без коллизии). Реальный меш/иконку задаёт BP/operator.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	UStaticMeshComponent* MeshComponent;

	// Build 1.2.2: содержимое мешка живёт ЗДЕСЬ — в том же контейнере, что у трупа врага.
	// Одно хранилище на оба пути (окно обыска и мгновенный подбор), поэтому «забрал в окне»
	// и «забрал сразу» не могут разойтись.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	UCorpseLootComponent* LootContainer;

	// Сумма денег в пикапе (0 = нет денег). D8: EditAnywhere — задаётся на РАЗМЕЩЁННОМ
	// экземпляре (лут точек интереса). Это ПОЛЕ-ЗАПОЛНЕНИЕ: в BeginPlay деньги переезжают
	// в контейнер обыска (и поле обнуляется), рантайм-дроп кладёт свои деньги туда же
	// через InitLoot — считать содержимое надо по контейнеру, а не по этому полю.
	// Build 1.2.2 (Ринат: «кнопка не срабатывает»): русские имена и подсказки у всех полей
	// наполнения — пустой пикап (ни денег, ни предмета, ни патронов) клавиша E не видит
	// (HasLoot()==false), поэтому дизайнер должен заполнить хотя бы одно из этих полей.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = "0.0", DisplayPriority = "1",
		DisplayName = "Денег внутри",
		ToolTip = "Сколько денег получит игрок при подборе (0 = денег нет). Пикап, у которого не заполнено НИ ОДНО поле наполнения, кнопка подбора не видит."))
	float MoneyAmount = 0.0f;

	// --- D8: размещаемый лут (заполняет дизайнер на экземпляре на карте) ---
	// BeginPlay спавнит предметы скрытыми (как DropLoot) и складывает их в контейнер
	// обыска вместе с деньгами — дальше всё идёт общим путём (окно обыска или Collect).

	// ADR-076 п.10: СПИСОК содержимого — сколько угодно разных предметов с количествами.
	// Непустой список ГЛАВНЕЕ одиночных полей ниже (те остаются как есть — они уже
	// расставлены на картах, поведение старых пикапов не меняется).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (DisplayPriority = "2",
		TitleProperty = "ItemRow",
		DisplayName = "Содержимое СПИСКОМ (много предметов)",
		ToolTip = "Список предметов мешка: каждая запись — предмет (строкой таблицы предметов или классом) и количество. Непустой список отменяет одиночные поля «Что лежит внутри»/«Сколько штук»/«Патронов внутри» ниже. Деньги — отдельным полем, как раньше."))
	TArray<FPlacedLootEntry> PlacedLootList;

	// Класс стартового предмета (nullptr = предмета нет). Особый случай: класс патронов
	// (AAmmoItem) спавнится ОДНОЙ пачкой со стаком PlacedItemCount, а не N пустыми копиями.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (DisplayPriority = "2",
		DisplayName = "Что лежит внутри (класс предмета)",
		ToolTip = "Класс предмета, который окажется в рюкзаке при подборе (пусто = предмета нет). Патроны (AAmmoItem) кладутся одной пачкой размером «Сколько штук»."))
	TSubclassOf<AMasterInventoryItem> PlacedItemClass;

	// СЛУЖЕБНЫЙ КЛЮЧ предмета (пусто = ключ класса по умолчанию). НЕ переводится: по нему
	// сходится зачёт квеста, если на уровень положен квест-предмет (ADR-050, порция 0).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (DisplayPriority = "3",
		DisplayName = "Служебный ключ предмета (для квестов)",
		ToolTip = "Внутренний ключ предмета, по нему засчитываются квесты (например «Шкура волка»). Пусто = ключ класса по умолчанию. НЕ переводится и игроку не показывается."))
	FString PlacedItemDisplayName;

	// ПЕРЕВОДИМОЕ название этого же предмета, которое увидит игрок в рюкзаке (пусто =
	// название класса по умолчанию, а если и его нет — откат на ключ выше).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (DisplayPriority = "4",
		DisplayName = "Название предмета для игрока",
		ToolTip = "Название, которое игрок увидит в рюкзаке (переводимое). Пусто = название класса по умолчанию, а если и его нет — показывается служебный ключ."))
	FText PlacedItemDisplayText;

	// Сколько предметов положить (для AAmmoItem — размер стака одной пачки).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = "1", DisplayPriority = "4",
		DisplayName = "Сколько штук",
		ToolTip = "Сколько предметов положить внутрь. Для патронов — размер стака одной пачки."))
	int32 PlacedItemCount = 1;

	// Патроны В ДОПОЛНЕНИЕ к предмету (одна пачка AAmmoItem с этим стаком; 0 = без патронов).
	// Отдельное поле, потому что точка интереса несёт «расходник И патроны» одним пикапом,
	// а слот PlacedItemClass один (аналог поля денег).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = "0", DisplayPriority = "5",
		DisplayName = "Патронов внутри (дополнительно)",
		ToolTip = "Пачка патронов В ДОПОЛНЕНИЕ к предмету выше (0 = без патронов). Позволяет одним пикапом выдать «расходник И патроны»."))
	int32 PlacedAmmoAmount = 0;

	// --- Build 1.2.1 (ТЗ А4, Ринат: «хочу добавить ему свечение») ---
	// Свечение мешка/свёртка: MID от материала меша (M_VColor несёт параметры
	// GlowColor/GlowIntensity с нулевыми дефолтами — остальные пользователи материала
	// не затронуты). Настройка на экземпляре/BP, дефолт ВЫКЛ (решение Рината).

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (DisplayName = "Свечение включено", DisplayPriority = "6"))
	bool bGlowEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (DisplayName = "Цвет свечения", DisplayPriority = "7", EditCondition = "bGlowEnabled"))
	FLinearColor GlowColor = FLinearColor(1.0f, 0.78f, 0.25f, 1.0f); // тёплый янтарный, в тон шнуру мешка

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = "0.0", DisplayName = "Сила свечения", DisplayPriority = "8", EditCondition = "bGlowEnabled"))
	float GlowStrength = 3.0f;

	// Период полного цикла пульсации, сек. 0 = ровное свечение без пульса (и без тика).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = "0.0", DisplayName = "Период пульсации (сек, 0 = ровное)", DisplayPriority = "9", EditCondition = "bGlowEnabled"))
	float GlowPulsePeriod = 2.0f;

public:
	// Задать содержимое СПИСКОМ до BeginPlay (deferred-спавн: SpawnActorDeferred → этот вызов
	// → FinishSpawningActor). Штатный путь рантайм-мешков с набором предметов: мешок-награда
	// базы (ТЗ 22.08 возрождение баз) и headless-тесты. MoneyOverride >= 0 — заодно деньги.
	void SetPlacedLootList(const TArray<FPlacedLootEntry>& List, float MoneyOverride = -1.0f)
	{
		PlacedLootList = List;
		if (MoneyOverride >= 0.0f)
		{
			MoneyAmount = MoneyOverride;
		}
	}

	// Спавнит предметы списка скрытыми акторами-данными и возвращает их (владение — у
	// вызывающего: он кладёт предметы в свой контейнер обыска). Строка таблицы главнее
	// класса; стакаемые — одним предметом со счётчиком, нестакаемые — копиями. Общий путь
	// для мешка, машины (Report1 п.9) и будущих хранилищ баз. ContextName — для журнала.
	static TArray<AMasterInventoryItem*> SpawnLootEntries(UWorld* World,
		const TArray<FPlacedLootEntry>& List, const FVector& Location, const FString& ContextName);

private:
	// D8: спавнит размещённый лут (PlacedItemClass/PlacedAmmoAmount) скрытыми предметами
	// и складывает его вместе с «Денег внутри» в контейнер обыска. Зовётся из BeginPlay
	// только в игровом мире.
	void SpawnPlacedLoot();

	// ADR-076 п.10: путь СПИСКА (PlacedLootList непуст) — предметы строками таблицы/классами
	// с количествами; стакаемые — одним предметом со счётчиком, нестакаемые — копиями.
	void SpawnPlacedLootFromList();

	// Из контейнера что-то забрали (окно обыска). Опустевший мешок исчезает — ровно так же,
	// как раньше исчезал сразу после подбора.
	void HandleLootChanged();

	// Build 1.2.1 (ТЗ А4): создаёт MID слота 0 и включает свечение (+тик при пульсации).
	void SetupGlow();

	// MID свечения (создан из материала меша). UPROPERTY — защита от GC.
	UPROPERTY()
	UMaterialInstanceDynamic* GlowMID = nullptr;

	// Накопленное время пульса (фаза синуса).
	float GlowTime = 0.0f;

	// Идёт забор ВСЕГО содержимого разом (Collect): пока флаг поднят, обработчик изменений
	// пикап не уничтожает — судьбу мешка решает сам Collect, когда закончит перебор.
	bool bTakingAll = false;

	// Реестр живых пикапов (ADR-076 п.2, групповой обыск мешков). См. GetActivePickups.
	static TArray<TWeakObjectPtr<APickup>> ActivePickups;
};
