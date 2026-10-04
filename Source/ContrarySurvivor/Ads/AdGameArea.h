// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UGameInstance;

/**
 * Место под рекламный баннер (решение Рината 04.10.2026, дословно): «Банер как бы двигает
 * (уменьшает) весь экран игры. Т.е. экран игры ужимается и на появившееся место встает
 * банер... После завершения показа банера - игра снова начинает занимать весь экран».
 *
 * Как это сделано (сверено по исходникам UE 5.5):
 *  - область вывода игрока — та же, что у разделённого экрана: доли экрана лежат в
 *    UGameViewportClient::SplitscreenInfo, LayoutPlayers() переносит их в ULocalPlayer::
 *    Size/Origin (GameViewportClient.cpp:2549-2574). По ним считается прямоугольник трёхмерной
 *    картинки (LocalPlayer.cpp:1163-1170), а всё вне его движок заливает чёрным
 *    (GameViewportClient.cpp:1783-1790). Поверхность Android при этом не пересоздаётся;
 *  - окна интерфейса, добавленные «на экран игрока» (UUserWidget::AddToPlayerScreen),
 *    раскладываются в том же прямоугольнике (SGameLayerManager.cpp:630-646) — вместе с
 *    зонами нажатий, потому что это обычная раскладка Slate. Поэтому все окна проекта
 *    добавляются через ContraryUI::AddToGameScreen (UI/GameScreenLayer.h);
 *  - рисунок HUD на холсте получает холст размером с тот же прямоугольник
 *    (GameViewportClient.cpp:1922-1937);
 *  - чтобы окна не обрезались, а именно УМЕНЬШИЛИСЬ, масштаб интерфейса умножается на ту же
 *    долю через UUserInterfaceSettings::ApplicationScale (UserInterfaceSettings.cpp:109).
 *
 * Игра сжимается равномерно (ширина и высота на одну и ту же долю) и прижимается к верху
 * по центру: раскладка окон остаётся прежней, просто мельче; снизу — полоса высотой ровно
 * с баннер, по бокам — узкие чёрные поля.
 */
namespace AdGameArea
{
	// Область игры в долях экрана.
	struct FArea
	{
		FVector2D Origin = FVector2D::ZeroVector;
		FVector2D Size = FVector2D(1.0, 1.0);

		bool IsFullScreen() const { return Size.X >= 1.0 && Size.Y >= 1.0; }
	};

	// Больше этой доли высоты экрана под баннер не отдаём, что бы ни сообщил SDK.
	inline constexpr float MaxBannerFraction = 0.25f;

	// Чистое правило: область игры при баннере высотой BannerHeightFraction (доля высоты
	// экрана). Ноль и меньше — весь экран. Низ области = 1 − высота баннера ровно.
	CONTRARYSURVIVOR_API FArea Compute(float BannerHeightFraction);

	/**
	 * Память о баннере у рекламной службы: какая точка заказала баннер и сколько места он
	 * сейчас занимает. Место отдаётся ТОЛЬКО загруженному баннеру: заказан, но не загрузился
	 * (или не заказан вовсе — например, нет согласия) — игра на весь экран.
	 */
	struct CONTRARYSURVIVOR_API FBannerState
	{
		// Баннер точки заказан (ещё не загружен): места не занимает.
		void OnRequested(FName Placement);

		// Баннер загрузился и сообщил высоту (доля высоты экрана). Событие от баннера,
		// которого никто не заказывал (успели убрать), игнорируется. true — высота принята.
		bool OnLoaded(float HeightFraction);

		// Баннер не загрузился: остаётся заказанным, места не занимает.
		void OnLoadFailed();

		// Баннер точки убран. Стоит баннер другой точки — ничего не меняется (false).
		bool OnHidden(FName Placement);

		// Убрать любой баннер (остановка службы).
		void Reset();

		FName GetPlacement() const { return Placement; }
		bool IsRequested() const { return !Placement.IsNone(); }
		bool IsOnScreen() const { return HeightFraction > 0.0f; }
		float GetHeightFraction() const { return HeightFraction; }
		FArea GetGameArea() const { return Compute(HeightFraction); }

	private:
		FName Placement;
		float HeightFraction = 0.0f;
	};

	// Применить область к живой игре: область вывода игрока и масштаб интерфейса. Ноль —
	// вернуть весь экран. Повторный вызов с тем же числом ничего не делает.
	CONTRARYSURVIVOR_API void Apply(UGameInstance* GameInstance, float BannerHeightFraction);
}
