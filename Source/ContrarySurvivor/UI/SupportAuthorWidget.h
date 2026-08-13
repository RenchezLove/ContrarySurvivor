// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h" // FButtonStyle — вид кнопок общий для кода и генератора
#include "SupportAuthorWidget.generated.h"

class UButton;
class UBorder;
class UTextBlock;
class UVerticalBox;
class USizeBox;
class UWidget;

/**
 * Оформление и тексты окна «Поддержать автора» (задание издателя, решение Рината 11.08.2026).
 * Живёт EditAnywhere-полем на контроллере — тот же приём, что у меню паузы и главного меню.
 *
 * ⚠ Цвета, шрифты и размеры действуют ТОЛЬКО для кодового дерева-запаски. Если окну назначен
 * готовый ассет из дизайнера (слот SupportWidgetClass на контроллере), вид правится мышкой;
 * из этих полей продолжают действовать ТЕКСТЫ — они данные, а не оформление.
 *
 * ⛔ У ДВУХ КНОПОК ОДИН РАЗМЕР И ОДИН СТИЛЬ — это дословное условие задания: «Обе кнопки
 * оформляются одинаково… Ни одну из них не выделяем и не приглушаем». Поэтому отдельных полей
 * под вторую кнопку здесь НЕТ и заводить их нельзя: разойтись им попросту нечем.
 */
USTRUCT(BlueprintType)
struct FSupportAuthorStyle
{
	GENERATED_BODY()

	// Затемнение экрана под окном (оно же ловит касания мимо кнопок).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Поддержать автора")
	FLinearColor DimColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.6f);

	// Кант и заливка самого окна — палитра остальных окон игры.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Поддержать автора")
	FLinearColor FrameColor = FLinearColor(0.8f, 0.65f, 0.25f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Поддержать автора")
	FLinearColor PanelColor = FLinearColor(0.06f, 0.07f, 0.09f, 0.97f);

	// --- Тексты. Дословно из задания издателя, менять формулировки нельзя. ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Поддержать автора|Тексты")
	FText TitleText = NSLOCTEXT("SupportAuthorWidget", "Title", "Поддержать автора");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Поддержать автора|Тексты",
		meta = (MultiLine = "true"))
	FText MessageText = NSLOCTEXT("SupportAuthorWidget", "Message",
		"Игру делает один человек. Самый простой способ помочь — посмотреть короткую рекламу: мне за это заплатят, с тебя ничего.");

	// ⛔ Слово «реклама» из подписи убирать нельзя (дословное условие задания).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Поддержать автора|Тексты")
	FText WatchAdText = NSLOCTEXT("SupportAuthorWidget", "WatchAd", "Посмотреть рекламу");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Поддержать автора|Тексты")
	FText SupportLinkText = NSLOCTEXT("SupportAuthorWidget", "SupportLink", "Другие способы поддержать");

	// Строка после просмотра ролика. Показывается В ЭТОМ ЖЕ окне: отдельное окно поверх окна
	// задание запрещает. Текст сокращён до одного слова по решению Рината 13.08.2026
	// (дословно: «Оставь просто "Спасибо", без "...это реально помогает"»).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Поддержать автора|Тексты")
	FText ThanksText = NSLOCTEXT("SupportAuthorWidget", "Thanks", "Спасибо");

	// Подсказка, куда делась кнопка просмотра (задача Рината 13.08.2026: после ролика кнопка
	// исчезает, пока следующий не загрузится, и игрок её теряет). Текст дословно из решения
	// лида, ADR-072. ⛔ Формулировка БЕЗ сроков и без «скоро» намеренно: запрет издателя из
	// шапки задания (см. условия класса ниже) действует и на эту строку — ранние варианты
	// «через 30–60 секунд» и «через несколько секунд» его нарушали. Вдобавок числа были бы
	// неправдой: по журналу телефона следующий ролик грузится меньше секунды
	// (phone-1313-b2.log:1233–1258). Показывается ТОЛЬКО вместе со строкой благодарности и
	// только пока следующий ролик не готов; при обычном открытии окна без готового ролика её
	// нет — окно с одной кнопкой не шумит (то же требование издателя).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Поддержать автора|Тексты",
		meta = (MultiLine = "true"))
	FText NextAdHintText = NSLOCTEXT("SupportAuthorWidget", "NextAdHint",
		"Новый ролик уже загружается — загляните в это окно ещё раз.");

	// Подпись крестика закрытия.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Поддержать автора|Тексты")
	FText CloseText = NSLOCTEXT("SupportAuthorWidget", "Close", "X");

	// --- Размеры и шрифты (только для кодового дерева-запаски) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Поддержать автора", meta = (ClampMin = "8"))
	int32 TitleFontSize = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Поддержать автора", meta = (ClampMin = "8"))
	int32 MessageFontSize = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Поддержать автора", meta = (ClampMin = "8"))
	int32 ButtonFontSize = 18;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Поддержать автора")
	FLinearColor TitleColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Поддержать автора")
	FLinearColor MessageColor = FLinearColor(0.9f, 0.9f, 0.9f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Поддержать автора")
	FLinearColor ButtonTextColor = FLinearColor(0.8469f, 0.8388f, 0.7991f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Поддержать автора")
	FLinearColor ButtonFillColor = FLinearColor(0.0080f, 0.0044f, 0.0027f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Поддержать автора")
	FLinearColor ButtonBorderColor = FLinearColor(0.0423f, 0.0331f, 0.0273f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Поддержать автора", meta = (ClampMin = "0.0"))
	float ButtonCornerRadius = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Поддержать автора", meta = (ClampMin = "0.0"))
	float ButtonBorderWidth = 2.0f;

	// ОДИН размер на ОБЕ кнопки — см. предупреждение в шапке структуры.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Поддержать автора")
	FVector2D ButtonSize = FVector2D(460.0f, 72.0f);

	// Ширина окна (высота растёт за содержимым).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Поддержать автора", meta = (ClampMin = "200.0"))
	float WindowWidth = 620.0f;
};

/**
 * Окно «Поддержать автора» (задание издателя, решение Рината 11.08.2026).
 *
 * Что это по смыслу: добровольная точка показа рекламы, куда игрок приходит САМ. Дословно из
 * задания: «Ни один экран её не навязывает, она не прерывает игру и ничего не даёт взамен».
 * Открывается ровно двумя пунктами — в главном меню и в меню паузы; сама игра его не
 * показывает никогда, напоминаний и всплытий нет.
 *
 * ⛔ Жёсткие условия задания, которые нарушать нельзя:
 *   • обе кнопки одинаковые по размеру и стилю, ни одна не выделена и не приглушена;
 *   • порядок кнопок менять нельзя: сначала «Посмотреть рекламу», потом «Другие способы»;
 *   • слово «реклама» из подписи кнопки убирать нельзя;
 *   • за просмотр ролика игроку НИЧЕГО не начисляется;
 *   • ролик не готов — кнопки просмотра нет ВОВСЕ (не серая и не с надписью «недоступно»);
 *   • в окне не должно быть сроков, слова «скоро», обещаний про бесплатность и назначение
 *     денег, счётчиков собранного и полосок прогресса.
 *
 * Устройство — два пути, как у UStartScreenWidget и UPauseMenuWidget (архитектура ADR-048):
 *  - создано из готового ассета (слот SupportWidgetClass на контроллере) → дерево из
 *    дизайнера, кубики приходят по именам, код не перекрашивает;
 *  - ассета нет / слот пуст → кодовое дерево-запаска, оформление из FSupportAuthorStyle.
 *
 * Виджет ТОЛЬКО рисует и сообщает о нажатиях; показывать ролик и открывать ссылку — дело
 * владельца (AContrarySurvivorPlayerController).
 */
UCLASS()
class CONTRARYSURVIVOR_API USupportAuthorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// «Посмотреть рекламу» — владелец показывает ролик.
	FSimpleMulticastDelegate OnWatchAdRequested;

	// «Другие способы поддержать» — владелец открывает внешнюю ссылку.
	FSimpleMulticastDelegate OnSupportLinkRequested;

	// Крестик — владелец закрывает окно. Ту же дорогу проходит аппаратная кнопка «Назад».
	FSimpleMulticastDelegate OnCloseRequested;

	// Применяет оформление и тексты. Зовёт владелец сразу после создания окна.
	void ApplyStyle(const FSupportAuthorStyle& Style);

	// Готов ли ролик к показу. Не готов — кнопки просмотра нет вовсе (условие задания).
	// Ставит владелец при КАЖДОМ открытии окна: между открытиями ролик мог и загрузиться,
	// и разгрузиться.
	void SetAdAvailable(bool bInAdAvailable);

	// Показать строку благодарности после просмотра. Отдельного окна поверх окна не заводим —
	// это прямой запрет задания. Заодно взводит подсказку про следующий ролик: покажется она
	// или нет — решает правило ShouldShowNextAdHint по готовности следующего ролика.
	void ShowThanks();

	// Сброс подсказки про следующий ролик. Зовёт владелец при КАЖДОМ открытии окна: подсказка
	// живёт от конца ролика до закрытия окна, а при обычном открытии без готового ролика её
	// быть не должно (окно с одной кнопкой — не шуметь, требование издателя).
	void ResetNextAdHint();

	// Идёт ли сейчас показ ролика (владелец ставит на время показа, чтобы повторный тап по
	// кнопке не запустил второй ролик).
	void SetAdInProgress(bool bInProgress) { bAdInProgress = bInProgress; }
	bool IsAdInProgress() const { return bAdInProgress; }

	// --- Чистые правила (покрыты автотестами ContrarySurvivor.SupportAuthor) ---

	// Видимость кнопки просмотра: ролик не готов — Collapsed, то есть пункта нет вовсе и
	// места он не занимает; готов — Visible.
	static ESlateVisibility WatchAdVisibilityFor(bool bAdReady);

	// ⛔ Показывать ли кнопку просмотра. Зависит РОВНО от готовности ролика: порог по
	// игровому времени (правило РИ-29) к этой точке не применяется — дословно из задания,
	// «Кнопка доступна всегда». Отдельная функция нужна именно затем, чтобы это условие было
	// написано в одном месте и его нельзя было тихо дополнить проверкой порога.
	static bool ShouldShowWatchAdButton(bool bAdReady);

	// ⛔ Начисляется ли что-нибудь игроку за просмотр этого ролика. Всегда ложь: «награда
	// превращает её в обычную ежедневку и убивает замер» (дословно из задания).
	static bool GrantsRewardForWatching();

	// Показывать ли подсказку «новый ролик уже загружается». Ровно два условия:
	// ролик закончился при текущем открытии окна (игрок видел, как исчезла кнопка) И следующий
	// ещё не готов. Готов — кнопка на месте и объяснять нечего; ролика не было — окно открыто
	// обычным способом, и подсказка запрещена (окно с одной кнопкой — не шуметь).
	static bool ShouldShowNextAdHint(bool bAdJustFinished, bool bAdReady);

	// Вид кнопки окна. ОДИН метод на обе кнопки и БЕЗ признака «главная»: разойтись им нечем.
	static FButtonStyle MakeSupportButtonStyle(const FSupportAuthorStyle& Style);

	// Нижняя граница области нажатия, точек (действующее требование Б8 — не меньше 48).
	// Ниже неё габарит кнопок и крестика не опускается, что бы ни стояло в настройке.
	static float GetMinTouchSizePx();

	// --- Обработчики. ПУБЛИЧНЫЕ намеренно: их зовут и клики кнопок, и автотесты (живой Slate
	// в тестах проекта не поднимается — паттерн UStartScreenWidget). ---

	UFUNCTION()
	void HandleWatchAdClicked();

	UFUNCTION()
	void HandleSupportLinkClicked();

	UFUNCTION()
	void HandleCloseClicked();

protected:
	virtual void NativeOnInitialized() override;

	// Модальный барьер: касание мимо кнопок гасится здесь и в мир не проходит.
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;

private:
	// Строит кодовое дерево-запаску. Имена кубиков совпадают с именами полей — по ним же
	// привяжется дерево из дизайнера.
	void BuildCodeTree();

	// Кнопка окна с подписью, обёрнутая в SizeBox тач-габарита. У обеих кнопок габарит один
	// и тот же (условие задания), а нижняя граница высоты — 48 точек под палец (требование Б8).
	UButton* MakeWindowButton(UVerticalBox* Column, const FText& Label, const FName& BaseName);

	// Скрыть/показать пункт ЦЕЛИКОМ: в кодовом дереве кнопка обёрнута в SizeBox — прятать надо
	// обёртку, иначе в колонке останется пустое место (урок AmmoRow).
	static void SetRowVisibility(UWidget* Widget, ESlateVisibility InVisibility);

	// ⛔ «Ролик не готов — окно остаётся С ОДНОЙ КНОПКОЙ» (условие задания). В окне из
	// дизайнера элементы лежат в холсте и сами не смыкаются: на месте убранной кнопки просмотра
	// осталась бы дыра. Поэтому вторую кнопку подтягиваем на освободившееся место и возвращаем
	// назад, когда ролик снова готов. В кодовом дереве-запаске делать нечего — там вертикальный
	// ящик смыкается сам, и метод молча выходит.
	void ApplySingleButtonLayout(bool bWatchAdVisible);

	// Сводит видимость подсказки про следующий ролик к правилу ShouldShowNextAdHint. Зовётся
	// из всех точек, где меняются входы правила: ShowThanks, SetAdAvailable, ResetNextAdHint —
	// поэтому порядок вызовов владельца (сначала спасибо или сначала готовность) не важен.
	void RefreshNextAdHint();

	FSupportAuthorStyle CachedStyle;

	// Родное место второй кнопки, снятое ОДИН раз при первом сдвиге: возврат обязан положить
	// её ровно туда, куда её поставил владелец, а не туда, где она оказалась после сдвига.
	FVector2D SupportLinkHomePosition = FVector2D::ZeroVector;
	bool bSupportLinkHomeSaved = false;

	// Ролик готов к показу (ставит владелец).
	bool bAdAvailable = false;

	// Идёт показ ролика — повторный тап игнорируем.
	bool bAdInProgress = false;

	// Ролик закончился (досмотрен или закрыт игроком) при ТЕКУЩЕМ открытии окна — один из двух
	// входов правила подсказки. Сбрасывается владельцем при каждом открытии (ResetNextAdHint).
	bool bAdFinishedThisOpen = false;

	// --- Кубики: из готового ассета по именам ЛИБО из BuildCodeTree (имена совпадают) ---

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> DimBorder;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MessageText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> WatchAdButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WatchAdText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SupportLinkButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SupportLinkText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CloseText;

	// Строка благодарности. Спрятана, пока ролик не досмотрен.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ThanksText;

	// Подсказка про следующий ролик. Спрятана всегда, кроме случая «ролик только что кончился,
	// а следующий ещё не готов» (правило ShouldShowNextAdHint).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NextAdHintText;

	// Рамка и подложка кодовой запаски (в готовом ассете их роль играет своя плашка).
	UPROPERTY()
	TObjectPtr<UBorder> FrameBorder;

	UPROPERTY()
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY()
	TArray<TObjectPtr<USizeBox>> ButtonBoxes;

	// Дерево пришло из готового ассета (детект в NativeOnInitialized, как в остальных окнах).
	bool bDesignerTree = false;

	// ⛔ ЭТО ДЕРЕВО ПОСТРОИЛИ МЫ САМИ — единственное основание что-либо в нём перекрашивать
	// и переставлять. Ставится ВНУТРИ BuildCodeTree, поэтому не зависит от того, успела ли
	// отработать инициализация окна.
	//
	// Почему не хватило bDesignerTree: движок 5.5 зовёт NativeOnInitialized только при живом
	// игровом контексте (UserWidget.cpp:159-163 — нужен PlayerContext), а ApplyStyle владелец
	// зовёт сразу после создания окна. Не отработала инициализация — bDesignerTree ещё false,
	// и окно из дизайнера было бы перекрашено поверх ручной настройки Рината. Признак «мы это
	// построили» безопасен в обе стороны: не построили — не трогаем.
	bool bCodeTreeBuilt = false;
};
