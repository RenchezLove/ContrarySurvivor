// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ContrarySurvivor/UI/SelfHidingWidget.h"
#include "EndOfStoryWidget.generated.h"

class UTextBlock;
class UButton;
class UBorder;
class USizeBox;

/**
 * Настройки связи с автором для карточки конца сюжета (поправка Рината к замечанию издателя:
 * кнопку «Написать мне» ОСТАВЛЯЕМ).
 *
 * Адрес канала живёт в КОНФИГЕ, а не на объекте HUD и не внутри ассета: строка читается из
 * Config/DefaultGame.ini, раздел [/Script/ContrarySurvivor.EndOfStorySettings]. Поэтому
 * вписать адрес, когда Ринат его даст, можно текстовым редактором — ни пересобирать код, ни
 * пересохранять и заново готовить контент не требуется. Пока строка пуста, кнопка ведёт себя
 * как раньше и показывает «Канал скоро появится».
 *
 * Тот же приём уже применён в проекте к идентификаторам рекламы (UCLASS(Config = ...) плюс
 * UPROPERTY(Config)) — значения, которые приходят позже, живут в конфиге.
 */
UCLASS(Config = Game, DefaultConfig)
class CONTRARYSURVIVOR_API UEndOfStorySettings : public UObject
{
	GENERATED_BODY()

public:
	// Адрес канала для кнопки «Написать мне». Пусто — кнопка показывает «Канал скоро появится».
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "EndOfStory",
		meta = (DisplayName = "Адрес канала для кнопки «Написать мне»"))
	FString ChannelUrl;

	// Готовый адрес из конфига (без лишних пробелов) либо пустая строка.
	static FString GetChannelUrl();
};

/**
 * Стиль плашки сообщения о конце сюжета. Живёт EditAnywhere-полем на AContrarySurvivorHUD
 * (виджет строится из C++-класса и в Details не виден — настройка на владельце, паттерн
 * FLimpIndicatorStyle). Плашка КОМПАКТНАЯ (решение Рината: не закрывать экран): фикс-ширина,
 * высота растёт за текстом, позиция — вверху по центру, ниже строки задачи интро.
 */
USTRUCT(BlueprintType)
struct FEndOfStoryStyle
{
	GENERATED_BODY()

	// Цвет плашки-подложки (тёмная полупрозрачная, как тосты онбординга).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EndOfStory", meta = (DisplayName = "Цвет подложки"))
	FLinearColor PlateColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.8f);

	// Цвет основного текста сообщения.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EndOfStory", meta = (DisplayName = "Цвет текста"))
	FLinearColor TextColor = FLinearColor(0.95f, 0.95f, 0.95f, 1.0f);

	// Цвет строки-статуса «Канал скоро появится».
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EndOfStory", meta = (DisplayName = "Цвет строки-статуса"))
	FLinearColor StatusColor = FLinearColor(1.0f, 0.85f, 0.3f, 1.0f);

	// Цвет фона кнопок (поверх стандартного скина кнопки UMG).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EndOfStory", meta = (DisplayName = "Цвет кнопок"))
	FLinearColor ButtonColor = FLinearColor(0.25f, 0.28f, 0.33f, 1.0f);

	// Цвет подписи на кнопках.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EndOfStory", meta = (DisplayName = "Цвет подписи кнопок"))
	FLinearColor ButtonTextColor = FLinearColor(0.95f, 0.96f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EndOfStory", meta = (ClampMin = "8", DisplayName = "Кегль текста"))
	int32 FontSize = 15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EndOfStory", meta = (ClampMin = "8", DisplayName = "Кегль кнопок"))
	int32 ButtonFontSize = 14;

	// Внутренние отступы плашки: X — по горизонтали, Y — по вертикали.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EndOfStory", meta = (DisplayName = "Отступы плашки"))
	FVector2D PlatePadding = FVector2D(16.0f, 12.0f);

	// Якорь на экране в долях (0.5/0 = верх-центр) и смещение от якоря в пикселях.
	// Дефолт — вверху по центру, ниже строки задачи интро/трекера квеста.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EndOfStory", meta = (DisplayName = "Якорь на экране (доли)"))
	FVector2D ScreenAnchor = FVector2D(0.5f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EndOfStory", meta = (DisplayName = "Смещение от якоря (px)"))
	FVector2D ScreenOffset = FVector2D(0.0f, 110.0f);

	// Ширина плашки (px). Высота НЕ задаётся: растёт за текстом (авто-перенос).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EndOfStory", meta = (ClampMin = "200", DisplayName = "Ширина плашки (px)"))
	float BoxWidth = 620.0f;
};

/**
 * Гейт «запланировать показ один раз» — чистая логика без UObject, вынесена под headless-тест
 * (паттерн FLimpFirstHintState/DailyRewardLogic: сам показ виджета тестируется только в PIE,
 * а решение «пора ли планировать» — тестируемо без мира). Живёт членом AContrarySurvivorHUD.
 */
struct FEndOfStoryGate
{
	// Показ уже запланирован в этой сессии (двойной таймер не заводим).
	bool bScheduled = false;

	// true РОВНО один раз: сценка о шкурах уже проиграна профилю (флаг сейва
	// bElderNotebookHintShown), а сообщение конца сюжета ещё не показывалось
	// (флаг сейва bEndOfStoryShown).
	bool ShouldSchedule(bool bEpilogueSeen, bool bAlreadyShownInSave)
	{
		if (bScheduled || !bEpilogueSeen || bAlreadyShownInSave)
		{
			return false;
		}
		bScheduled = true;
		return true;
	}
};

/**
 * Сообщение о конце сюжета текущей версии (Build 1.2, задача Рината 07-31): компактная плашка
 * через ~30 с после того, как староста закончил сценку про шкуры волков. Игра НЕ на паузе,
 * показ ОДИН раз за сохранение (флаг в сейве ставит AContrarySurvivorHUD при показе).
 * Кнопки: [Написать мне] — открывает ссылку на канал, а пока ссылки нет, показывает строку
 * «Канал скоро появится»; [Играть дальше] — закрывает плашку.
 *
 * Build 1.2.1 (ТЗ Д2) — два пути, как у UTouchControlsWidget:
 *  - создан из WBP_EndOfStory (родитель этот класс) → дерево Рината из дизайнера, кубики
 *    приходят по BindWidgetOptional-именам (Plate/MessageText/StatusText/WriteButton/
 *    PlayButton/WriteButtonText/PlayButtonText), код их НЕ перекрашивает (стиль целиком
 *    в ассете); ассет генерирует GenerateWbpCommandlet (канвас-первая, кнопки с ручками);
 *  - ассета нет / слот HUD пуст → прежний кодовый вид: дерево строится в C++ (WidgetTree),
 *    стиль — EditAnywhere-поля AContrarySurvivorHUD (FEndOfStoryStyle).
 * Тексты в ОБОИХ путях ставит InitContent (дословный текст Рината живёт на HUD).
 */
UCLASS()
class CONTRARYSURVIVOR_API UEndOfStoryWidget : public USelfHidingWidget
{
	GENERATED_BODY()

public:
	// Тексты и ссылка — готовые переводимые FText (ADR-050), владеет ими HUD.
	// InChannelUrl пуст — кнопка [Написать мне] показывает InChannelPendingText.
	void InitContent(const FText& InMessage, const FText& InWriteButtonLabel,
		const FText& InPlayButtonLabel, const FText& InChannelPendingText,
		const FString& InChannelUrl);

	// Применяет стиль к КОДОВОМУ дереву (зовёт HUD сразу после создания). Для дерева из
	// WBP — no-op: стиль и раскладка целиком принадлежат ассету Рината (ТЗ Д2).
	void ApplyStyle(const FEndOfStoryStyle& Style);

protected:
	virtual void NativeOnInitialized() override;

	// Пока открыт модальный экран (инвентарь/магазин/диалог/смерть/пауза) — плашка прячется
	// (SelfHiding-паттерн), после закрытия возвращается: сообщение одно на профиль, терять нельзя.
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION() void HandleWriteClicked();
	UFUNCTION() void HandlePlayClicked();

private:
	// Закрыть плашку и вернуть игровой режим ввода (если игрок не успел открыть модалку).
	void CloseAndRestoreInput();

	// Строит прежнее кодовое дерево (путь «ассета нет»). Имена кубиков = именам полей —
	// те же, что генерирует коммандлет в WBP_EndOfStory (BindWidgetOptional биндит по имени).
	void BuildCodeTree();

	// --- Кубики: из WBP по BindWidgetOptional ЛИБО из BuildCodeTree (имена совпадают) ---

	// Фиксирует ширину плашки (только кодовое дерево; в WBP её нет — размер у канвас-слотов).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> WidthBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Plate;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MessageText;

	// Строка «Канал скоро появится» — скрыта, показывается по [Написать мне] при пустой ссылке.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> WriteButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> PlayButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WriteButtonText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayButtonText;

	// Дерево пришло из WBP-ассета (детект в NativeOnInitialized, как TouchControlsWidget.cpp).
	bool bDesignerTree = false;

	// Текст-статус и ссылка (латчатся в InitContent).
	FText ChannelPendingText;
	FString ChannelUrl;
};
