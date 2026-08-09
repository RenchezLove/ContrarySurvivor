// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StartScreenWidget.generated.h"

class UButton;
class UVerticalBox;
class UBorder;
class UTextBlock;
class USizeBox;
class UWidget;
class UImage;     // фон и логотип меню (переделка 08-09)
class UTexture2D; // мягкие ссылки на картинку фона и логотип в FStartScreenStyle

/**
 * Настройки главного меню (волна «Главное меню» 08-08, ADR-062).
 *
 * Адрес сообщества живёт в КОНФИГЕ, а не в коде: Config/DefaultGame.ini, раздел
 * [/Script/ContrarySurvivor.MainMenuSettings] — по спеке главного меню («Адрес берётся из
 * конфига, не зашит в код»). Пока строка пуста, пункт «Сообщество» спрятан целиком; вписать
 * адрес можно текстовым редактором, пересобирать код не нужно. Тот же приём — у адреса
 * канала (UEndOfStorySettings) и у политики (UDataConsentSettings::PrivacyPolicyUrl).
 */
UCLASS(Config = Game, DefaultConfig)
class CONTRARYSURVIVOR_API UMainMenuSettings : public UObject
{
	GENERATED_BODY()

public:
	// Адрес нашего Telegram для пункта «Сообщество». Пусто — пункт в меню не показывается.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Главное меню",
		meta = (DisplayName = "Адрес сообщества для пункта «Сообщество»"))
	FString CommunityUrl;

	// Адрес Telegram для пункта «Сообщить об ошибке» на экране настроек (подход 2 волны меню).
	// Строка отдельная — дверь на будущее, если издатель заведёт под отчёты об ошибках свой
	// чат. Пока она пуста, отчёты идут в тот же чат, что и «Сообщество» (решение game-lead
	// 08-09: канал у нас ОДИН). Пусты обе строки — пункт спрятан целиком.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Главное меню",
		meta = (DisplayName = "Адрес для пункта «Сообщить об ошибке» (пусто — берётся адрес сообщества)"))
	FString BugReportUrl;

	// Готовый адрес из конфига (без лишних пробелов) либо пустая строка.
	static FString GetCommunityUrl();

	// Собственный адрес отчётов об ошибках, как он записан в конфиге (без запасного пути).
	static FString GetBugReportUrl();

	// Адрес, по которому реально уходит «Сообщить об ошибке»: свой, а если его нет — адрес
	// сообщества. Пусто — только когда не заполнено ни то, ни другое.
	static FString GetEffectiveBugReportUrl();
};

/**
 * Стиль главного меню (бывший стартовый экран Б3; расширен волной «Главное меню» 08-08,
 * ADR-062). Живёт EditAnywhere-полем на контроллере.
 *
 * ⚠ Цвета/шрифты/размеры действуют ТОЛЬКО для кодового дерева-фолбэка. Если экрану назначен
 * ассет WBP_StartScreen (слот StartScreenWidgetClass на контроллере, ТЗ Рината 08-07), вид
 * правится мышкой в дизайнере; из стиля продолжают действовать ТЕКСТЫ — они переключаются
 * кодом между обычным выбором и переспросом «Новая игра» (это данные, а не вид).
 *
 * Локализация (ADR-050): подписи — FText с дефолтами через NSLOCTEXT (LOCTEXT в значении по
 * умолчанию UHT запрещает — UhtTextProperty.cs:104).
 */
USTRUCT(BlueprintType)
struct FStartScreenStyle
{
	GENERATED_BODY()

	// --- Фон меню (переделка 08-09 по кадру с телефона). Спека: «Фон меню статичный, без
	// анимаций». До переделки фоновой картинки не было вовсе, и сквозь полупрозрачное
	// затемнение просвечивал живой мир игры — именно это Ринат и увидел на устройстве. ---

	// Картинка фона на весь экран. Пусто — рисуем сплошную заливку BackgroundFallbackColor
	// (меню остаётся рабочим и читаемым, пока художник готовит кадр). Мягкая ссылка —
	// паттерн иконок проекта: нет ассета, значит просто не грузим, без падений.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen|Фон",
		meta = (DisplayName = "Картинка фона (пусто — сплошная заливка)"))
	TSoftObjectPtr<UTexture2D> BackgroundTexture;

	// Фирменный тёмный цвет подложки иконки приложения (#271D14) в линейном виде, каким его
	// ждёт движок. Заливка НЕПРОЗРАЧНАЯ: сквозь меню не должно быть видно игру.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen|Фон",
		meta = (DisplayName = "Цвет заливки, если картинки нет"))
	FLinearColor BackgroundFallbackColor = FLinearColor(0.0203f, 0.0132f, 0.0070f, 1.0f);

	// Логотип игры слева (просьба Рината). Пусто — логотип не рисуется вовсе, раскладка не
	// ломается: заголовок и кнопки остаются на своих местах.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen|Фон",
		meta = (DisplayName = "Логотип игры (пусто — не показывать)"))
	TSoftObjectPtr<UTexture2D> LogoTexture;

	// Затемнение нижней части экрана, чтобы мелкие строки версии и политики читались на
	// любой картинке. Доля высоты экрана и непрозрачность у самого низа.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen|Фон",
		meta = (DisplayName = "Высота затемнения снизу (доля экрана)", ClampMin = "0.0", ClampMax = "1.0"))
	float BottomShadeHeightFraction = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen|Фон",
		meta = (DisplayName = "Непрозрачность затемнения у нижнего края", ClampMin = "0.0", ClampMax = "1.0"))
	float BottomShadeOpacity = 0.75f;

	// Затемнение поверх фона. По умолчанию ПРОЗРАЧНОЕ: непрозрачность даёт сам фон, а этот
	// слой нужен только чтобы ловить касания мимо кнопок (модальный барьер).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FLinearColor DimColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

	// Золотой кант панели и тёмный фон панели (палитра модалок HUD, как у меню паузы).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FLinearColor FrameColor = FLinearColor(0.8f, 0.65f, 0.25f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FLinearColor PanelColor = FLinearColor(0.06f, 0.07f, 0.09f, 0.97f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FText TitleText = NSLOCTEXT("StartScreenWidget", "TitleText", "С ВОЗВРАЩЕНИЕМ");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen", meta = (ClampMin = "8"))
	int32 TitleFontSize = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FLinearColor TitleColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);

	// Б3: найден сейв с настоящим прогрессом — предлагаем «Продолжить» или честно начать заново.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FText SubtitleText = NSLOCTEXT("StartScreenWidget", "SubtitleText", "Найдено сохранение прошлой игры.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen", meta = (ClampMin = "8"))
	int32 SubtitleFontSize = 15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FLinearColor SubtitleColor = FLinearColor(0.85f, 0.85f, 0.85f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FText ContinueText = NSLOCTEXT("StartScreenWidget", "ContinueText", "Продолжить");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FText NewGameText = NSLOCTEXT("StartScreenWidget", "NewGameText", "Новая игра");

	// --- Пункты главного меню (спека glavnoe-menu-spec.md, ADR-062) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FText SettingsText = NSLOCTEXT("StartScreenWidget", "SettingsText", "Настройки");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FText CommunityText = NSLOCTEXT("StartScreenWidget", "CommunityText", "Сообщество");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FText ExitText = NSLOCTEXT("StartScreenWidget", "ExitText", "Выход");

	// Мелкая строка версии и ссылка политики внизу (спека: «мелким шрифтом, не кнопками»).
	// Сами тексты — данные: версию собирает UDataConsentSubsystem, подпись политики живёт в
	// UDataConsentSettings (переиспользован механизм меню паузы); здесь только вид.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen",
		meta = (DisplayName = "Размер шрифта строк версии и политики", ClampMin = "6"))
	int32 VersionFontSize = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen",
		meta = (DisplayName = "Цвет строк версии и политики"))
	FLinearColor VersionColor = FLinearColor(0.6f, 0.6f, 0.6f, 1.0f);

	// --- Подтверждение «Новая игра» (решение лида 08-05: случайное касание на телефоне
	// стирает чужой прогресс без возможности отмены — нужен один явный переспрос). Первый клик
	// по «Новая игра» НЕ стирает сейв — панель переключается в этот режим (те же две кнопки,
	// подписи меняются); «Отмена» возвращает обычный выбор, «Да» стирает по-настоящему.
	// Тексты — дословно из спеки главного меню («Начать заново? Текущий прогресс будет
	// удалён»); переспрос идёт ТОЛЬКО поверх существующего сейва (без сейва стирать нечего). ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen|Confirm New Game")
	FText ConfirmTitleText = NSLOCTEXT("StartScreenWidget", "ConfirmTitleText", "Начать заново?");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen|Confirm New Game")
	FText ConfirmSubtitleText = NSLOCTEXT("StartScreenWidget", "ConfirmSubtitleText",
		"Текущий прогресс будет удалён.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen|Confirm New Game")
	FText ConfirmYesText = NSLOCTEXT("StartScreenWidget", "ConfirmYesText", "Да, начать заново");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen|Confirm New Game")
	FText ConfirmCancelText = NSLOCTEXT("StartScreenWidget", "ConfirmCancelText", "Отмена");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen", meta = (ClampMin = "8"))
	int32 ButtonFontSize = 19;

	// Цвет подписей кнопок (тёмный — на светлой штатной кнопке UButton).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FLinearColor ButtonTextColor = FLinearColor(0.05f, 0.05f, 0.05f, 1.0f);

	// Габарит кнопки под палец (SizeBox: у UButton 5.5 нет SetPadding).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen")
	FVector2D ButtonSize = FVector2D(280.0f, 58.0f);

	// --- Раскладка «логотип слева, кнопки справа» (просьба Рината 08-09: телефон лежит
	// горизонтально, кнопки должны попадать под большой палец правой руки). Поля действуют
	// ТОЛЬКО для кодового дерева-фолбэка; в живом WBP раскладку двигает владелец мышкой. ---

	// Размер логотипа, точек. Логотип прижат к ЛЕВОМУ краю и центрирован по высоте.
	// Дефолт квадратный под присланный логотип «МАРЕВО» (картинка 1000x1000, сама надпись
	// занимает примерно центральную треть высоты — поэтому квадрат берём с запасом, иначе
	// надпись на телефоне выйдет мелкой).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen|Раскладка",
		meta = (DisplayName = "Размер логотипа"))
	FVector2D LogoSize = FVector2D(420.0f, 420.0f);

	// Отступ логотипа от левого края экрана, точек.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen|Раскладка",
		meta = (DisplayName = "Отступ логотипа от левого края"))
	float LogoLeftMargin = 56.0f;

	// Отступ столбика кнопок от ПРАВОГО края экрана, точек.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen|Раскладка",
		meta = (DisplayName = "Отступ кнопок от правого края"))
	float ButtonsRightMargin = 64.0f;

	// Вертикальный зазор между кнопками, точек.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen|Раскладка",
		meta = (DisplayName = "Зазор между кнопками", ClampMin = "0.0"))
	float ButtonSpacing = 12.0f;
};

/**
 * Главное меню (волна «Главное меню» 08-08, ADR-062; вырос из стартового экрана Б3).
 * Показывается со ВТОРОГО запуска игры (AContrarySurvivorPlayerController::MaybeStartIntro);
 * самый первый запуск после установки идёт сразу во вступление, без меню (спека).
 *
 * Состав сверху вниз (спека glavnoe-menu-spec.md): «Продолжить» (без сейва пункт не
 * показывается вовсе), «Новая игра» (с переспросом только поверх сейва), «Настройки»
 * (вход-заглушка до подхода 2), «Сообщество» (спрятан, пока в конфиге нет адреса), «Выход»;
 * внизу мелко — версия сборки и ссылка «Политика конфиденциальности» (механизм меню паузы).
 * Фон статичный, без анимаций.
 *
 * ТЗ Рината 08-07 — два пути, как у UEndOfStoryWidget (архитектура ADR-048):
 *  - создан из WBP_StartScreen (слот StartScreenWidgetClass на контроллере) → дерево
 *    владельца из дизайнера, кубики по BindWidgetOptional-именам, код не перекрашивает
 *    (тексты кнопок код ПЕРЕКЛЮЧАЕТ — режим переспроса «Новая игра», это данные);
 *  - ассета нет / слот пуст → прежний кодовый вид (BuildCodeTree + FStartScreenStyle).
 * Виджет ТОЛЬКО рисует и сообщает о нажатиях; владелец (контроллер) решает, что грузить/стирать.
 */
UCLASS()
class CONTRARYSURVIVOR_API UStartScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// «Продолжить» — владелец грузит сейв (APlayerCharacter::LoadGameForContinue), интро не играет.
	FSimpleMulticastDelegate OnContinueRequested;

	// «Новая игра» — владелец стирает сейв (ResetToNewGame) и запускает интро с нуля.
	FSimpleMulticastDelegate OnNewGameRequested;

	// «Настройки» — владелец открывает экран настроек (подход 2; пока вход-заглушка).
	FSimpleMulticastDelegate OnSettingsRequested;

	// «Выход» — владелец закрывает игру.
	FSimpleMulticastDelegate OnExitRequested;

	// Применяет стиль к уже построенному дереву (NativeOnInitialized отработал в CreateWidget
	// с дефолтами). Зовёт контроллер сразу после создания виджета (OpenStartScreen). Запоминает
	// стиль (CachedStyle) — переспрос «Новая игра» и отмена переключают подписи без пересоздания.
	// При дизайнер-дереве меняет только тексты (см. FStartScreenStyle).
	void ApplyStyle(const FStartScreenStyle& Style);

	// Есть ли сейв с реальным прогрессом (ставит владелец при открытии). Без сейва пункт
	// «Продолжить» не показывается вовсе (не гаснет серым — спека), а «Новая игра» стартует
	// без переспроса (стирать нечего).
	void SetHasSave(bool bInHasSave);

	// Идёт ли сейчас переспрос «Начать заново?» (для автотестов и владельца).
	bool IsConfirmingNewGame() const { return bConfirmingNewGame; }

	// --- Чистые правила меню (покрыты автотестами ContrarySurvivor.MainMenu) ---

	// «Продолжить»: без сейва — Collapsed (пункта нет вовсе, место не занимает), с сейвом — Visible.
	static ESlateVisibility ContinueVisibilityFor(bool bInHasSave);

	// Переспрос «Новая игра» нужен только поверх существующего сейва.
	static bool ShouldConfirmNewGame(bool bInHasSave);

	// «Сообщество»: пустой/пробельный адрес в конфиге — пункт спрятан целиком.
	static ESlateVisibility CommunityVisibilityFor(const FString& CommunityUrl);

	// Логотип: текстура не назначена — пункт не рисуется вовсе (раскладка при этом цела).
	static ESlateVisibility LogoVisibilityFor(const TSoftObjectPtr<UTexture2D>& LogoTexture);

	// Нужна ли сплошная заливка вместо картинки (переделка 08-09): картинки нет — да.
	// Фон обязан быть непрозрачным в любом случае, сквозь меню игру видно быть не должно.
	static bool ShouldFillBackgroundWithColor(const TSoftObjectPtr<UTexture2D>& BackgroundTexture);

	// --- Обработчики кнопок. ПУБЛИЧНЫЕ намеренно: их зовут и клики кнопок, и headless-тесты
	// меню (живой Slate в Automation-тестах проекта не поднимается — паттерн DeathScreenWidget). ---

	// «Продолжить» в обычном режиме; в режиме переспроса эта же кнопка подписана «Отмена»
	// и возвращает обычный выбор, а не грузит сейв.
	UFUNCTION()
	void HandleContinueClicked();

	// «Новая игра»: поверх сейва первый клик ТОЛЬКО включает переспрос (сейв ещё цел), второй
	// («Да, начать заново») стирает по-настоящему; без сейва — сразу новая игра, без переспроса.
	UFUNCTION()
	void HandleNewGameClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	// «Сообщество»: открывает адрес из конфига во внешнем браузере (пункт виден только при
	// непустом адресе, так что пустой адрес сюда штатно не попадает).
	UFUNCTION()
	void HandleCommunityClicked();

	UFUNCTION()
	void HandleExitClicked();

	// Ссылка «Политика конфиденциальности» — тот же механизм, что в меню паузы
	// (UDataConsentSubsystem::OpenPrivacyPolicy; пустой адрес — тихо ничего не делает).
	UFUNCTION()
	void HandlePolicyClicked();

protected:
	virtual void NativeOnInitialized() override;

	// Виджет добавляется на экран при открытии меню — здесь освежаются данные-строки:
	// версия сборки, подпись политики, видимость «Сообщества» по конфигу (паттерн меню паузы).
	virtual void NativeConstruct() override;

	// Модальный барьер: клик/тап мимо кнопок гасится здесь и в мир не проходит.
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;

private:
	// Строит прежнее кодовое дерево (путь «ассета нет»). Имена кубиков = именам полей —
	// те же, что генерирует коммандлет в WBP_StartScreen.
	void BuildCodeTree();

	// Кнопка меню с подписью, обёрнутая в SizeBox тач-размера, добавленная в колонку.
	UButton* MakeMenuButton(UVerticalBox* Column, const FText& Label, const FName& BaseName);

	// Геометрия кодового дерева по полям стиля: размер и отступ логотипа, отступ колонки
	// кнопок от правого края, зазор между кнопками, полосы затемнения внизу. Только для
	// кодового пути — в живом WBP раскладку двигает владелец.
	void ApplyCodeTreeLayout(const FStartScreenStyle& Style);

	// Подписи панели/кнопок и видимость пунктов для текущего режима (обычный выбор либо
	// переспрос «Новая игра»: на время переспроса лишние пункты меню прячутся, остаются две
	// кнопки «Отмена»/«Да, начать заново»). При кодовом дереве заодно ставит шрифт/цвет;
	// при дизайнер-дереве — только тексты и видимость.
	void ApplyChoiceLabels(const FStartScreenStyle& Style);
	void ApplyConfirmLabels(const FStartScreenStyle& Style);

	// Видимость пунктов обычного выбора: «Продолжить» — по наличию сейва (вместе с
	// подзаголовком про найденное сохранение), «Настройки» — когда владелец привязал
	// открытие экрана настроек (до подхода 2 пункт спрятан), «Сообщество» — по адресу
	// из конфига, «Выход» — всегда.
	void ApplyMenuRowVisibility();

	// Версия сборки, подпись политики, видимость «Сообщества» — данные из настроек/подсистем
	// (механизм меню паузы), освежаются при каждом показе меню (NativeConstruct).
	void RefreshMenuExtras();

	// Скрыть/показать пункт меню ЦЕЛИКОМ: в кодовом дереве кнопка обёрнута в SizeBox
	// тач-габарита — прятать надо обёртку, иначе колонка сохранит пустое место под пунктом.
	static void SetRowVisibility(UWidget* Widget, ESlateVisibility InVisibility);

	// Стиль, переданный ApplyStyle — нужен, чтобы переключаться между обычным выбором и
	// переспросом «Новая игра» без пересоздания дерева.
	FStartScreenStyle CachedStyle;

	// Идёт переспрос «Начать заново?» (кнопки временно переподписаны).
	bool bConfirmingNewGame = false;

	// Есть ли сейв с реальным прогрессом (SetHasSave). По умолчанию true — прежнее поведение
	// Б3 (экран открывался только при найденном сейве) не меняется без явного вызова.
	bool bHasSave = true;

	// --- Кубики: из WBP по BindWidgetOptional ЛИБО из BuildCodeTree (имена совпадают) ---

	// Фон на весь экран: картинка либо сплошная заливка (переделка 08-09).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UImage> BackgroundImage;

	// Логотип игры слева. Текстуры нет — кубик схлопнут.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UImage> LogoImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> DimBorder;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SubtitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ContinueButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ContinueText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> NewGameButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NewGameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SettingsButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SettingsText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CommunityButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CommunityText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ExitButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ExitText;

	// Низ панели (спека: «мелким шрифтом, не кнопками»): ссылка политики — прозрачная
	// кнопка, видна только подпись (как на экране согласия); строка версии — просто текст.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> PolicyButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PolicyText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> VersionText;

	// Ступенчатое затемнение нижней части экрана (несколько полос с растущей непрозрачностью):
	// в UMG готового виджета-градиента нет, а заводить ради этого материал-ассет — лишняя
	// зависимость. На тёмном фоне ступени не видны, зато работает на любом экране без ассетов.
	UPROPERTY()
	TArray<TObjectPtr<UBorder>> BottomShadeBands;

	// Двойная рамка кодового фолбэка (в WBP её нет — там одна плашка PanelPlate с кантом).
	UPROPERTY()
	TObjectPtr<UBorder> FrameBorder;

	UPROPERTY()
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY()
	TArray<TObjectPtr<USizeBox>> ButtonBoxes;

	// Дерево пришло из WBP-ассета (детект в NativeOnInitialized, как TouchControlsWidget.cpp).
	bool bDesignerTree = false;
};
