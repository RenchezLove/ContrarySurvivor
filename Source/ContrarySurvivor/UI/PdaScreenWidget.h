// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Widgets/Layout/Anchors.h"
#include "ContrarySurvivor/UI/PdaMapMath.h"
#include "ContrarySurvivor/UI/SelfHidingWidget.h"
#include "PdaScreenWidget.generated.h"

class APlayerCharacter;
class UBorder;
class UCanvasPanel;
class UImage;
class UPanelWidget;
class UScrollBox;
class UTextBlock;
class UTexture2D;
class UMapExplorationComponent;
class UWidgetTree;
struct FQuest;

/**
 * Кнопка, которая помнит своё задание: строка списка заданий и отметка задания на карте КПК.
 * У обычной UButton событие нажатия без параметров — по нему не понять, какую из десятка
 * одинаковых кнопок нажали.
 */
UCLASS()
class CONTRARYSURVIVOR_API UPdaQuestButton : public UButton
{
	GENERATED_BODY()

public:
	DECLARE_DELEGATE_OneParam(FOnQuestPicked, FName /*QuestId*/);

	FName QuestId = NAME_None;
	FOnQuestPicked OnPicked;

	// Подписаться на собственное нажатие (один раз после создания).
	void BindPick();

private:
	UFUNCTION()
	void HandlePicked();
};

/** Что показывает вкладка «Задания»: журнал, разложенный по разделам. */
struct FPdaJournalView
{
	TArray<FName> Main;      // идущие основные, в порядке журнала
	TArray<FName> Side;      // идущие побочные
	TArray<FName> Done;      // сданные
	FName Tracked = NAME_None; // какое задание отслеживается (пусто — идущих нет)

	bool IsEmpty() const { return Main.Num() == 0 && Side.Num() == 0 && Done.Num() == 0; }
};

/**
 * Окно карманного компьютера — КПК (слова Рината 10.10.2026: «Игроку понадобится аналог PDA из
 * Stalker. Там будет карта, сведения о квестах. Там же можно будет переключать метку на карте с
 * квеста на квест»). Две вкладки:
 *  - «Задания»: слева список (основные, побочные, свёрнутые выполненные), справа сведения о
 *    выбранном и кнопка «Отслеживать»;
 *  - «Карта»: картинка местности (или тёмное поле с сеткой, пока картинки нет) с отметками —
 *    герой, цели заданий, известные места. Нажатие по отметке задания делает его отслеживаемым.
 *    Карта открывается по мере исследования: неизведанное закрыто чернотой, места и их подписи
 *    появляются, когда герой там побывал (память героя — UMapExplorationComponent).
 *    Карту можно приближать (кнопки «+» и «−», колесо мыши, щипок двумя пальцами) и двигать
 *    (палец или мышь с зажатой кнопкой); правила вида — PdaMap::FMapView.
 *
 * Окно выглядит смартфоном (слова Рината 10.10.2026: «подложка виджета должна быть похожа на
 * смартфон… только там КПК, а здесь смартфон»): подложка PanelPlate занимает почти весь экран,
 * её размер, картинку и отступы экрана внутри картинки задают настройки UContraryPdaSettings —
 * код ставит их при открытии и при смене размера экрана. Содержимое лежит внутри экрана
 * смартфона: слева столбец (заголовок, вкладки, подсказка, «Закрыть»), справа вкладка.
 *
 * Два пути, как у остальных окон проекта:
 *  - готовый файл окна WBP_Pda (создаёт генератор окон) — раскладку, шрифты и постоянные
 *    подписи владелец правит мышкой, код их не перезаписывает;
 *  - файла нет — то же самое дерево строится кодом (BuildDefaultTree — единственное место,
 *    где описана раскладка: им же пользуется генератор).
 * Строки списка, сведения о задании и отметки карты — живые данные, их всегда ставит код.
 *
 * Тексты для игрока и цвета отметок — поля этого класса (Class Defaults файла окна). Цвета
 * вида задания, картинка карты и её границы в мире — UContraryPdaSettings (Project Settings).
 */
UCLASS()
class CONTRARYSURVIVOR_API UPdaScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// --- Постоянные подписи (кладутся в окно при его создании; дальше правятся в самом окне) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Подписи", meta = (DisplayName = "Заголовок окна"))
	FText TitleLabel = NSLOCTEXT("Pda", "Title", "Смартфон");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Подписи", meta = (DisplayName = "Вкладка заданий"))
	FText QuestsTabLabel = NSLOCTEXT("Pda", "QuestsTab", "Задания");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Подписи", meta = (DisplayName = "Вкладка карты"))
	FText MapTabLabel = NSLOCTEXT("Pda", "MapTab", "Карта");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Подписи", meta = (DisplayName = "Кнопка закрытия"))
	FText CloseLabel = NSLOCTEXT("Pda", "Close", "Закрыть");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Подписи", meta = (DisplayName = "Кнопка «приблизить карту»"))
	FText ZoomInLabel = NSLOCTEXT("Pda", "ZoomIn", "+");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Подписи", meta = (DisplayName = "Кнопка «отдалить карту»"))
	FText ZoomOutLabel = NSLOCTEXT("Pda", "ZoomOut", "−");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Подписи", meta = (DisplayName = "Кнопка «найти героя на карте»"))
	FText FindHeroLabel = NSLOCTEXT("Pda", "FindHero", "Где я");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Подписи", meta = (DisplayName = "Подсказка вкладки карты"))
	FText MapHintLabel = NSLOCTEXT("Pda", "MapHint",
		"Нажмите на отметку задания, чтобы отслеживать его. Оранжевые точки — костры, где можно сохраниться.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Подписи", meta = (DisplayName = "Заданий нет"))
	FText EmptyLabel = NSLOCTEXT("Pda", "Empty", "Заданий пока нет. Поговорите со старостой.");

	// --- Живые тексты вкладки «Задания» ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Задания", meta = (DisplayName = "Раздел основных"))
	FText MainHeader = NSLOCTEXT("Pda", "MainHeader", "Основные");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Задания", meta = (DisplayName = "Раздел побочных"))
	FText SideHeader = NSLOCTEXT("Pda", "SideHeader", "Побочные");

	// {Count} — сколько заданий сдано.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Задания", meta = (DisplayName = "Раздел выполненных (свёрнут)"))
	FText DoneHeaderCollapsed = NSLOCTEXT("Pda", "DoneCollapsed", "Выполненные: {Count} (показать)");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Задания", meta = (DisplayName = "Раздел выполненных (развёрнут)"))
	FText DoneHeaderExpanded = NSLOCTEXT("Pda", "DoneExpanded", "Выполненные: {Count} (скрыть)");

	// {Title} — название задания.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Задания", meta = (DisplayName = "Строка отслеживаемого задания"))
	FText TrackedRowFormat = NSLOCTEXT("Pda", "TrackedRow", "{Title} (отслеживается)");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Задания", meta = (DisplayName = "Кто выдал"))
	FText GiverFormat = NSLOCTEXT("Pda", "Giver", "Выдал: {Giver}");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Задания", meta = (DisplayName = "Кому сдавать"))
	FText ReceiverFormat = NSLOCTEXT("Pda", "Receiver", "Сдать: {Receiver}");

	// {Objectives} — цели, каждая со своей строки.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Задания", meta = (DisplayName = "Цели", MultiLine = "true"))
	FText ObjectivesFormat = NSLOCTEXT("Pda", "Objectives", "Цели:\n{Objectives}");

	// Одна цель: {Objective} — что сделать, {Done} — сделано, {Total} — сколько надо.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Задания", meta = (DisplayName = "Одна цель"))
	FText ObjectiveFormat = NSLOCTEXT("Pda", "Objective", "{Objective} {Done} из {Total}");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Задания", meta = (DisplayName = "Задание готово к сдаче"))
	FText ReadyText = NSLOCTEXT("Pda", "Ready", "Цели выполнены — задание можно сдавать.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Задания", meta = (DisplayName = "Задание сдано"))
	FText TurnedInText = NSLOCTEXT("Pda", "TurnedIn", "Задание выполнено и сдано.");

	// {Reward} — монеты и предметы через запятую.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Задания", meta = (DisplayName = "Награда"))
	FText RewardFormat = NSLOCTEXT("Pda", "Reward", "Награда: {Reward}");

	// {Amount} — число, {Word} — слово «монета» в нужной форме.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Задания", meta = (DisplayName = "Награда монетами"))
	FText RewardMoneyFormat = NSLOCTEXT("Pda", "RewardMoney", "{Amount} {Word}");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Задания", meta = (DisplayName = "Слово: 1 монета"))
	FText MoneyWordOne = NSLOCTEXT("Pda", "MoneyOne", "монета");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Задания", meta = (DisplayName = "Слово: 2 монеты"))
	FText MoneyWordFew = NSLOCTEXT("Pda", "MoneyFew", "монеты");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Задания", meta = (DisplayName = "Слово: 5 монет"))
	FText MoneyWordMany = NSLOCTEXT("Pda", "MoneyMany", "монет");

	// {Item} — название предмета, {Count} — число штук.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Задания", meta = (DisplayName = "Награда предметом"))
	FText RewardItemFormat = NSLOCTEXT("Pda", "RewardItem", "{Item} — {Count} шт.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Задания", meta = (DisplayName = "Кнопка «Отслеживать»"))
	FText TrackLabel = NSLOCTEXT("Pda", "Track", "Отслеживать");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Задания", meta = (DisplayName = "Кнопка у отслеживаемого задания"))
	FText TrackedLabel = NSLOCTEXT("Pda", "Tracked", "Отслеживается");

	// Строки списка заданий строит код, поэтому их размер — здесь (Class Defaults окна), а не
	// в дереве: отступы вокруг названия дают строку высотой под палец (не ниже 96), просвет —
	// расстояние между строками.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Задания", meta = (DisplayName = "Строка задания: отступы вокруг названия"))
	FMargin QuestRowPadding = FMargin(12.0f, 36.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Задания", meta = (ClampMin = "0.0", DisplayName = "Строка задания: просвет между строками"))
	float QuestRowGap = 12.0f;

	// --- Карта ---

	// Подпись зоны деревни, если места «рядом с ней» нет в списке мест настроек карты.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Карта", meta = (DisplayName = "Подпись деревни"))
	FText VillageLabel = NSLOCTEXT("Pda", "Village", "Деревня");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Карта", meta = (DisplayName = "Цвет ничьих мест"))
	FLinearColor NeutralPlaceColor = FLinearColor(0.85f, 0.82f, 0.7f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Карта", meta = (DisplayName = "Цвет мирных мест"))
	FLinearColor FriendlyPlaceColor = FLinearColor(0.55f, 0.9f, 0.55f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Карта", meta = (DisplayName = "Цвет баз врагов"))
	FLinearColor EnemyPlaceColor = FLinearColor(0.95f, 0.4f, 0.35f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Карта", meta = (DisplayName = "Цвет костров"))
	FLinearColor CampfireColor = FLinearColor(1.0f, 0.6f, 0.2f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Карта", meta = (DisplayName = "Цвет стрелки героя"))
	FLinearColor PlayerArrowColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Карта", meta = (DisplayName = "Цвет сетки (карта без картинки)"))
	FLinearColor GridColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.07f);

	// Шаг сетки в единицах мира (сантиметры): 2000 = клетка 20 на 20 метров.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Карта", meta = (ClampMin = "200.0", DisplayName = "Шаг сетки, см"))
	float GridStepWorld = 2000.0f;

	// Насколько тусклее отметки неотслеживаемых заданий (множитель прозрачности).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pda|Карта", meta = (ClampMin = "0.1", ClampMax = "1.0", DisplayName = "Тусклость прочих отметок заданий"))
	float OtherQuestMarkerOpacity = 0.55f;

	// --- Работа с окном ---

	// Наполнить окно журналом героя. Звать при КАЖДОМ открытии.
	void InitPda(APlayerCharacter* InPlayer);

	// Показать вкладку: false — «Задания», true — «Карта».
	void ShowTab(bool bMap);
	bool IsMapTabShown() const { return bMapTab; }

	// Выбрать задание в списке (сведения справа).
	void SelectQuest(FName QuestId);
	FName GetSelectedQuestId() const { return SelectedQuestId; }

	// Сделать задание отслеживаемым (кнопка «Отслеживать» и отметка на карте).
	bool TrackQuest(FName QuestId);

	// Развернуть или свернуть раздел «Выполненные».
	void ToggleDoneSection();

	// Перечитать журнал и перестроить список, сведения и отметки карты прямо сейчас. В игре это
	// делает сам кадр окна; автопроверкам кадра не дождаться.
	void RefreshNow();

	// Игрок нажал «Закрыть» — владелец (контроллер) убирает окно и возвращает управление.
	FSimpleMulticastDelegate OnCloseRequested;

	// --- Приближение и сдвиг карты. Точки и сдвиги — в единицах поля карты (MapPanel), его
	// левый-верхний угол — ноль. Этими же дверями ходят колесо мыши, пальцы и кнопки. ---

	// Приблизить или отдалить к точке поля: то место карты, что под ней, под ней и останется.
	void ZoomMapAt(const FVector2D& ViewportPoint, double NewZoom);
	// Кнопки «+» и «−»: на столько шагов (минус — отдалить), к середине поля.
	void ZoomMapBySteps(int32 Steps);
	void PanMapBy(const FVector2D& Delta);
	// Два пальца: были в OldA и OldB, стали в NewA и NewB.
	void PinchMap(const FVector2D& OldA, const FVector2D& OldB, const FVector2D& NewA, const FVector2D& NewB);
	// Кнопка «найти героя»: поставить героя в середину поля.
	void CenterMapOnHero();

	// Размер поля карты. В игре его каждый кадр сверяет само окно; автопроверкам живой
	// раскладки не дождаться — они задают размер сами.
	void SetMapViewportSize(const FVector2D& Size);

	double GetMapZoom() const { return MapView.Zoom; }
	const PdaMap::FMapView& GetMapView() const { return MapView; }
	FVector2D GetMapViewportSize() const { return MapViewportSize; }
	// Размер карты при приближении 1 (вписана в поле с пропорциями мира).
	FVector2D GetMapBaseSize() const;
	// Где сейчас стоит карта в поле и какого она размера (то, что уходит в раскладку).
	FVector2D GetMapAreaOffset() const { return MapView.GetContentOffset(MapViewportSize, GetMapBaseSize()); }
	FVector2D GetMapAreaSize() const { return MapView.GetContentSize(GetMapBaseSize()); }
	// Точка мира -> точка поля карты при нынешнем виде (там рисуются значок и стрелка героя).
	FVector2D WorldToMapViewport(const FVector& WorldLocation) const;

	// --- Окно-смартфон ---

	// Поставить размер подложки, отступы экрана и картинку по настройкам для экрана такого
	// размера. В игре зовёт само окно; автопроверки зовут напрямую.
	void ApplyPhoneLayout(const FVector2D& ScreenSize);
	FVector2D GetPhoneSize() const { return PhoneSize; }
	FMargin GetPhoneScreenPadding() const { return PhonePadding; }
	bool IsPhoneTextureShown() const { return LoadedPhoneTexture != nullptr; }

	// Разложить журнал по разделам окна. Чистое правило — его гоняют автопроверки.
	static void BuildJournalView(const TArray<FQuest>& Journal, FName TrackedQuestId, FPdaJournalView& OutView);

	// ЕДИНСТВЕННОЕ описание раскладки окна: им строится и готовый файл WBP_Pda (генератор
	// окон), и кодовая запаска, когда файла нет. Имена элементов = именам полей ниже.
	static void BuildDefaultTree(UWidgetTree* Tree);

	// --- Для автопроверок (живой Slate в прогонах не поднимается) ---
	int32 GetQuestRowCount() const { return QuestRowCount; }
	int32 GetQuestMapMarkerCount() const { return QuestMapMarkerCount; }
	int32 GetPlaceMapMarkerCount() const { return PlaceMapMarkerCount; }
	// Подписи мест, которые сейчас видны на карте (место открыто).
	const TArray<FString>& GetShownPlaceLabels() const { return ShownPlaceLabels; }
	// Маска черноты, как она ушла в картинку: по байту на клетку, 255 — закрыто. Пусто — черноты нет.
	const TArray<uint8>& GetFogAlpha() const { return FogAlpha; }
	// Сколько раз маска пересчитывалась (она не должна считаться каждый кадр).
	int32 GetFogRebuildCount() const { return FogRebuildCount; }
	// Где на поле карты рисуется стрелка героя: середина стрелки, прижатая внутрь поля.
	static FVector2D ClampArrowCenter(const FVector2D& Center, const FVector2D& AreaMin, const FVector2D& AreaMax, double Inset);
	bool IsDoneSectionExpanded() const { return bDoneExpanded; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// Мышь: колесо приближает к курсору, зажатая левая кнопка двигает карту, нажатие мимо
	// смартфона закрывает окно. Пальцы: один двигает карту, два — приближают и двигают.
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

	UFUNCTION()
	void HandleZoomInClicked();

	UFUNCTION()
	void HandleZoomOutClicked();

	UFUNCTION()
	void HandleFindHeroClicked();

	UFUNCTION()
	void HandleQuestsTabClicked();

	UFUNCTION()
	void HandleMapTabClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleTrackClicked();

private:
	// Достроить дерево кодом, если его нет, привязать элементы и кнопки. Повторный вызов
	// бесплатен. Зовётся и из NativeOnInitialized, и из InitPda: в прогонах автопроверок
	// NativeOnInitialized движок не зовёт (нет живого игрока — UserWidget.cpp:160).
	void EnsureSetup();

	// Привязать поля-элементы по именам (для дерева, построенного кодом).
	void BindCodeTree();

	void RebuildQuestList();
	void RefreshDetails();
	void RebuildMap();
	void RefreshTabLook();

	// Поставить карту в поле по нынешнему виду (приближение и сдвиг): карта не растягивается,
	// пропорции прямоугольника мира сохраняются.
	void ApplyMapView();

	// Точка экрана -> точка поля карты; false — точка вне поля либо вкладка карты не показана.
	bool ScreenToMapViewport(const FVector2D& ScreenPosition, FVector2D& OutPoint) const;
	// Под точкой экрана кнопка поля карты (приближение, «найти героя», отметка задания)?
	bool IsOverMapButton(const FVector2D& ScreenPosition) const;
	// Точка экрана мимо смартфона?
	bool IsOutsidePhone(const FVector2D& ScreenPosition) const;

	// Изменился ли журнал с прошлой отрисовки (число заданий, состояния, счёт, отслеживаемое).
	uint32 ComputeJournalStamp() const;

	void AddSectionHeader(const FText& Text);
	void AddQuestRow(const FQuest& Quest, bool bTracked);
	void AddMapDot(const FVector& WorldLocation, float Size, const FLinearColor& Color, const FText& Label);

	// Открытая часть карты героя (нет героя или компонента — nullptr: чернота не рисуется).
	const UMapExplorationComponent* GetExploration() const;
	// Видно ли место на карте: открыта ли клетка под ним (без памяти героя видно всё).
	bool IsPlaceRevealed(const FVector& WorldLocation) const;
	// Пересчитать картинку черноты, если открытое изменилось с прошлого раза.
	void UpdateFogMask();
	void AddGridLines();

	FText BuildRewardText(const FQuest& Quest) const;

	// --- Элементы: из WBP_Pda по именам либо из BuildDefaultTree (имена те же) ---

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> QuestsTabButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuestsTabText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MapTabButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MapTabText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CloseText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> QuestsPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> QuestList;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailTitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailGiverText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailDescriptionText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailObjectivesText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailRewardText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> TrackButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TrackText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> MapPanel;

	// Сама карта: код ставит её в поле MapPanel по нынешнему приближению и сдвигу. Всё, что
	// лежит на ней, привязано долями и едет вместе с ней.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> MapArea;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MapZoomInButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MapZoomOutButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MapFindHeroButton;

	// Подложка окна и её холст: размер, отступы и картинку ставит ApplyPhoneLayout.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> PanelPlate;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> MapBackground;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> MapImage;

	// Чернота неизведанного поверх картинки карты и под отметками.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> MapFogImage;

	// Сюда код кладёт сетку и отметки (положение — долями карты через якоря).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> MapMarkers;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MapHintText;

	// --- Состояние ---

	TWeakObjectPtr<APlayerCharacter> Player;

	// Картинка карты, пока окно живо (мягкая ссылка настроек сама её не удержит).
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LoadedMapTexture;

	// Картинка-маска черноты: по точке на клетку сетки, растягивается на поле со сглаживанием.
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FogTexture;

	TArray<uint8> FogAlpha;
	TArray<FString> ShownPlaceLabels;
	int32 LastFogRevision = INDEX_NONE;
	int32 FogRebuildCount = 0;

	FName SelectedQuestId = NAME_None;
	bool bSetupDone = false;
	bool bMapTab = false;
	bool bDoneExpanded = false;
	bool bRefreshPending = false;
	uint32 LastJournalStamp = 0;

	// Картинка смартфона, пока окно живо.
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LoadedPhoneTexture;

	// Вид карты переживает закрытие окна (окно у контроллера одно на запуск): приближение
	// остаётся, каким игрок его оставил, а при открытии карта встаёт героем в середину.
	PdaMap::FMapView MapView;
	FVector2D MapViewportSize = FVector2D::ZeroVector;
	bool bCenterOnHeroPending = false;

	FVector2D PhoneSize = FVector2D::ZeroVector;
	FMargin PhonePadding;
	FVector2D LastScreenSize = FVector2D::ZeroVector;

	// Мышь с зажатой кнопкой и пальцы на поле карты (номер пальца -> точка поля).
	bool bMouseDragging = false;
	FVector2D LastMouseDragPoint = FVector2D::ZeroVector;
	TMap<uint32, FVector2D> MapTouches;

	int32 QuestRowCount = 0;
	int32 QuestMapMarkerCount = 0;
	int32 PlaceMapMarkerCount = 0;
};

