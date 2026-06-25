// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ArrowComponent.h"
#include "EnemySpawnPointComponent.generated.h"

/**
 * Маркер ТОЧКИ СПАВНА врага для AMasterEnemyBase (задача Рината: точки должны быть видны и
 * перемещаемы во вьюпорте BP_WolfDen / BP_BanditBase, дизайнер сам их расставляет).
 *
 * Наследник UArrowComponent: рисуется стрелкой-спрайтом в редакторе (видна, выделяется,
 * двигается gizmo), в игре не отображается. Отдельный тип нужен, чтобы AMasterEnemyBase брал
 * ТОЛЬКО эти компоненты как позиции спавна (GetComponents<UEnemySpawnPointComponent>()) и не
 * путал их с мешем базы/триггерами/прочими стрелками. Стрелка ещё и показывает, КУДА смотрит
 * заспавненный враг (берём поворот точки).
 *
 * Использование в BP: добавить компонент «Enemy Spawn Point» в дерево BP-наследника, поставить
 * в нужное место. Сколько точек — столько врагов (число задаёт расстановка, не код).
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent, DisplayName = "Enemy Spawn Point"))
class CONTRARYSURVIVOR_API UEnemySpawnPointComponent : public UArrowComponent
{
	GENERATED_BODY()

public:
	UEnemySpawnPointComponent();
};
