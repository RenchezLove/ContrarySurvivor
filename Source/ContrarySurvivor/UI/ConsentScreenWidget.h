// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ConsentScreenWidget.generated.h"

class UButton;
class UVerticalBox;
class UBorder;
class UTextBlock;
class USizeBox;

/**
 * Стиль и тексты экрана согласия (Б6, задание издателя ADR-059).
 *
 * ⛔ ТЕКСТЫ — ДОСЛОВНО из источника истины `docs/contrary-survivor/soglasie-i-politika.md`,
 * раздел 2. Своими словами не переписывать: издатель проверяет, что названы ВСЕ сборщики
 * данных (рекламная сеть Яндекса, AppMetrica, GameAnalytics).
 *
 * Живёт настройкой проекта (UDataConsentSettings, раздел «Согласие и политика») — экран
 * строится из C++-класса и в Details блюпринта не виден, поэтому правится в настройках
 * проекта и в Config/DefaultGame.ini. Локализация (ADR-050): подписи — FText с дефолтами
 * через NSLOCTEXT (LOCTEXT в значении по умолчанию UHT запрещает — UhtTextProperty.cs:104).
 */
USTRUCT(BlueprintType)
struct FConsentScreenStyle
{
	GENERATED_BODY()

	// Затемнение экрана под панелью.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consent Screen",
		meta = (DisplayName = "Затемнение фона", DisplayPriority = "20"))
	FLinearColor DimColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.8f);

	// Золотой кант панели и тёмный фон панели (палитра модалок HUD, как у меню паузы).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consent Screen",
		meta = (DisplayName = "Цвет канта панели", DisplayPriority = "21"))
	FLinearColor FrameColor = FLinearColor(0.8f, 0.65f, 0.25f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consent Screen",
		meta = (DisplayName = "Цвет фона панели", DisplayPriority = "22"))
	FLinearColor PanelColor = FLinearColor(0.06f, 0.07f, 0.09f, 0.98f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consent Screen",
		meta = (DisplayName = "Заголовок", DisplayPriority = "1"))
	FText TitleText = NSLOCTEXT("ConsentScreenWidget", "TitleText", "Пара слов перед началом");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consent Screen",
		meta = (DisplayName = "Первый абзац", DisplayPriority = "2"))
	FText BodyText1 = NSLOCTEXT("ConsentScreenWidget", "BodyText1",
		"Игра бесплатная и живёт за счёт рекламы. Чтобы реклама работала, а я понимал, где игроку тяжело, игра передаёт обезличенные сведения: модель телефона, версию системы, язык, страну, рекламный идентификатор устройства и игровые события — например, начало игры, смерть, покупку в магазине, просмотр рекламного ролика.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consent Screen",
		meta = (DisplayName = "Второй абзац (кто собирает данные)", DisplayPriority = "3"))
	FText BodyText2 = NSLOCTEXT("ConsentScreenWidget", "BodyText2",
		"Этим занимаются рекламная сеть Яндекса, AppMetrica и GameAnalytics. Имя, телефон, почта, контакты и точное местоположение НЕ собираются никогда.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consent Screen",
		meta = (DisplayName = "Третий абзац (что будет при отказе)", DisplayPriority = "4"))
	FText BodyText3 = NSLOCTEXT("ConsentScreenWidget", "BodyText3",
		"Если не согласиться, играть можно точно так же: статистика отключится, а реклама станет неперсональной.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consent Screen",
		meta = (DisplayName = "Кнопка согласия", DisplayPriority = "5"))
	FText AcceptText = NSLOCTEXT("ConsentScreenWidget", "AcceptText", "Принимаю");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consent Screen",
		meta = (DisplayName = "Кнопка отказа", DisplayPriority = "6"))
	FText DeclineText = NSLOCTEXT("ConsentScreenWidget", "DeclineText", "Не сейчас");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consent Screen",
		meta = (DisplayName = "Ссылка на политику", DisplayPriority = "7"))
	FText PolicyLinkText = NSLOCTEXT("ConsentScreenWidget", "PolicyLinkText", "Политика конфиденциальности");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consent Screen",
		meta = (DisplayName = "Размер шрифта заголовка", ClampMin = "8", DisplayPriority = "10"))
	int32 TitleFontSize = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consent Screen",
		meta = (DisplayName = "Размер шрифта текста", ClampMin = "8", DisplayPriority = "11"))
	int32 BodyFontSize = 15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consent Screen",
		meta = (DisplayName = "Размер шрифта кнопок", ClampMin = "8", DisplayPriority = "12"))
	int32 ButtonFontSize = 19;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consent Screen",
		meta = (DisplayName = "Размер шрифта ссылки", ClampMin = "8", DisplayPriority = "13"))
	int32 LinkFontSize = 13;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consent Screen",
		meta = (DisplayName = "Цвет заголовка", DisplayPriority = "23"))
	FLinearColor TitleColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consent Screen",
		meta = (DisplayName = "Цвет текста", DisplayPriority = "24"))
	FLinearColor BodyColor = FLinearColor(0.88f, 0.88f, 0.88f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consent Screen",
		meta = (DisplayName = "Цвет ссылки", DisplayPriority = "25"))
	FLinearColor LinkColor = FLinearColor(0.55f, 0.75f, 1.0f, 1.0f);

	// Цвет подписей кнопок (тёмный — на светлой штатной кнопке UButton).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consent Screen",
		meta = (DisplayName = "Цвет подписей кнопок", DisplayPriority = "26"))
	FLinearColor ButtonTextColor = FLinearColor(0.05f, 0.05f, 0.05f, 1.0f);

	// Габарит кнопки под палец (SizeBox: у UButton 5.5 нет SetPadding).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consent Screen",
		meta = (DisplayName = "Размер кнопки", DisplayPriority = "27"))
	FVector2D ButtonSize = FVector2D(300.0f, 58.0f);

	// Ширина колонки текста. На телефоне в альбомной ориентации панель шире экрана быть не должна.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consent Screen",
		meta = (DisplayName = "Ширина колонки текста", ClampMin = "200", DisplayPriority = "28"))
	float TextColumnWidth = 900.0f;
};

/**
 * Экран согласия на обработку данных (Б6, задание издателя ADR-059; условие издателя по
 * РИ-30 — «согласие игрока НЕ ставить за игрока»).
 *
 * Показывается один раз за установку игры, ДО первого показа рекламы и ДО отправки первого
 * события статистики. Показом и последствиями выбора управляет UDataConsentSubsystem —
 * виджет ТОЛЬКО рисует и сообщает о нажатии (паттерн UPauseMenuWidget/UStartScreenWidget).
 * Дерево целиком строится в C++ (WidgetTree), без BP-наследника.
 */
UCLASS()
class CONTRARYSURVIVOR_API UConsentScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// «Принимаю» — владелец сохраняет согласие, включает статистику и передаёт согласие рекламе.
	FSimpleMulticastDelegate OnAccepted;

	// «Не сейчас» — владелец сохраняет отказ; игра остаётся полностью проходимой.
	FSimpleMulticastDelegate OnDeclined;

	// Нажата ссылка на политику. Адрес живёт в настройке, открывает его владелец.
	FSimpleMulticastDelegate OnPolicyRequested;

	// Применяет стиль и тексты к уже построенному дереву (NativeOnInitialized отработал в
	// CreateWidget с дефолтами). Зовёт UDataConsentSubsystem сразу после создания виджета.
	void ApplyStyle(const FConsentScreenStyle& Style);

protected:
	virtual void NativeOnInitialized() override;

	// Модальный барьер: клик/тап мимо кнопок гасится здесь и в мир не проходит.
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;

	UFUNCTION()
	void HandleAcceptClicked();

	UFUNCTION()
	void HandleDeclineClicked();

	UFUNCTION()
	void HandlePolicyClicked();

private:
	// Кнопка с подписью, обёрнутая в SizeBox тач-размера, добавленная в колонку.
	UButton* MakeMenuButton(UVerticalBox* Column, const FText& Label, const FName& BaseName);

	// Абзац основного текста с переносом слов.
	UTextBlock* MakeParagraph(UVerticalBox* Column, const FText& Text, const FName& Name);

	UPROPERTY()
	TObjectPtr<UBorder> DimmerBorder;

	UPROPERTY()
	TObjectPtr<UBorder> FrameBorder;

	UPROPERTY()
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY()
	TObjectPtr<USizeBox> ColumnBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleBlock;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> BodyBlocks;

	UPROPERTY()
	TObjectPtr<UTextBlock> AcceptLabel;

	UPROPERTY()
	TObjectPtr<UTextBlock> DeclineLabel;

	UPROPERTY()
	TObjectPtr<UTextBlock> PolicyLabel;

	UPROPERTY()
	TObjectPtr<UButton> PolicyButton;

	UPROPERTY()
	TArray<TObjectPtr<USizeBox>> ButtonBoxes;
};
