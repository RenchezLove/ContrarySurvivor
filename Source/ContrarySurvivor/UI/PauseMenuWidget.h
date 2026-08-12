// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PauseMenuWidget.generated.h"

class UButton;
class UVerticalBox;
class UBorder;
class UTextBlock;

/**
 * Стиль меню паузы. Живёт EditAnywhere-полем на контроллере (виджет строится из C++-класса
 * и в Details не виден — паттерн FTouchControlsConfig; директива Рината 07-18). Дефолты
 * дословно повторяют прежние зашитые значения.
 *
 * Локализация (ADR-050): подписи — FText с дефолтами через NSLOCTEXT (LOCTEXT в значении
 * по умолчанию UHT запрещает — UhtTextProperty.cs:104). Дерево строится кодом, ассета в
 * дизайнере у панели нет, поэтому подпись и значение по кубикам не разделяются.
 */
USTRUCT(BlueprintType)
struct FPauseMenuStyle
{
	GENERATED_BODY()

	// Затемнение экрана под панелью.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FLinearColor DimColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.55f);

	// Золотой кант панели и тёмный фон панели (палитра модалок HUD).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FLinearColor FrameColor = FLinearColor(0.8f, 0.65f, 0.25f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FLinearColor PanelColor = FLinearColor(0.06f, 0.07f, 0.09f, 0.95f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FText TitleText = NSLOCTEXT("PauseMenuWidget", "TitleText", "ПАУЗА");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu", meta = (ClampMin = "8"))
	int32 TitleFontSize = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FLinearColor TitleColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FText ResumeText = NSLOCTEXT("PauseMenuWidget", "ResumeText", "Продолжить");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FText QuitText = NSLOCTEXT("PauseMenuWidget", "QuitText", "Выход");

	// --- Возврат в главное меню (подход 3 волны меню; спека glavnoe-menu-spec.md, раздел
	// «Поведение паузы»: «Возврат в меню из паузы — с подтверждением, если прогресс не
	// сохранён»). Переспрос переключает подписи тех же кнопок, как у «Новой игры» в меню. ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FText MainMenuText = NSLOCTEXT("PauseMenuWidget", "MainMenuText", "В главное меню");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu|Confirm Main Menu")
	FText ConfirmMainMenuTitleText = NSLOCTEXT("PauseMenuWidget", "ConfirmMainMenuTitle",
		"Выйти в меню без сохранения?");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu|Confirm Main Menu")
	FText ConfirmMainMenuYesText = NSLOCTEXT("PauseMenuWidget", "ConfirmMainMenuYes", "Да, выйти");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu|Confirm Main Menu")
	FText ConfirmMainMenuCancelText = NSLOCTEXT("PauseMenuWidget", "ConfirmMainMenuCancel", "Отмена");

	// --- Новые пункты паузы (просьба Рината 08-09: «добавь кнопку ведующую в сообщество, а
	// также кнопку открывающую меню настроек»). Открывают ровно то же, что одноимённые пункты
	// главного меню: адрес сообщества берётся из общей настройки проекта, экран настроек —
	// тот же самый. Второго адреса и второго экрана в проекте не заводится. ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FText SettingsText = NSLOCTEXT("PauseMenuWidget", "SettingsText", "Настройки");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FText CommunityText = NSLOCTEXT("PauseMenuWidget", "CommunityText", "Сообщество");

	// Пункт «Поддержать автора» (задание издателя, решение Рината 11.08.2026): вторая точка
	// входа в то же окно, что и из главного меню. Стоит рядом со строками «Сообщество» и
	// «Политика конфиденциальности» и оформлен так же, как они — выделять его нельзя.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FText SupportText = NSLOCTEXT("PauseMenuWidget", "SupportText", "Поддержать автора");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu", meta = (ClampMin = "8"))
	int32 ButtonFontSize = 19;

	// Кегль подписи «Политика конфиденциальности». Отдельный и мельче остальных: подпись
	// длинная (27 знаков) и при общем кегле не помещалась в кнопку — Ринат увидел её
	// обрезанной на телефоне 08-09.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu",
		meta = (DisplayName = "Кегль строки политики", ClampMin = "6"))
	int32 PolicyFontSize = 14;

	// Цвет подписей кнопок (тёмный — на светлой штатной кнопке UButton).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FLinearColor ButtonTextColor = FLinearColor(0.05f, 0.05f, 0.05f, 1.0f);

	// Габарит кнопки под палец (SizeBox: у UButton 5.5 нет SetPadding).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu")
	FVector2D ButtonSize = FVector2D(280.0f, 58.0f);

	// --- Б6: номер версии сборки мелкой строкой внизу панели (ADR-059). Сам текст версии
	// собирается кодом из настроек магазина, здесь только его вид. ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu",
		meta = (DisplayName = "Размер шрифта строки версии", ClampMin = "6"))
	int32 VersionFontSize = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu",
		meta = (DisplayName = "Цвет строки версии"))
	FLinearColor VersionColor = FLinearColor(0.6f, 0.6f, 0.6f, 1.0f);
};

/**
 * Меню паузы (этап G, меню-минимум по решению game-lead: пауза + «Продолжить» + «Выход»).
 *
 * ТЗ Рината 08-07 — два пути, как у UEndOfStoryWidget (архитектура ADR-048):
 *  - создан из WBP_PauseMenu (слот PauseMenuWidgetClass на контроллере) → дерево владельца
 *    из дизайнера, кубики по BindWidgetOptional-именам, цвета/шрифты код НЕ перекрашивает;
 *    тексты переключателя согласия, строки политики и номера версии код ставит всегда —
 *    они зависят от настроек и состояния (это данные, а не вид);
 *  - ассета нет / слот пуст → прежний кодовый вид (BuildCodeTree + FPauseMenuStyle).
 *
 * Виджет ТОЛЬКО рисует и сообщает о нажатиях; паузу мира ставит/снимает владелец
 * (AContrarySurvivorPlayerController::OpenPauseMenu/ClosePauseMenu). Кнопки работают при
 * паузе: Slate игровой паузой не останавливается.
 */
UCLASS()
class CONTRARYSURVIVOR_API UPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// «Продолжить» — владелец снимает паузу и убирает виджет.
	FSimpleMulticastDelegate OnResumeRequested;

	// «Выход» — владелец закрывает игру.
	FSimpleMulticastDelegate OnQuitRequested;

	// «В главное меню» — владелец закрывает паузу и открывает главное меню. Сигнал уходит
	// только когда решение окончательное: при несохранённом прогрессе виджет сначала
	// переспрашивает сам (спека).
	FSimpleMulticastDelegate OnMainMenuRequested;

	// «Настройки» — владелец открывает ТОТ ЖЕ экран настроек, что и из главного меню
	// (AContrarySurvivorPlayerController::OpenSettingsScreen). Пункт появляется сам по факту
	// привязки: не привязан — не показывается (тот же приём, что в главном меню).
	FSimpleMulticastDelegate OnSettingsRequested;

	// «Поддержать автора» — владелец открывает ТО ЖЕ окно, что и из главного меню
	// (AContrarySurvivorPlayerController::OpenSupportScreen). Второго окна не заводится.
	FSimpleMulticastDelegate OnSupportRequested;

	// Применяет стиль к уже построенному дереву (NativeOnInitialized отработал в CreateWidget
	// с дефолтами). Зовёт контроллер сразу после создания виджета (OpenPauseMenu).
	void ApplyStyle(const FPauseMenuStyle& Style);

	// Есть ли несохранённый прогресс (ставит владелец при каждом открытии паузы). От этого
	// зависит, спросит ли «В главное меню» подтверждение.
	void SetProgressUnsaved(bool bInUnsaved);

	// Идёт ли сейчас переспрос «Выйти в меню без сохранения?» (для владельца и автотестов).
	bool IsConfirmingMainMenu() const { return bConfirmingMainMenu; }

	// Чистое правило (покрыто автотестом): переспрашиваем ровно тогда, когда есть что терять.
	static bool ShouldConfirmMainMenu(bool bProgressUnsaved);

	// --- Обработчики. ПУБЛИЧНЫЕ намеренно: их зовут и клики кнопок, и headless-тесты
	// (живой Slate в Automation-тестах проекта не поднимается — паттерн StartScreenWidget). ---

	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleQuitClicked();

	// «В главное меню»: при несохранённом прогрессе первое нажатие ТОЛЬКО переспрашивает,
	// второе («Да, выйти») отправляет сигнал владельцу. Когда терять нечего — уходим сразу.
	UFUNCTION()
	void HandleMainMenuClicked();

	// «Настройки» — просто просит владельца открыть экран настроек поверх паузы.
	UFUNCTION()
	void HandleSettingsClicked();

	// «Сообщество» — открывает адрес из настройки проекта во внешнем браузере. Адрес тот же,
	// что у пункта главного меню (UMainMenuSettings::GetCommunityUrl) — второго не заводим.
	UFUNCTION()
	void HandleCommunityClicked();

	// «Поддержать автора» — просто просит владельца открыть окно. Ничего не решает сам.
	UFUNCTION()
	void HandleSupportClicked();

protected:
	virtual void NativeOnInitialized() override;

	// Виджет создаётся один раз и добавляется на экран при каждом открытии паузы, поэтому
	// подпись переключателя согласия и строку версии освежаем именно здесь (Б6).
	virtual void NativeConstruct() override;

	// Модальный барьер: клик/тап мимо кнопок гасится здесь и в мир не проходит
	// (затемнение-подложка Visible ловит хит-тест, событие всплывает сюда).
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;

	// Переключатель согласия из паузы УБРАН решением Рината 08-08 (согласие спрашивается
	// только на экране согласия при первом запуске). Обработчик и кубики удалены совсем
	// 08-09: невидимая кнопка оставалась в ассете и мешала — она лежала ровно под новой
	// «В главное меню» и валила проверку раскладки. Отзыв согласия живёт на экране согласия.

	// Б6: строка «Политика конфиденциальности». Адрес живёт в настройке проекта; пока он
	// пуст, нажатие ничего не делает и пустую страницу не открывает.
	UFUNCTION()
	void HandlePolicyClicked();

private:
	// Строит прежнее кодовое дерево (путь «ассета нет»). Имена кубиков = именам полей —
	// те же, что генерирует коммандлет в WBP_PauseMenu.
	void BuildCodeTree();

	// Подпись переключателя согласия и строка версии сборки по текущему состоянию.
	void RefreshConsentAndVersion();

	// Подписи обычного вида и режима переспроса «Выйти в меню без сохранения?». На время
	// переспроса остаются две кнопки: «Отмена» (бывшая «Продолжить») и «Да, выйти» (бывшая
	// «В главное меню»), остальные прячутся, чтобы случайный тап рядом не увёл с вопроса.
	void ApplyNormalLabels();
	void ApplyConfirmMainMenuLabels();

	// Скрыть/показать пункт ЦЕЛИКОМ: в кодовом дереве кнопка обёрнута в SizeBox — прятать
	// надо обёртку, иначе в колонке останется пустое место (урок AmmoRow).
	static void SetRowVisibility(class UWidget* Widget, ESlateVisibility InVisibility);

	// Стиль, переданный ApplyStyle — нужен, чтобы переключать подписи без пересоздания дерева.
	FPauseMenuStyle CachedStyle;

	// Идёт переспрос «Выйти в меню без сохранения?».
	bool bConfirmingMainMenu = false;

	// Пункты «Выход» и «Политика» спрятаны НАМИ на время вопроса (и только тогда их видимость
	// возвращается): владелец мог скрыть что-то из них сам, и наш переспрос не вправе это менять.
	bool bRowsHiddenByConfirm = false;
	ESlateVisibility SavedQuitVisibility = ESlateVisibility::Visible;
	ESlateVisibility SavedPolicyVisibility = ESlateVisibility::Visible;

	// Подписи, с которыми панель пришла из дизайнера. Переспрос временно меняет заголовок и
	// подпись «Продолжить», а выход из переспроса обязан вернуть ИМЕННО их, а не значения
	// стиля: иначе первый же отменённый переспрос затёр бы тексты, набранные владельцем в WBP.
	FText OriginalTitleText;
	FText OriginalResumeText;

	// Есть ли несохранённый прогресс (ставит владелец). По умолчанию true — безопасная
	// сторона: лучше лишний раз спросить, чем молча потерять прохождение.
	bool bProgressUnsaved = true;

	// Кнопка меню с подписью, обёрнутая в SizeBox тач-размера (мин. высота под палец),
	// добавленная в колонку. Возвращает кнопку для подписки OnClicked.
	UButton* MakeMenuButton(UVerticalBox* Column, const FText& Label, const FName& BaseName);

	// --- Кубики: из WBP по BindWidgetOptional ЛИБО из BuildCodeTree (имена совпадают) ---

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> DimBorder;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ResumeButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ResumeText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> QuitButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuitText;

	// Подход 3 волны меню: возврат в главное меню.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MainMenuButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MainMenuText;

	// Волна 08-09: «Настройки» и «Сообщество» прямо из паузы.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SettingsButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SettingsText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CommunityButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CommunityText;

	// Задание издателя: вторая точка входа в окно «Поддержать автора».
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SupportButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SupportText;

	// Б6: строка политики и мелкий номер версии сборки (переключатель согласия отсюда убран).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> PolicyButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PolicyText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> VersionText;

	// Двойная рамка кодового фолбэка (в WBP её нет — там одна плашка PanelPlate с кантом).
	UPROPERTY()
	TObjectPtr<UBorder> FrameBorder;

	UPROPERTY()
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY()
	TArray<TObjectPtr<class USizeBox>> ButtonBoxes;

	// Дерево пришло из WBP-ассета (детект в NativeOnInitialized, как TouchControlsWidget.cpp).
	bool bDesignerTree = false;
};
