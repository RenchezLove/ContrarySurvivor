// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * Открытая часть карты КПК (слова Рината 10.10.2026: «изначально на карте ничего не должно быть
 * видно, кроме деревни. Остальное — чернота, неизведанные территории… игрок пошёл от деревни в
 * сторону неизведанных территорий и карта появляется в каком-то радиусе от него»).
 *
 * Сетка клеток поверх прямоугольника карты: клетка либо открыта, либо нет; открытое обратно не
 * закрывается. Всё считается в долях карты (0,0 — левый-верхний угол картинки, 1,1 —
 * правый-нижний, как у PdaMap::WorldToMapUV), поэтому здесь нет ни мира, ни окон — чистые
 * правила, их гоняют автопроверки.
 */
struct FPdaMapFog
{
	static constexpr int32 MinSide = 8;
	static constexpr int32 MaxSide = 512;

	// Сетка заданного размера, всё закрыто.
	void Init(int32 InWidth, int32 InHeight)
	{
		Width = FMath::Clamp(InWidth, MinSide, MaxSide);
		Height = FMath::Clamp(InHeight, MinSide, MaxSide);
		Cells.Init(false, Width * Height);
	}

	bool IsValid() const { return Width > 0 && Height > 0 && Cells.Num() == Width * Height; }
	int32 GetWidth() const { return Width; }
	int32 GetHeight() const { return Height; }

	bool IsCellRevealed(int32 X, int32 Y) const
	{
		return IsValid() && X >= 0 && Y >= 0 && X < Width && Y < Height && Cells[Y * Width + X];
	}

	// Открыта ли клетка, в которую попадает точка карты (точка вне карты прижимается к краю).
	bool IsRevealedAt(const FVector2D& UV) const
	{
		return IsValid() && Cells[CellIndexAt(UV)];
	}

	int32 CountRevealed() const
	{
		return Cells.CountSetBits();
	}

	// Открыть круг (в долях карты радиусы по осям свои: карта может быть не квадратной).
	// Открывается клетка, чья середина попала в круг, и всегда — клетка под самим центром.
	// true — открылось хоть что-то новое.
	bool RevealCircle(const FVector2D& CenterUV, const FVector2D& RadiusUV)
	{
		if (!IsValid())
		{
			return false;
		}
		bool bChanged = false;
		const int32 CenterIndex = CellIndexAt(CenterUV);
		if (!Cells[CenterIndex])
		{
			Cells[CenterIndex] = true;
			bChanged = true;
		}
		const double RadiusX = FMath::Max(0.0, RadiusUV.X) * Width;   // в клетках
		const double RadiusY = FMath::Max(0.0, RadiusUV.Y) * Height;
		if (RadiusX <= KINDA_SMALL_NUMBER || RadiusY <= KINDA_SMALL_NUMBER)
		{
			return bChanged;
		}
		const double CenterX = CenterUV.X * Width;
		const double CenterY = CenterUV.Y * Height;
		const int32 MinX = FMath::Clamp(FMath::FloorToInt32(CenterX - RadiusX), 0, Width - 1);
		const int32 MaxX = FMath::Clamp(FMath::CeilToInt32(CenterX + RadiusX), 0, Width - 1);
		const int32 MinY = FMath::Clamp(FMath::FloorToInt32(CenterY - RadiusY), 0, Height - 1);
		const int32 MaxY = FMath::Clamp(FMath::CeilToInt32(CenterY + RadiusY), 0, Height - 1);
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			const double Dy = (Y + 0.5 - CenterY) / RadiusY;
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				const double Dx = (X + 0.5 - CenterX) / RadiusX;
				if (Dx * Dx + Dy * Dy <= 1.0 && !Cells[Y * Width + X])
				{
					Cells[Y * Width + X] = true;
					bChanged = true;
				}
			}
		}
		return bChanged;
	}

	// true — что-то изменилось.
	bool RevealAll()
	{
		const bool bChanged = IsValid() && CountRevealed() != Cells.Num();
		Cells.Init(true, Width * Height);
		return bChanged;
	}

	bool HideAll()
	{
		const bool bChanged = CountRevealed() != 0;
		Cells.Init(false, Width * Height);
		return bChanged;
	}

	// --- Сохранение: длины серий «закрыто, открыто, закрыто…» по строкам сетки, первая серия —
	// закрытые клетки (может быть нулевой). Число пишется по семь бит на байт, старший бит —
	// «есть продолжение». Сплошные области сжимаются в считаные байты: сетка 128 на 128 с одной
	// деревней занимает десятки байт вместо двух тысяч.
	void Pack(TArray<uint8>& Out) const
	{
		Out.Reset();
		if (!IsValid())
		{
			return;
		}
		bool bRunValue = false;
		uint32 RunLength = 0;
		auto Flush = [&Out](uint32 Length)
		{
			do
			{
				uint8 Byte = static_cast<uint8>(Length & 0x7F);
				Length >>= 7;
				if (Length != 0)
				{
					Byte |= 0x80;
				}
				Out.Add(Byte);
			} while (Length != 0);
		};
		for (int32 Index = 0; Index < Cells.Num(); ++Index)
		{
			if (Cells[Index] != bRunValue)
			{
				Flush(RunLength);
				bRunValue = !bRunValue;
				RunLength = 0;
			}
			++RunLength;
		}
		Flush(RunLength);
	}

	// Прочитать сохранённое. false — запись не подходит (чужой размер, обрыв, лишнее): сетка
	// при этом остаётся прежней.
	bool Unpack(int32 InWidth, int32 InHeight, const TArray<uint8>& In)
	{
		if (InWidth < MinSide || InHeight < MinSide || InWidth > MaxSide || InHeight > MaxSide)
		{
			return false;
		}
		const int32 Total = InWidth * InHeight;
		TBitArray<> Read;
		Read.Init(false, Total);
		int32 Filled = 0;
		bool bRunValue = false;
		int32 Pos = 0;
		while (Pos < In.Num())
		{
			uint32 Length = 0;
			int32 Shift = 0;
			bool bComplete = false;
			while (Pos < In.Num() && Shift <= 28)
			{
				const uint8 Byte = In[Pos++];
				Length |= static_cast<uint32>(Byte & 0x7F) << Shift;
				Shift += 7;
				if ((Byte & 0x80) == 0)
				{
					bComplete = true;
					break;
				}
			}
			if (!bComplete || Length > static_cast<uint32>(Total - Filled))
			{
				return false;
			}
			if (bRunValue && Length > 0)
			{
				Read.SetRange(Filled, static_cast<int32>(Length), true);
			}
			Filled += static_cast<int32>(Length);
			bRunValue = !bRunValue;
		}
		if (Filled != Total)
		{
			return false;
		}
		Width = InWidth;
		Height = InHeight;
		Cells = MoveTemp(Read);
		return true;
	}

	// Перенести открытое на сетку другого размера (размер сетки сменили в настройках, а
	// сохранение записано со старым): новая клетка открыта, если открыта старая под её серединой.
	void ResampleTo(int32 NewWidth, int32 NewHeight)
	{
		FPdaMapFog Resized;
		Resized.Init(NewWidth, NewHeight);
		if (IsValid())
		{
			for (int32 Y = 0; Y < Resized.Height; ++Y)
			{
				for (int32 X = 0; X < Resized.Width; ++X)
				{
					const FVector2D UV((X + 0.5) / Resized.Width, (Y + 0.5) / Resized.Height);
					Resized.Cells[Y * Resized.Width + X] = IsRevealedAt(UV);
				}
			}
		}
		*this = MoveTemp(Resized);
	}

	// Маска черноты для рисования: по байту на клетку, 255 — полностью закрыто, 0 — открыто.
	// Граница размывается (каждый проход — сглаживание по соседям «1-2-1» по обеим осям), и
	// растянутая со сглаживанием маска даёт мягкий край вместо ступенек клеток.
	void BuildAlpha(TArray<uint8>& OutAlpha, int32 BlurPasses) const
	{
		OutAlpha.Reset();
		if (!IsValid())
		{
			return;
		}
		TArray<float> Value, Temp;
		Value.SetNumUninitialized(Cells.Num());
		Temp.SetNumUninitialized(Cells.Num());
		for (int32 Index = 0; Index < Cells.Num(); ++Index)
		{
			Value[Index] = Cells[Index] ? 0.0f : 1.0f;
		}
		for (int32 Pass = 0; Pass < FMath::Clamp(BlurPasses, 0, 8); ++Pass)
		{
			for (int32 Y = 0; Y < Height; ++Y)
			{
				const float* Row = &Value[Y * Width];
				float* OutRow = &Temp[Y * Width];
				for (int32 X = 0; X < Width; ++X)
				{
					OutRow[X] = (Row[FMath::Max(X - 1, 0)] + 2.0f * Row[X] + Row[FMath::Min(X + 1, Width - 1)]) * 0.25f;
				}
			}
			for (int32 Y = 0; Y < Height; ++Y)
			{
				const float* Up = &Temp[FMath::Max(Y - 1, 0) * Width];
				const float* Mid = &Temp[Y * Width];
				const float* Down = &Temp[FMath::Min(Y + 1, Height - 1) * Width];
				float* OutRow = &Value[Y * Width];
				for (int32 X = 0; X < Width; ++X)
				{
					OutRow[X] = (Up[X] + 2.0f * Mid[X] + Down[X]) * 0.25f;
				}
			}
		}
		OutAlpha.SetNumUninitialized(Cells.Num());
		for (int32 Index = 0; Index < Cells.Num(); ++Index)
		{
			OutAlpha[Index] = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Value[Index] * 255.0f), 0, 255));
		}
	}

	bool operator==(const FPdaMapFog& Other) const
	{
		return Width == Other.Width && Height == Other.Height && Cells == Other.Cells;
	}

private:
	int32 CellIndexAt(const FVector2D& UV) const
	{
		const int32 X = FMath::Clamp(FMath::FloorToInt32(UV.X * Width), 0, Width - 1);
		const int32 Y = FMath::Clamp(FMath::FloorToInt32(UV.Y * Height), 0, Height - 1);
		return Y * Width + X;
	}

	int32 Width = 0;
	int32 Height = 0;
	TBitArray<> Cells; // по строкам сверху вниз
};
