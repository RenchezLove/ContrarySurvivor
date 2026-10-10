// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * Пересчёты карты КПК — чистые функции без мира и окон (их гоняют автопроверки).
 *
 * Карта — картинка прямоугольника мира, заданного двумя углами (UContraryPdaSettings):
 * TopLeft — точка мира в левом-верхнем углу картинки, BottomRight — в правом-нижнем. Какая ось
 * мира куда смотрит, определяется самими углами: растёт ли X мира вправо или влево по карте —
 * решает знак разности углов. Оси мира и карты при этом параллельны (поворота карты нет).
 */
namespace PdaMap
{
	// Точка мира -> доли карты: (0,0) — левый-верхний угол картинки, (1,1) — правый-нижний.
	// Точка вне прямоугольника прижимается к краю (отметка не уезжает за карту).
	// Вырожденный прямоугольник (нулевая сторона) даёт середину по этой оси.
	inline FVector2D WorldToMapUV(const FVector2D& World, const FVector2D& TopLeft, const FVector2D& BottomRight)
	{
		const FVector2D Span = BottomRight - TopLeft;
		const double U = FMath::IsNearlyZero(Span.X) ? 0.5 : (World.X - TopLeft.X) / Span.X;
		const double V = FMath::IsNearlyZero(Span.Y) ? 0.5 : (World.Y - TopLeft.Y) / Span.Y;
		return FVector2D(FMath::Clamp(U, 0.0, 1.0), FMath::Clamp(V, 0.0, 1.0));
	}

	// Направление в мире -> направление на карте (в долях, без прижатия; для стрелки героя).
	inline FVector2D WorldDirToMapDir(const FVector2D& WorldDir, const FVector2D& TopLeft, const FVector2D& BottomRight)
	{
		const FVector2D Span = BottomRight - TopLeft;
		return FVector2D(
			FMath::IsNearlyZero(Span.X) ? 0.0 : WorldDir.X / Span.X,
			FMath::IsNearlyZero(Span.Y) ? 0.0 : WorldDir.Y / Span.Y);
	}

	// Размер поля карты, вписанного в доступное место с сохранением пропорций прямоугольника
	// мира (карта не растягивается). Вырожденные входы — доступное место как есть.
	inline FVector2D FitMapSize(const FVector2D& Available, const FVector2D& TopLeft, const FVector2D& BottomRight)
	{
		const double WorldW = FMath::Abs(BottomRight.X - TopLeft.X);
		const double WorldH = FMath::Abs(BottomRight.Y - TopLeft.Y);
		if (WorldW <= KINDA_SMALL_NUMBER || WorldH <= KINDA_SMALL_NUMBER
			|| Available.X <= KINDA_SMALL_NUMBER || Available.Y <= KINDA_SMALL_NUMBER)
		{
			return Available;
		}
		const double Scale = FMath::Min(Available.X / WorldW, Available.Y / WorldH);
		return FVector2D(WorldW * Scale, WorldH * Scale);
	}
}
