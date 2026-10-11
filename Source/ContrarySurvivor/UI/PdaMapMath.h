// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "Layout/Margin.h"

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

	// Размер окна-смартфона на экране (слова Рината 10.10.2026: «сам виджет КПК и саму карту
	// сделай побольше… подложка виджета должна быть похожа на смартфон»): высота — доля высоты
	// экрана, ширина — по соотношению сторон подложки; если такой смартфон шире экрана с
	// полями, он уменьшается целиком, сохраняя соотношение. Вырожденные входы — весь экран.
	// Размер картинки в точках — тот, с которым её завели в проект. Спрашивать у картинки
	// «размер сейчас» нельзя: в редакторе в первый кадр после загрузки она ещё готовится и
	// отвечает размером заглушки 32 на 32 (11.10.2026: из-за этого смартфон вставал квадратом).
	// У картинки, созданной кодом, исходного размера нет — берётся нынешний.
	inline FIntPoint GetPictureSize(const UTexture2D* Texture)
	{
		if (!Texture)
		{
			return FIntPoint::ZeroValue;
		}
		const FIntPoint Imported = Texture->GetImportedSize();
		return (Imported.X > 0 && Imported.Y > 0) ? Imported : FIntPoint(Texture->GetSizeX(), Texture->GetSizeY());
	}

	inline FVector2D ComputePhoneSize(const FVector2D& Screen, double HeightFraction, double Aspect, double SideMargin)
	{
		if (Screen.X <= KINDA_SMALL_NUMBER || Screen.Y <= KINDA_SMALL_NUMBER || Aspect <= KINDA_SMALL_NUMBER)
		{
			return Screen;
		}
		double Height = Screen.Y * FMath::Clamp(HeightFraction, 0.3, 1.0);
		double Width = Height * Aspect;
		const double MaxWidth = FMath::Max(1.0, Screen.X - 2.0 * FMath::Max(0.0, SideMargin));
		if (Width > MaxWidth)
		{
			Width = MaxWidth;
			Height = Width / Aspect;
		}
		return FVector2D(Width, Height);
	}

	// Отступы экрана смартфона внутри подложки: доли картинки -> единицы раскладки.
	inline FMargin ComputePhoneScreenPadding(const FVector2D& PhoneSize, const FMargin& InsetFractions)
	{
		auto Clamped = [](float Value) { return FMath::Clamp(Value, 0.0f, 0.45f); };
		return FMargin(
			Clamped(InsetFractions.Left) * PhoneSize.X, Clamped(InsetFractions.Top) * PhoneSize.Y,
			Clamped(InsetFractions.Right) * PhoneSize.X, Clamped(InsetFractions.Bottom) * PhoneSize.Y);
	}

	/**
	 * Вид карты в поле: приближение и сдвиг (слова Рината 10.10.2026: «дай игроку возможность
	 * приближать и отдалять её как кнопками (+ и −), так и пальцами (раздвигая)»).
	 *
	 * Поле (Viewport) — прямоугольник окна, в котором видна карта. Карта при приближении 1
	 * вписана в поле целиком (BaseSize, пропорции мира сохранены); при приближении Z её размер
	 * BaseSize * Z. Положение карты задаётся точкой карты (в долях), которая стоит в середине
	 * поля. Карта за края поля не уезжает: по оси, где она больше поля, её края не заходят
	 * внутрь поля; по оси, где она меньше, она стоит по центру.
	 *
	 * Отметки, подписи, чернота и стрелка героя привязаны к карте долями, поэтому двигаются и
	 * расходятся вместе с ней, а их собственный размер на экране не меняется.
	 */
	struct FMapView
	{
		double Zoom = 1.0;
		FVector2D CenterUV = FVector2D(0.5, 0.5);

		static double ClampZoom(double InZoom, double MaxZoom)
		{
			return FMath::Clamp(InZoom, 1.0, FMath::Max(1.0, MaxZoom));
		}

		FVector2D GetContentSize(const FVector2D& BaseSize) const
		{
			return BaseSize * Zoom;
		}

		// Левый-верхний угол карты в координатах поля (с учётом правила «не уезжать за края»).
		FVector2D GetContentOffset(const FVector2D& Viewport, const FVector2D& BaseSize) const
		{
			const FVector2D Content = GetContentSize(BaseSize);
			auto Axis = [](double View, double Size, double Center)
			{
				if (Size <= View)
				{
					return (View - Size) * 0.5;
				}
				return FMath::Clamp(View * 0.5 - Center * Size, View - Size, 0.0);
			};
			return FVector2D(Axis(Viewport.X, Content.X, CenterUV.X), Axis(Viewport.Y, Content.Y, CenterUV.Y));
		}

		// Точка карты (доли) -> точка поля, и обратно.
		FVector2D MapToViewport(const FVector2D& UV, const FVector2D& Viewport, const FVector2D& BaseSize) const
		{
			return GetContentOffset(Viewport, BaseSize) + UV * GetContentSize(BaseSize);
		}

		FVector2D ViewportToMap(const FVector2D& Point, const FVector2D& Viewport, const FVector2D& BaseSize) const
		{
			const FVector2D Content = GetContentSize(BaseSize);
			const FVector2D Offset = GetContentOffset(Viewport, BaseSize);
			return FVector2D(
				Content.X > KINDA_SMALL_NUMBER ? (Point.X - Offset.X) / Content.X : 0.5,
				Content.Y > KINDA_SMALL_NUMBER ? (Point.Y - Offset.Y) / Content.Y : 0.5);
		}

		// Поставить карту так, чтобы её левый-верхний угол был в Offset (и прижать по правилу).
		void SetContentOffset(const FVector2D& Offset, const FVector2D& Viewport, const FVector2D& BaseSize)
		{
			const FVector2D Content = GetContentSize(BaseSize);
			CenterUV = FVector2D(
				Content.X > KINDA_SMALL_NUMBER ? (Viewport.X * 0.5 - Offset.X) / Content.X : 0.5,
				Content.Y > KINDA_SMALL_NUMBER ? (Viewport.Y * 0.5 - Offset.Y) / Content.Y : 0.5);
			Normalize(Viewport, BaseSize);
		}

		// Привести середину к тому, что реально показано после прижатия к краям: иначе сдвиг
		// «в упор» копился бы и карта отлипала от края не сразу.
		void Normalize(const FVector2D& Viewport, const FVector2D& BaseSize)
		{
			const FVector2D Content = GetContentSize(BaseSize);
			const FVector2D Offset = GetContentOffset(Viewport, BaseSize);
			CenterUV = FVector2D(
				Content.X > KINDA_SMALL_NUMBER ? FMath::Clamp((Viewport.X * 0.5 - Offset.X) / Content.X, 0.0, 1.0) : 0.5,
				Content.Y > KINDA_SMALL_NUMBER ? FMath::Clamp((Viewport.Y * 0.5 - Offset.Y) / Content.Y, 0.0, 1.0) : 0.5);
		}

		// Приблизить или отдалить так, чтобы точка карты под Point (палец, курсор) осталась под ним.
		void ZoomAt(const FVector2D& Point, double NewZoom, double MaxZoom, const FVector2D& Viewport, const FVector2D& BaseSize)
		{
			const FVector2D Anchor = ViewportToMap(Point, Viewport, BaseSize);
			Zoom = ClampZoom(NewZoom, MaxZoom);
			SetContentOffset(Point - Anchor * GetContentSize(BaseSize), Viewport, BaseSize);
		}

		// Сдвинуть карту на Delta единиц поля (палец или мышь с зажатой кнопкой).
		void PanBy(const FVector2D& Delta, const FVector2D& Viewport, const FVector2D& BaseSize)
		{
			SetContentOffset(GetContentOffset(Viewport, BaseSize) + Delta, Viewport, BaseSize);
		}

		// Поставить точку карты в середину поля (кнопка «найти героя»).
		void CenterOn(const FVector2D& UV, const FVector2D& Viewport, const FVector2D& BaseSize)
		{
			CenterUV = UV;
			Normalize(Viewport, BaseSize);
		}

		// Щипок двумя пальцами: были в OldA и OldB, стали в NewA и NewB. Карта растягивается во
		// столько раз, во сколько разошлись пальцы, и едет за серединой между ними.
		void Pinch(const FVector2D& OldA, const FVector2D& OldB, const FVector2D& NewA, const FVector2D& NewB,
			double MaxZoom, const FVector2D& Viewport, const FVector2D& BaseSize)
		{
			const double OldDistance = FVector2D::Distance(OldA, OldB);
			const double NewDistance = FVector2D::Distance(NewA, NewB);
			const FVector2D OldMiddle = (OldA + OldB) * 0.5;
			const FVector2D NewMiddle = (NewA + NewB) * 0.5;
			const FVector2D Anchor = ViewportToMap(OldMiddle, Viewport, BaseSize);
			if (OldDistance > 1.0 && NewDistance > 1.0)
			{
				Zoom = ClampZoom(Zoom * NewDistance / OldDistance, MaxZoom);
			}
			SetContentOffset(NewMiddle - Anchor * GetContentSize(BaseSize), Viewport, BaseSize);
		}
	};
}
