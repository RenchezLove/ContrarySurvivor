// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CorpseLootWidget.generated.h"

class UTextBlock;
class UButton;
class UImage;
class UScrollBox;
class UTexture2D;
class UCorpseLootComponent;
class AMasterInventoryItem;
class APlayerCharacter;
class UItemTileWidget;

// Строковый UCorpseLootRowWidget УДАЛЁН (Build 1.2.2): обыск, как инвентарь и магазин,
// перешёл на общую плитку UItemTileWidget («иконки в сетке, как в сталкере или LDoE»);
// клик по плитке = прежняя кнопка «Забрать», деньги — плитка с иконкой T_Item_Money.

/**
 * Что реально ушло игроку за одно нажатие «Забрать всё» (издатель 11.08.2026, п.3.3:
 * «показать, что именно упало в рюкзак — сводкой, а не четырьмя отдельными сообщениями»).
 *
 * Обычная структура без отражения: сборка строки должна проверяться автотестом, а живой
 * Slate в headless-прогонах проекта не поднимается.
 */
struct FCorpseLootTakenSummary
{
	// Название предмета + сколько штук, в порядке забора (одинаковые складываются).
	TArray<TPair<FText, int32>> Items;

	// Деньги всей группы одной суммой.
	int32 Money = 0;

	// Забрали огнестрел: напоминание «наденьте в инвентаре» (ADR-063 п.2) уезжает ТОЙ ЖЕ
	// строкой, иначе игрок получил бы два всплывающих сообщения подряд — прямой запрет п.3.3.
	bool bFirearmTaken = false;

	// Хоть один предмет рюкзак не принял (п.3.4): тело осталось полным, в землю не ушло.
	bool bBackpackRefused = false;

	// Складывает одинаковые названия в одну запись («Шкура волка» ×4).
	void AddItem(const FText& Name, int32 Count);

	bool IsEmpty() const { return Items.Num() == 0 && Money <= 0; }
};

/**
 * Окно обыска трупа (Build 1.2.1, ТЗ А1; Ринат: «Открывается окно (похожее немного на
 * экран торговли) и игрок выбирает что из лута трупа себе в инвентарь добавить. Как в
 * сталкере, LDoE»).
 *
 * Содержимое: список предметов трупа (иконка+название+количество) И деньги ОДНИМ списком
 * (дефолт Рината), забор кликом по строке («Забрать»), кнопка «Забрать всё», закрытие —
 * крестик/Esc (Esc ведёт контроллер). Частичный обыск штатен: остаток лежит в трупе до
 * таймера исчезновения.
 *
 * Build 1.2.2 (Ринат про BP_Picup: «механика похожая на ту, что я обыскиваю ящик или труп
 * и выбираю что себе положить в инвентарь»): это же окно показывает содержимое мешка-пикапа
 * (APickup несёт такой же UCorpseLootComponent). Отличие только в поведении источника:
 * обысканный до конца мешок исчезает, и окно закрывается само (контейнер умер — NativeTick
 * просит закрытия). Заголовок берётся у контейнера (SearchTitle), поэтому над мешком не
 * висит надпись про труп.
 *
 * Архитектура — ADR-048, по образцу UShopScreenWidget: логика здесь, раскладку
 * WBP_CorpseLoot (канвас-схема) строит Ринат/генератор; кубики цепляются по ТОЧНЫМ
 * именам через BindWidgetOptional. ЗАПАСНОЙ ВИД: если виджет создан БЕЗ ассета (слот
 * класса на HUD пуст), NativeOnInitialized строит дерево кодом (паттерн этапа F /
 * UEndOfStoryWidget) — механика работает и до генерации/назначения WBP.
 *
 * Закрытие — делегат OnCloseRequested: мир/режим ввода возвращает контроллер
 * (CloseCorpseLoot), сам виджет их не трогает (как магазин).
 *
 * ЗАДАНИЕ ИЗДАТЕЛЯ 11.08.2026 (групповой обыск): окно работает не с одним телом, а с
 * ГРУППОЙ (п.3.1) — содержимое всех необысканных тел рядом лежит в одном списке
 * вперемешку, «Забрать всё» опустошает всю группу разом, а по итогу игрок получает ОДНУ
 * строку «Получено: …» (п.3.3). Мешок-пикап — та же группа из одного контейнера, его
 * поведение не изменилось. Окно просит закрытия, когда исчезло ПОСЛЕДНЕЕ тело группы.
 */
UCLASS()
class CONTRARYSURVIVOR_API UCorpseLootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Привязка данных после CreateWidget (труп + игрок) и первая сборка списка.
	void InitCorpseLoot(UCorpseLootComponent* InCorpse, APlayerCharacter* InPlayer);

	// То же по ГРУППЕ тел (издатель 11.08.2026, п.3.1): одно нажатие — ОДНО окно, в котором
	// содержимое всех необысканных тел рядом лежит вперемешку, «Забрать всё» опустошает всю
	// группу разом (решение game-lead: второй кнопки и второго окна не заводим). Первый
	// элемент — тело, которое подсветила подсказка: у него берётся заголовок окна.
	void InitCorpseLootGroup(const TArray<UCorpseLootComponent*>& InCorpses, APlayerCharacter* InPlayer);

	// Крестик нажат / труп исчез под открытым окном — подписан контроллер (CloseCorpseLoot).
	FSimpleMulticastDelegate OnCloseRequested;

	// --- Настройки (Class Defaults WBP_CorpseLoot) ---

	// Класс ПЛИТКИ списка (Build 1.2.2, тайлы): дефолт — C++-плитка с кодовым деревом;
	// Ринат/генератор назначает сюда WBP_ItemTile для стилизации.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot", meta = (DisplayPriority = "1",
		DisplayName = "Класс плитки предмета"))
	TSubclassOf<UItemTileWidget> TileWidgetClass;

	// Сетка лута: число колонок и габариты плитки/иконки (настраиваемые — ТЗ).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot", meta = (ClampMin = "1", DisplayPriority = "2",
		DisplayName = "Колонок в сетке лута"))
	int32 TileColumns = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot", meta = (DisplayPriority = "3",
		DisplayName = "Размер плитки"))
	FVector2D TileSize = FVector2D(110.0f, 150.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot", meta = (ClampMin = "16.0", DisplayPriority = "4",
		DisplayName = "Размер иконки в плитке"))
	float TileIconSize = 86.0f;

	// Иконка плитки денег (у денег нет класса-предмета — иконку задаёт окно).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot", meta = (DisplayPriority = "5",
		DisplayName = "Иконка плитки денег"))
	TSoftObjectPtr<UTexture2D> MoneyRowIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/UI/Icons/Items/T_Item_Money.T_Item_Money")));

	// Заголовок окна (в кодовом фолбэке; в WBP кубик TitleText может держать и статичный текст).
	// ADR-076 п.2 (решение Рината «окно — универсальное окно обыска»): дефолт «Обыск» — без
	// привязки к трупу; конкретику даёт перечень объектов (SearchObjectsListText) и SearchTitle
	// контейнера. Имя файла/класса намеренно не меняются — ассет под правками Рината,
	// слот HUD и таблицы генератора (согласовано с лидом 22.08).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Texts", meta = (DisplayPriority = "1"))
	FText TitleLabel = NSLOCTEXT("CorpseLoot", "Title", "Обыск");

	// --- Перечень обыскиваемых объектов (ADR-076 п.2: «СВЕРХУ в окне (или над ним) перечень
	// ЧЕРЕЗ ТОЧКУ С ЗАПЯТОЙ» — «Труп волка; Труп волка; Мешок»). Названия дают контейнеры
	// (UCorpseLootComponent::SearchObjectName), строку собирает BuildSearchObjectsLine. ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Перечень", meta = (DisplayPriority = "1",
		DisplayName = "Показывать перечень обыскиваемых"))
	bool bShowSearchObjectsList = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Перечень", meta = (DisplayPriority = "2",
		DisplayName = "Разделитель перечня"))
	FText SearchObjectsSeparator = NSLOCTEXT("CorpseLoot", "SearchObjectsSeparator", "; ");

	// Название объекта, у контейнера которого поле «Название для перечня» пустое.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Перечень", meta = (DisplayPriority = "3",
		DisplayName = "Запасное название объекта"))
	FText SearchObjectsFallbackName = NSLOCTEXT("CorpseLoot", "SearchObjectsFallback", "Труп");

	// П.0 ADR-077: стиль строки-перечня (кегль/цвет/позиция) живёт В АССЕТЕ — кубик
	// SearchObjectsListText добавляет -augment генератора, Ринат правит его в дизайнере.
	// Здесь остались только ДАННЫЕ: разделитель и запасное имя (код собирает текст).

	// Чистая сборка строки перечня («Труп волка; Труп волка; Мешок») — открыта для
	// headless-теста: имена контейнеров через разделитель, пустое имя -> запасное.
	static FText BuildSearchObjectsLine(const TArray<UCorpseLootComponent*>& InCorpses,
		const FText& Separator, const FText& FallbackName);

	// Название строки денег в общем списке.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Texts", meta = (DisplayPriority = "2"))
	FText MoneyRowLabel = NSLOCTEXT("CorpseLoot", "MoneyRow", "Деньги");

	// Подпись «Забрать» строкового списка УДАЛЕНА (Build 1.2.2): забор — клик по плитке.

	// Подпись кнопки «Забрать всё».
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Texts", meta = (DisplayPriority = "4"))
	FText TakeAllCaption = NSLOCTEXT("CorpseLoot", "TakeAll", "Забрать всё");

	// Подпись крестика (кодовый фолбэк; в WBP Ринат волен нарисовать свой крестик).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Texts", meta = (DisplayPriority = "5"))
	FText CloseCaption = NSLOCTEXT("CorpseLoot", "Close", "X");

	// Строка-заглушка пустого трупа (всё забрано, труп лежит до таймера).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Texts", meta = (DisplayPriority = "6"))
	FText EmptyLabel = NSLOCTEXT("CorpseLoot", "Empty", "Пусто");

	// ADR-063 п.2: всплывашка при подборе огнестрела с трупа ({Item} — имя предмета).
	// Ствол остаётся в рюкзаке, в слот игрок надевает сам — надпись это объясняет.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Texts", meta = (DisplayPriority = "7"))
	FText FirearmPickupHintFormat = NSLOCTEXT("CorpseLoot", "FirearmPickupHint",
		"Подобрано: {Item} — наденьте в инвентаре");

	// --- Сводка «что упало в рюкзак» (издатель 11.08.2026, п.3.3) ---

	// Вся сводка ОДНОЙ строкой: {List} — перечень через разделитель.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Сводка обыска", meta = (DisplayPriority = "1"))
	FText TakenSummaryFormat = NSLOCTEXT("CorpseLoot", "TakenSummary", "Получено: {List}");

	// Одна запись перечня, когда штук больше одной ({Name} — название, {Count} — сколько).
	// Названия предметов задаются данными, склонять их по-русски нечем — поэтому число
	// стоит рядом («Шкура волка x4»), а не внутри фразы.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Сводка обыска", meta = (DisplayPriority = "2"))
	FText TakenSummaryItemFormat = NSLOCTEXT("CorpseLoot", "TakenSummaryItem", "{Name} x{Count}");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Сводка обыска", meta = (DisplayPriority = "3"))
	FText TakenSummarySeparator = NSLOCTEXT("CorpseLoot", "TakenSummarySeparator", ", ");

	// Деньги в перечне: {Count} — сумма, {Word} — слово под число (см. три поля ниже).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Сводка обыска", meta = (DisplayPriority = "4"))
	FText TakenSummaryMoneyFormat = NSLOCTEXT("CorpseLoot", "TakenSummaryMoney", "{Count} {Word}");

	// Русский счёт денег: 1 монета, 2-4 монеты, 5 и больше — монет (11-14 всегда «монет»).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Сводка обыска", meta = (DisplayPriority = "5"))
	FText MoneyWordOne = NSLOCTEXT("CorpseLoot", "MoneyWordOne", "монета");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Сводка обыска", meta = (DisplayPriority = "6"))
	FText MoneyWordFew = NSLOCTEXT("CorpseLoot", "MoneyWordFew", "монеты");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Сводка обыска", meta = (DisplayPriority = "7"))
	FText MoneyWordMany = NSLOCTEXT("CorpseLoot", "MoneyWordMany", "монет");

	// Хвост той же строки, если среди забранного был огнестрел (ADR-063 п.2 — напоминание
	// живёт, но отдельным сообщением больше не показывается).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Сводка обыска", meta = (DisplayPriority = "8"))
	FText TakenSummaryFirearmSuffix = NSLOCTEXT("CorpseLoot", "TakenSummaryFirearm",
		" (оружие наденьте в инвентаре)");

	// п.3.4: рюкзак принял не всё — говорим словами и оставляем такие тела нетронутыми.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Сводка обыска", meta = (DisplayPriority = "9"))
	FText BackpackFullWithSummaryFormat = NSLOCTEXT("CorpseLoot", "BackpackFullWithSummary",
		"{Summary}. В рюкзак влезло не всё");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CorpseLoot|Сводка обыска", meta = (DisplayPriority = "10"))
	FText BackpackFullHint = NSLOCTEXT("CorpseLoot", "BackpackFull",
		"В рюкзак влезло не всё — эти тела остались нетронутыми");

	// Забрать один предмет трупа в рюкзак игрока. true — предмет ушёл игроку. ПУБЛИЧНЫЙ
	// намеренно: его зовут и клики плиток, и headless-тесты потока лута (живой Slate в
	// Automation-тестах проекта не поднимается — паттерн UStartScreenWidget).
	// OutSummary задан — забор идёт в общей сводке (групповой обыск): отдельные всплывашки
	// по каждому предмету не показываются, всё уедет одной строкой.
	bool TakeItemToBackpack(AMasterInventoryItem* TakenItem, FCorpseLootTakenSummary* OutSummary = nullptr);

	// Опустошить ВСЮ группу тел (кнопка «Забрать всё»). Возвращает то, что реально ушло
	// игроку: тела, из которых забрать не удалось, остаются полными.
	FCorpseLootTakenSummary TakeAllFromGroup();

	// Одна строка сводки для всплывашки. Пустая сводка без отказов — пустой текст.
	FText BuildTakenSummaryText(const FCorpseLootTakenSummary& Summary) const;

	// Слово под число монет по русским правилам счёта. Статическая и без состояния —
	// проверяется автотестом.
	static FText PickMoneyWord(int32 Amount, const FText& One, const FText& Few, const FText& Many);

protected:
	virtual void NativeOnInitialized() override;

	// Труп исчез по таймеру под открытым окном — окно честно просит закрытия (один раз).
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION() void HandleTakeAllClicked();
	UFUNCTION() void HandleCloseClicked();

	// Клик по плитке = забрать (payload — в плитке).
	void HandleTileTake(UItemTileWidget* Tile);

	// Полная пересборка списка по текущему содержимому трупа.
	void RefreshList();

	// --- Кубики WBP_CorpseLoot (имена ТОЧНЫЕ; нет ассета — кодовое дерево) ---

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	// Перечень обыскиваемых (ADR-076 п.2). П.0 ADR-077: кубик — ИЗ АССЕТА (-augment), код
	// его не создаёт и не двигает (кодовое fallback-дерево целиком без ассета — исключение,
	// там строка встроена в колонку). Нет в ассете — предупреждение, перечня не будет.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SearchObjectsListText;

	// Обновляет ТЕКСТ строки-перечня по живым контейнерам группы (InitCorpseLootGroup).
	void UpdateSearchObjectsLine();

	// Список лута (деньги + предметы одним списком, прокрутка штатная).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> LootList;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> TakeAllButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TakeAllText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CloseText;

private:
	// Кодовое дерево-фолбэк, если окно создано без WBP.
	void BuildFallbackTree();

	// Забрать деньги ВСЕЙ группы на баланс игрока; возвращает забранную сумму.
	float TakeMoneyToPlayer();

	// Живые тела группы (мёртвые записи отсеяны) и суммарные деньги/предметы по ним.
	TArray<UCorpseLootComponent*> GetGroupCorpses() const;
	float GetGroupMoney() const;
	TArray<AMasterInventoryItem*> GetGroupItems() const;
	bool GroupHasLoot() const;

	// В каком теле группы лежит этот предмет (владелец забора).
	UCorpseLootComponent* FindItemHolder(const AMasterInventoryItem* Item) const;

	// Примет ли рюкзак предмет. ВМЕСТИМОСТИ у рюкзака сегодня НЕТ (UInventoryComponent::AddItem
	// отказывает только на пустом указателе) — это единственное место, куда придёт проверка
	// вместимости, когда её заведут: отказ здесь оставляет тело полным (п.3.4).
	bool CanBackpackAcceptItem(const AMasterInventoryItem* Item) const;

	// Показать сводку одной всплывашкой (тем же тостом, что подсказки онбординга).
	void ShowTakenSummary(const FCorpseLootTakenSummary& Summary);

	// Группа тел под окном (слабые ссылки: тела исчезают независимо от окна). Первый —
	// подсвеченное тело; одиночный контейнер (мешок-пикап) — группа из одного элемента.
	TArray<TWeakObjectPtr<UCorpseLootComponent>> Corpses;

	UPROPERTY()
	TObjectPtr<APlayerCharacter> Player;

	// Закрытие уже запрошено (труп исчез) — не спамим делегат каждый тик.
	bool bCloseRequested = false;
};
