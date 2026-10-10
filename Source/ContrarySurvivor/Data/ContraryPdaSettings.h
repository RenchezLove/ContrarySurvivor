// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"
#include "ContrarySurvivor/Components/QuestComponent.h" // EQuestKind
#include "ContraryPdaSettings.generated.h"

class UTexture2D;

/**
 * Настройки карманного компьютера (КПК) и карты (слова Рината 10.10.2026: «аналог PDA из
 * Stalker… карта, сведения о квестах… основные и побочные квесты обозначаются оранжевыми и
 * синими метками и текстом»). Страница в Project Settings → Game → «Contrary Survivor: КПК и
 * карта»; значения пишутся в Config/DefaultGame.ini — меняются без кода и без правки окон.
 *
 * Цвета вида задания действуют везде разом: строка задания на экране, метка-указатель в мире,
 * список и карта в окне КПК.
 *
 * КАРТА. Картинка местности снимается одним кадром строго сверху; прямоугольник мира, который
 * она покрывает, задаётся двумя углами. Игровая камера смотрит вдоль +Y мира (поворот штанги
 * камеры 90°, APlayerCharacter::CameraBoomRotation), поэтому на экране «вверх» = +Y мира,
 * «вправо» = −X мира. Чтобы верх карты совпадал с верхом экрана в игре, левый-верхний угол
 * карты — это НАИБОЛЬШИЕ X и Y прямоугольника, правый-нижний — наименьшие.
 *
 * Границы по умолчанию — игровая зона уровня L_World_C целиком, до внешнего края тумана
 * (актор BP_WorldBorder: центр (5500, −4370), сторона 46875 см; снято с уровня 10.10.2026).
 * Невидимая стена стоит на 1400 см внутри, так что герой за край карты не выходит.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Contrary Survivor: КПК и карта"))
class CONTRARYSURVIVOR_API UContraryPdaSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return FName(TEXT("Game")); }

	// Мягкий оранжевый: читается на тёмной панели и поверх игрового мира.
	UPROPERTY(config, EditAnywhere, Category = "Задания", meta = (
		DisplayName = "Цвет основных заданий"))
	FLinearColor MainQuestColor = FLinearColor(1.0f, 0.55f, 0.15f, 1.0f);

	// Мягкий синий (светлый, чтобы не тонул на тёмном фоне).
	UPROPERTY(config, EditAnywhere, Category = "Задания", meta = (
		DisplayName = "Цвет побочных заданий"))
	FLinearColor SideQuestColor = FLinearColor(0.35f, 0.65f, 1.0f, 1.0f);

	// Пусто — карта рисуется тёмным полем с сеткой и всеми отметками.
	UPROPERTY(config, EditAnywhere, Category = "Карта", meta = (
		DisplayName = "Картинка карты (снимок местности сверху)"))
	TSoftObjectPtr<UTexture2D> MapTexture;

	UPROPERTY(config, EditAnywhere, Category = "Карта", meta = (
		DisplayName = "Левый-верхний угол карты в мире (X, Y)",
		ToolTip = "Точка мира, которая попадает в левый-верхний угол картинки. При игровой камере это наибольшие X и Y прямоугольника."))
	FVector2D MapWorldTopLeft = FVector2D(28937.5, 19067.5);

	UPROPERTY(config, EditAnywhere, Category = "Карта", meta = (
		DisplayName = "Правый-нижний угол карты в мире (X, Y)",
		ToolTip = "Точка мира, которая попадает в правый-нижний угол картинки. При игровой камере это наименьшие X и Y прямоугольника."))
	FVector2D MapWorldBottomRight = FVector2D(-17937.5, -27807.5);

	FLinearColor GetQuestKindColor(EQuestKind Kind) const
	{
		return Kind == EQuestKind::Side ? SideQuestColor : MainQuestColor;
	}

	static const UContraryPdaSettings* Get() { return GetDefault<UContraryPdaSettings>(); }
};
