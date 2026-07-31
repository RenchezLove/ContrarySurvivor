// Fill out your copyright notice in the Description page of Project Settings.

#include "AddGripSocketCommandlet.h"

#include "Animation/Skeleton.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogAddGripSocket, Log, All);

namespace
{
	const TCHAR* GGripSocketName = TEXT("WeaponGripSocket");
	const TCHAR* GGripBoneName = TEXT("R_Hand");

	// Скелет лидер-меша ИГРОКА (PreProduction-дубль) и скелет лидер-меша БАНДИТА (Shared).
	// Пути сверены живым чтением CDO BP_PlayerCharacter/BP_Bandit (probe_socket.py, 08-01).
	const TCHAR* GPlayerSkeletonPath =
		TEXT("/Game/TestContentAndCode/PreProduction/HeadAndSkeletonfbx_Head_Skeleton.HeadAndSkeletonfbx_Head_Skeleton");
	const TCHAR* GBanditSkeletonPath =
		TEXT("/Game/Characters/Shared/Humanoid/HeadAndSkeletonfbx_Head_Skeleton.HeadAndSkeletonfbx_Head_Skeleton");

	USkeleton* LoadSkeletonChecked(const TCHAR* Path)
	{
		USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, Path);
		if (!Skeleton)
		{
			UE_LOG(LogAddGripSocket, Error, TEXT("Скелет не загрузился: %s"), Path);
		}
		return Skeleton;
	}

	USkeletalMeshSocket* FindGripSocket(USkeleton* Skeleton)
	{
		for (USkeletalMeshSocket* Socket : Skeleton->Sockets)
		{
			if (Socket && Socket->SocketName == FName(GGripSocketName))
			{
				return Socket;
			}
		}
		return nullptr;
	}

	bool SaveSkeletonPackage(USkeleton* Skeleton)
	{
		const FString PackageName = Skeleton->GetOutermost()->GetName();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			PackageName, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		if (!UPackage::SavePackage(Skeleton->GetOutermost(), Skeleton, *Filename, SaveArgs))
		{
			UE_LOG(LogAddGripSocket, Error, TEXT("SavePackage не сохранил %s."), *Filename);
			return false;
		}
		UE_LOG(LogAddGripSocket, Display, TEXT("Сохранён %s"), *Filename);
		return true;
	}
}

int32 UAddGripSocketCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	if (Switches.Contains(TEXT("verify")))
	{
		const int32 A = VerifySkeleton(GPlayerSkeletonPath);
		const int32 B = VerifySkeleton(GBanditSkeletonPath);
		return (A == 0 && B == 0) ? 0 : 1;
	}
	if (Switches.Contains(TEXT("sync")))
	{
		return SyncFromPlayer();
	}

	const int32 A = AddToSkeleton(GPlayerSkeletonPath);
	const int32 B = AddToSkeleton(GBanditSkeletonPath);
	return (A == 0 && B == 0) ? 0 : 1;
}

int32 UAddGripSocketCommandlet::AddToSkeleton(const TCHAR* SkeletonPath)
{
	USkeleton* Skeleton = LoadSkeletonChecked(SkeletonPath);
	if (!Skeleton)
	{
		return 1;
	}

	if (FindGripSocket(Skeleton))
	{
		UE_LOG(LogAddGripSocket, Display,
			TEXT("Сокет %s уже есть на %s — ассет не трогаю."), GGripSocketName, SkeletonPath);
		return 0;
	}

	// Кость должна существовать — иначе сокет мёртвый (тот же контракт, что у
	// USkeletalMesh::AddSocket, SkeletalMesh.cpp:4585).
	if (Skeleton->GetReferenceSkeleton().FindBoneIndex(FName(GGripBoneName)) == INDEX_NONE)
	{
		UE_LOG(LogAddGripSocket, Error,
			TEXT("Кость %s НЕ найдена на скелете %s — НЕ сохраняю, ассет цел."),
			GGripBoneName, SkeletonPath);
		return 1;
	}

	Skeleton->Modify();
	USkeletalMeshSocket* Socket = NewObject<USkeletalMeshSocket>(
		Skeleton, FName(GGripSocketName), RF_Transactional);
	Socket->SocketName = FName(GGripSocketName);
	Socket->BoneName = FName(GGripBoneName);
	Socket->RelativeLocation = FVector::ZeroVector;   // текущие офсеты хвата нулевые (проверено CDO)
	Socket->RelativeRotation = FRotator::ZeroRotator; // Ринат двигает мышкой в редакторе скелета
	Skeleton->Sockets.Add(Socket);

	if (!SaveSkeletonPackage(Skeleton))
	{
		return 1;
	}
	UE_LOG(LogAddGripSocket, Display,
		TEXT("ГОТОВО: сокет %s (кость %s, трансформ нулевой) добавлен на %s."),
		GGripSocketName, GGripBoneName, SkeletonPath);
	return 0;
}

int32 UAddGripSocketCommandlet::VerifySkeleton(const TCHAR* SkeletonPath)
{
	USkeleton* Skeleton = LoadSkeletonChecked(SkeletonPath);
	if (!Skeleton)
	{
		return 1;
	}
	USkeletalMeshSocket* Socket = FindGripSocket(Skeleton);
	if (!Socket)
	{
		UE_LOG(LogAddGripSocket, Error, TEXT("ПРОВЕРКА: сокета %s НЕТ на %s."),
			GGripSocketName, SkeletonPath);
		return 1;
	}
	if (Socket->BoneName != FName(GGripBoneName))
	{
		UE_LOG(LogAddGripSocket, Error,
			TEXT("ПРОВЕРКА: сокет %s на %s сидит на кости '%s', ожидалась '%s'."),
			GGripSocketName, SkeletonPath, *Socket->BoneName.ToString(), GGripBoneName);
		return 1;
	}
	UE_LOG(LogAddGripSocket, Display,
		TEXT("ПРОВЕРКА ПРОЙДЕНА: %s на %s, кость %s, loc=%s rot=%s scale=%s"),
		GGripSocketName, SkeletonPath, *Socket->BoneName.ToString(),
		*Socket->RelativeLocation.ToString(), *Socket->RelativeRotation.ToString(),
		*Socket->RelativeScale.ToString());
	return 0;
}

int32 UAddGripSocketCommandlet::SyncFromPlayer()
{
	USkeleton* PlayerSkeleton = LoadSkeletonChecked(GPlayerSkeletonPath);
	USkeleton* BanditSkeleton = LoadSkeletonChecked(GBanditSkeletonPath);
	if (!PlayerSkeleton || !BanditSkeleton)
	{
		return 1;
	}
	USkeletalMeshSocket* Src = FindGripSocket(PlayerSkeleton);
	USkeletalMeshSocket* Dst = FindGripSocket(BanditSkeleton);
	if (!Src || !Dst)
	{
		UE_LOG(LogAddGripSocket, Error,
			TEXT("SYNC: сокет отсутствует (игрок: %s, бандит: %s) — сначала основной проход."),
			Src ? TEXT("есть") : TEXT("НЕТ"), Dst ? TEXT("есть") : TEXT("НЕТ"));
		return 1;
	}
	if (Src->RelativeLocation.Equals(Dst->RelativeLocation)
		&& Src->RelativeRotation.Equals(Dst->RelativeRotation)
		&& Src->RelativeScale.Equals(Dst->RelativeScale))
	{
		UE_LOG(LogAddGripSocket, Display, TEXT("SYNC: трансформы уже совпадают — ассет не трогаю."));
		return 0;
	}
	BanditSkeleton->Modify();
	Dst->RelativeLocation = Src->RelativeLocation;
	Dst->RelativeRotation = Src->RelativeRotation;
	Dst->RelativeScale = Src->RelativeScale;
	if (!SaveSkeletonPackage(BanditSkeleton))
	{
		return 1;
	}
	UE_LOG(LogAddGripSocket, Display,
		TEXT("SYNC ГОТОВО: трансформ сокета игрока перенесён бандитам: loc=%s rot=%s scale=%s"),
		*Dst->RelativeLocation.ToString(), *Dst->RelativeRotation.ToString(),
		*Dst->RelativeScale.ToString());
	return 0;
}
