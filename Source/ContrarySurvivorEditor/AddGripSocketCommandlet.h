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
 * ЧТО ДЕЛАЕТ. Основной проход добавляет авторский сокет WeaponGripSocket на кость R_Hand
 * на ОБА скелета-дубля гуманоида (лидер-меш игрока живёт на PreProduction-копии скелета,
 * лидер-меш бандита — на Shared-копии; сведение дублей — в бэклоге, ADR-053 п.3).
 * Ринат двигает сокет мышкой в редакторе скелета, EquipWeapon подхватывает сокет
 * приоритетно (MasterHumanoidCharacter.cpp, ветка bUseGripSocket); с Build 1.2.2 поверх
 * сокета всегда применяется цифровая поправка класса оружия (AMasterWeapon::GripOffset*).
 *
 * КОНТРАКТ Build 1.2.2 (эта волна): рабочий сокет живёт ТОЛЬКО на двух скелетах, с
 * RelativeScale строго (1,1,1) и ОДИНАКОВЫМ трансформом на обоих; на самих мешах
 * дублей-сокетов с этим именем быть не должно (сокет меша брони терялся бы при
 * переодевании — EquipArmor подменяет меши слотов). Масштаб сокета на итоговую позу
 * оружия НЕ влияет (attach SnapToTargetNotIncludingScale: позиция/поворот берутся из
 * мировой позы сокета, масштаб оружия сохраняется мировым — SceneComponent.cpp:2258+),
 * но растит/зеркалит превью в редакторе и масштабировал бы цифровые поправки — поэтому
 * нормализуется в 1.
 *
 * Запуск (редактор НЕ нужен, headless):
 *   UnrealEditor-Cmd.exe <проект.uproject> -run=AddGripSocket          — добавить сокет на оба
 *     скелета (идемпотентно: сокет уже есть — ассет не трогается)
 *   UnrealEditor-Cmd.exe <проект.uproject> -run=AddGripSocket -dump    — срез без правок:
 *     сокеты обоих скелетов, трансформ кости R_Hand в референс-позе, все скелетные меши
 *     /Game (чей скелет + есть ли дубль сокета на самом меше)
 *   UnrealEditor-Cmd.exe <проект.uproject> -run=AddGripSocket -normalize — навести порядок:
 *     поза сокета с Shared-скелета (ручная настройка Рината под пистолет, Build 1.2.2)
 *     копируется на ОБА скелета, RelativeScale = (1,1,1); дубли сокета на мешах удаляются;
 *     печатается точная поправка ножа = инверсия сокет-трансформа
 *   UnrealEditor-Cmd.exe <проект.uproject> -run=AddGripSocket -verify  — проверка без правок:
 *     сокет на обоих скелетах, кость R_Hand, scale=(1,1,1), трансформы скелетов совпадают,
 *     на мешах /Game дублей нет
 *   UnrealEditor-Cmd.exe <проект.uproject> -run=AddGripSocket -sync    — скопировать трансформ
 *     сокета со скелета ИГРОКА (PreProduction) на скелет бандита (Shared). ВНИМАНИЕ:
 *     направление именно игрок→бандит; после -normalize оба и так совпадают
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

	// Проверка одного скелета: сокет есть, кость верная, scale=(1,1,1). 0 — порядок.
	int32 VerifySkeleton(const TCHAR* SkeletonPath);

	// Копирует RelativeLocation/Rotation/Scale сокета игрока на скелет бандита.
	int32 SyncFromPlayer();

	// Срез фактического состояния (только чтение): сокеты скелетов, кость R_Hand в
	// референс-позе, скелетные меши /Game с их скелетами и дублями сокета.
	int32 DumpState();

	// Build 1.2.2: поза Рината (Shared) -> оба скелета, scale=1, дубли на мешах — удалить,
	// печать точной поправки ножа (инверсия сокет-трансформа).
	int32 NormalizeSockets();

	// Все скелетные меши /Game (через реестр ассетов). Возвращает false при сбое реестра.
	bool CollectGameSkeletalMeshes(TArray<class USkeletalMesh*>& OutMeshes);
};
