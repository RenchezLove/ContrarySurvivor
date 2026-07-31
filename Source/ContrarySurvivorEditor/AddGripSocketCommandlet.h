// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "AddGripSocketCommandlet.generated.h"

/**
 * Сокет хвата оружия на скелетах гуманоида (Build 1.2, задание Рината: «Я могу настроить
 * положение пистолета в руке сам в редакторе. Что бы было наглядно. Без выставления всего
 * цифрами, а перемещая сокет»).
 *
 * ЗАЧЕМ КОДОМ. Из Python сокет не создать: у USkeletalMeshSocket поля SocketName/BoneName
 * помечены VisibleAnywhere/BlueprintReadOnly и запись через set_editor_property падает
 * («read-only», проверено живым прогоном probe_socket.py 08-01). Редакторный C++ пишет поля
 * напрямую — тот же путь, что PatchAnimBp/GenerateWbp (ADR-048/053).
 *
 * ЧТО ДЕЛАЕТ. Добавляет авторский сокет WeaponGripSocket на кость R_Hand с нулевым
 * относительным трансформом на ОБА скелета-дубля гуманоида (лидер-меш игрока живёт на
 * PreProduction-копии скелета, лидер-меш бандита — на Shared-копии; сведение дублей — в
 * бэклоге, ADR-053 п.3). Нулевой трансформ = текущее поведение (фактические офсеты
 * WeaponGripLocation/Rotation у игрока и бандита нулевые — проверено чтением CDO 08-01).
 * Дальше Ринат двигает сокет мышкой в редакторе скелета, EquipWeapon подхватывает сокет
 * приоритетно (MasterHumanoidCharacter.cpp, ветка bUseGripSocket).
 *
 * Запуск (редактор НЕ нужен, headless):
 *   UnrealEditor-Cmd.exe <проект.uproject> -run=AddGripSocket          — добавить сокет на оба
 *     скелета (идемпотентно: сокет уже есть — ассет не трогается)
 *   UnrealEditor-Cmd.exe <проект.uproject> -run=AddGripSocket -verify  — проверка без правок:
 *     сокет на месте на обоих скелетах, печать кости и трансформа
 *   UnrealEditor-Cmd.exe <проект.uproject> -run=AddGripSocket -sync    — скопировать трансформ
 *     сокета со скелета ИГРОКА на скелет бандита (после ручной настройки Рината, чтобы у
 *     бандитов пистолет сидел так же)
 */
UCLASS()
class UAddGripSocketCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;

private:
	// Добавляет сокет на один скелет. 0 — успех (в т.ч. «уже есть, не трогаю»).
	int32 AddToSkeleton(const TCHAR* SkeletonPath);

	// Проверка одного скелета: сокет есть, кость верная. 0 — порядок.
	int32 VerifySkeleton(const TCHAR* SkeletonPath);

	// Копирует RelativeLocation/Rotation/Scale сокета игрока на скелет бандита.
	int32 SyncFromPlayer();
};
