// Fill out your copyright notice in the Description page of Project Settings.

#include "AShotgun.h"
#include "AAmmoItem.h" // ContraryAmmo: вид дробового патрона
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AShotgun::AShotgun()
{
	// Сетка ружья (оси и точка хвата как у SM_Pistol: опора в шейке приклада). Материал уже
	// на ассете. Чертёж-наследник может переопределить.
	if (ItemMesh)
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> ShotgunMeshAsset(
			TEXT("/Game/Weapons/Shotgun/SM_Shotgun_TOZ34.SM_Shotgun_TOZ34"));
		if (ShotgunMeshAsset.Succeeded())
		{
			ItemMesh->SetStaticMesh(ShotgunMeshAsset.Object);
		}
		// Носится прикреплённым к персонажу; собственная коллизия не нужна (как у пистолета).
		ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// --- Параметры двустволки: ВСЕ ЧИСЛА ЧЕРНОВЫЕ, на утверждение Рината ---
	// Залп в упор = 8 дробин x 8 урона = 64, то есть примерно в 2,5 раза больше одного
	// выстрела пистолета (25).
	Damage             = 8.0f;    // урон ОДНОЙ дробины
	PelletsPerShot     = 8;
	PelletSpreadAngle  = 16.0f;   // полный угол конуса, градусы
	FireRate           = 1.0f;    // один выстрел в секунду — заметная пауза между стволами
	Range              = 1000.0f; // дальность полёта дроби (у пистолета 1500)
	WeaponType         = EWeaponType::TwoHanded;
	FirearmSlot        = EFirearmSlot::Long; // длинное двуручное: свой слот, рядом с пистолетом
	Spread             = 0.0f;    // разброс пули не используется: у дроби свой конус

	// Патроны: два в стволах, потом перезарядка. Запас — сколько патронов приходит вместе
	// с ружьём (при взятии переливается видимой пачкой в рюкзак, как у пистолета).
	MaxAmmoInClip      = 2;
	CurrentAmmoInClip  = MaxAmmoInClip;
	MaxAmmoReserve     = 8;
	CurrentAmmoReserve = MaxAmmoReserve;
	AmmoType           = ContraryAmmo::Shotgun12gType();

	// Имя предмета. Ключ = имя строки таблицы предметов (путь новых предметов).
	ItemName        = FString("shotgun_toz34");
	ItemDisplayText = NSLOCTEXT("Items", "ShotgunToz34", "Двустволка");
	ItemDescription = FString("Double-barrel shotgun. Two shells, wide pellet spread.");

	// Картинка — только из строки таблицы DT_Items (ADR-088 п.2), здесь не задаётся.

	// Эффекты: дульный срез в 87 см от точки хвата (длина ствола по сетке), вспышка и
	// отдача крупнее пистолетных, следы дробин тоньше следа пули.
	MuzzleSocketName = FName("MuzzleSocket");
	MuzzleOffset     = FVector(87.0f, 0.0f, 0.0f);
	MuzzleFlashSize  = 0.25f;
	TracerThickness  = 1.5f;
	FireShakeTrauma  = 0.25f;
}
