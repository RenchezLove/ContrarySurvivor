// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemySpawnPointComponent.h"

UEnemySpawnPointComponent::UEnemySpawnPointComponent()
{
	// Видимый в редакторе маркер, скрытый в игре: стрелка показывает точку и направление врага.
	bHiddenInGame = true;

	// Ярко-красная заметная стрелка приличной длины — чтобы дизайнер легко находил/двигал точки
	// во вьюпорте BP. Значения — стартовые, тюнятся в редакторе на самом компоненте.
	// (ArrowColor/ArrowSize/ArrowLength/bIsScreenSizeScaled — постоянные члены UArrowComponent,
	// не editor-only; сама стрелка рендерится только в редакторе.)
	ArrowColor = FColor(255, 40, 40, 255);
	ArrowSize = 1.5f;
	ArrowLength = 120.0f;
	// Не масштабировать по экрану — пусть стрелка имеет реальный мировой размер в сцене.
	bIsScreenSizeScaled = false;
}
