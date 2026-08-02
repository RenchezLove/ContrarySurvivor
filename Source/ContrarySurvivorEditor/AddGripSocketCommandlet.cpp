// Fill out your copyright notice in the Description page of Project Settings.

#include "AddGripSocketCommandlet.h"

#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogAddGripSocket, Log, All);

namespace
{
	const TCHAR* GGripSocketName = TEXT("WeaponGripSocket");
	const TCHAR* GGripBoneName = TEXT("R_Hand");

	// Скелет лидер-меша ИГРОКА (PreProduction-дубль) и скелет лидер-меша БАНДИТА (Shared).
	// Пути сверены живым чтением CDO BP_PlayerCharacter/BP_Bandit (probe_socket.py, 08-01)
	// и повторно срезом мешей 08-02 (build122-socket-dump-before.txt): голова игрока
	// HeadAndSkeletonfbx_Head -> PreProduction, все остальные меши гуманоида -> Shared.
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

	bool SavePackageOf(UObject* Asset)
	{
		const FString PackageName = Asset->GetOutermost()->GetName();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			PackageName, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		if (!UPackage::SavePackage(Asset->GetOutermost(), Asset, *Filename, SaveArgs))
		{
			UE_LOG(LogAddGripSocket, Error, TEXT("SavePackage не сохранил %s."), *Filename);
			return false;
		}
		UE_LOG(LogAddGripSocket, Display, TEXT("Сохранён %s"), *Filename);
		return true;
	}

	// Трансформ кости в референс-позе В ПРОСТРАНСТВЕ КОМПОНЕНТА (композиция локальных
	// трансформов вверх по цепочке родителей). Нужен для среза: масштаб кости показывает,
	// во что реально превращаются «местные единицы сокета» (у гуманоида кость крупная).
	bool GetRefPoseComponentTransform(const USkeleton* Skeleton, const FName BoneName, FTransform& OutTM)
	{
		const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
		int32 BoneIndex = Ref.FindBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE)
		{
			return false;
		}
		const TArray<FTransform>& Pose = Ref.GetRefBonePose();
		FTransform TM = Pose[BoneIndex];
		for (int32 Parent = Ref.GetParentIndex(BoneIndex); Parent != INDEX_NONE;
			Parent = Ref.GetParentIndex(Parent))
		{
			TM = TM * Pose[Parent];
		}
		OutTM = TM;
		return true;
	}

	void LogSocket(const TCHAR* Prefix, const USkeletalMeshSocket* Socket)
	{
		UE_LOG(LogAddGripSocket, Display, TEXT("%s%s | кость=%s | loc=%s | rot=%s | scale=%s"),
			Prefix, *Socket->SocketName.ToString(), *Socket->BoneName.ToString(),
			*Socket->RelativeLocation.ToString(), *Socket->RelativeRotation.ToString(),
			*Socket->RelativeScale.ToString());
	}
}

int32 UAddGripSocketCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	if (Switches.Contains(TEXT("dump")))
	{
		return DumpState();
	}
	if (Switches.Contains(TEXT("normalize")))
	{
		return NormalizeSockets();
	}
	if (Switches.Contains(TEXT("verify")))
	{
		const int32 A = VerifySkeleton(GPlayerSkeletonPath);
		const int32 B = VerifySkeleton(GBanditSkeletonPath);

		// Build 1.2.2: трансформы сокета на обоих скелетах обязаны СОВПАДАТЬ (иначе игрок
		// и бандит держат оружие по-разному) и на мешах /Game не должно быть дублей.
		int32 C = 0;
		USkeleton* PlayerSkeleton = LoadSkeletonChecked(GPlayerSkeletonPath);
		USkeleton* BanditSkeleton = LoadSkeletonChecked(GBanditSkeletonPath);
		USkeletalMeshSocket* PlayerSocket = PlayerSkeleton ? FindGripSocket(PlayerSkeleton) : nullptr;
		USkeletalMeshSocket* BanditSocket = BanditSkeleton ? FindGripSocket(BanditSkeleton) : nullptr;
		if (PlayerSocket && BanditSocket)
		{
			if (!PlayerSocket->RelativeLocation.Equals(BanditSocket->RelativeLocation)
				|| !PlayerSocket->RelativeRotation.Equals(BanditSocket->RelativeRotation))
			{
				UE_LOG(LogAddGripSocket, Error,
					TEXT("ПРОВЕРКА: трансформы сокета на двух скелетах РАСХОДЯТСЯ — прогоните -normalize."));
				C = 1;
			}
		}

		TArray<USkeletalMesh*> Meshes;
		if (!CollectGameSkeletalMeshes(Meshes))
		{
			C = 1;
		}
		for (USkeletalMesh* Mesh : Meshes)
		{
			for (const USkeletalMeshSocket* MeshSocket : Mesh->GetMeshOnlySocketList())
			{
				if (MeshSocket && MeshSocket->SocketName == FName(GGripSocketName))
				{
					UE_LOG(LogAddGripSocket, Error,
						TEXT("ПРОВЕРКА: ДУБЛЬ сокета %s прямо на меше %s (должен жить только на скелетах) — прогоните -normalize."),
						GGripSocketName, *Mesh->GetPathName());
					C = 1;
				}
			}
		}
		if (C == 0)
		{
			UE_LOG(LogAddGripSocket, Display,
				TEXT("ПРОВЕРКА Build 1.2.2: трансформы скелетов совпадают, дублей на %d мешах /Game нет."),
				Meshes.Num());
		}
		return (A == 0 && B == 0 && C == 0) ? 0 : 1;
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

	if (!SavePackageOf(Skeleton))
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
	// Build 1.2.2: scale строго 1 — иначе масштабируются цифровые поправки хвата
	// и уродуется превью в редакторе скелета (см. контракт в шапке .h).
	if (!Socket->RelativeScale.Equals(FVector::OneVector))
	{
		UE_LOG(LogAddGripSocket, Error,
			TEXT("ПРОВЕРКА: сокет %s на %s имеет scale=%s, требуется (1,1,1) — прогоните -normalize."),
			GGripSocketName, SkeletonPath, *Socket->RelativeScale.ToString());
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
	if (!SavePackageOf(BanditSkeleton))
	{
		return 1;
	}
	UE_LOG(LogAddGripSocket, Display,
		TEXT("SYNC ГОТОВО: трансформ сокета игрока перенесён бандитам: loc=%s rot=%s scale=%s"),
		*Dst->RelativeLocation.ToString(), *Dst->RelativeRotation.ToString(),
		*Dst->RelativeScale.ToString());
	return 0;
}

bool UAddGripSocketCommandlet::CollectGameSkeletalMeshes(TArray<USkeletalMesh*>& OutMeshes)
{
	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	AssetRegistry.ScanPathsSynchronous({ TEXT("/Game") }, /*bForceRescan=*/true);

	FARFilter Filter;
	Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(TEXT("/Game"));
	Filter.bRecursivePaths = true;

	TArray<FAssetData> Assets;
	if (!AssetRegistry.GetAssets(Filter, Assets))
	{
		UE_LOG(LogAddGripSocket, Error, TEXT("Реестр ассетов не отдал скелетные меши /Game."));
		return false;
	}
	for (const FAssetData& Asset : Assets)
	{
		if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset.GetAsset()))
		{
			OutMeshes.Add(Mesh);
		}
		else
		{
			UE_LOG(LogAddGripSocket, Warning, TEXT("Скелетный меш не загрузился: %s"),
				*Asset.GetSoftObjectPath().ToString());
		}
	}
	return true;
}

int32 UAddGripSocketCommandlet::DumpState()
{
	const TCHAR* Skeletons[] = { GPlayerSkeletonPath, GBanditSkeletonPath };
	for (const TCHAR* Path : Skeletons)
	{
		USkeleton* Skeleton = LoadSkeletonChecked(Path);
		if (!Skeleton)
		{
			return 1;
		}
		UE_LOG(LogAddGripSocket, Display, TEXT("=== СКЕЛЕТ %s: сокетов %d"), Path, Skeleton->Sockets.Num());
		for (const USkeletalMeshSocket* Socket : Skeleton->Sockets)
		{
			if (Socket)
			{
				LogSocket(TEXT("  "), Socket);
			}
		}
		FTransform HandTM;
		if (GetRefPoseComponentTransform(Skeleton, FName(GGripBoneName), HandTM))
		{
			UE_LOG(LogAddGripSocket, Display,
				TEXT("  кость %s в референс-позе (пространство компонента): loc=%s rot=%s scale=%s"),
				GGripBoneName, *HandTM.GetLocation().ToString(),
				*HandTM.Rotator().ToString(), *HandTM.GetScale3D().ToString());
		}
	}

	TArray<USkeletalMesh*> Meshes;
	if (!CollectGameSkeletalMeshes(Meshes))
	{
		return 1;
	}
	UE_LOG(LogAddGripSocket, Display, TEXT("=== СКЕЛЕТНЫЕ МЕШИ /Game: %d"), Meshes.Num());
	for (USkeletalMesh* Mesh : Meshes)
	{
		const USkeleton* MeshSkeleton = Mesh->GetSkeleton();
		UE_LOG(LogAddGripSocket, Display, TEXT("МЕШ %s | скелет=%s | сокетов на самом меше: %d"),
			*Mesh->GetPathName(), MeshSkeleton ? *MeshSkeleton->GetPathName() : TEXT("НЕТ"),
			Mesh->GetMeshOnlySocketList().Num());
		for (const USkeletalMeshSocket* MeshSocket : Mesh->GetMeshOnlySocketList())
		{
			if (MeshSocket)
			{
				LogSocket(TEXT("  [МЕШ-СОКЕТ] "), MeshSocket);
			}
		}
	}
	return 0;
}

int32 UAddGripSocketCommandlet::NormalizeSockets()
{
	// Источник позы — Shared-скелет: именно туда редактор скелета сохранил ручную
	// настройку Рината под пистолет (коммит 30aedb1; подтверждено срезом 08-02).
	USkeleton* BanditSkeleton = LoadSkeletonChecked(GBanditSkeletonPath);
	USkeleton* PlayerSkeleton = LoadSkeletonChecked(GPlayerSkeletonPath);
	if (!BanditSkeleton || !PlayerSkeleton)
	{
		return 1;
	}
	USkeletalMeshSocket* Source = FindGripSocket(BanditSkeleton);
	if (!Source)
	{
		UE_LOG(LogAddGripSocket, Error,
			TEXT("NORMALIZE: на Shared-скелете нет сокета %s — сначала основной проход."), GGripSocketName);
		return 1;
	}
	const FVector PoseLocation = Source->RelativeLocation;
	const FRotator PoseRotation = Source->RelativeRotation;
	UE_LOG(LogAddGripSocket, Display,
		TEXT("NORMALIZE: поза Рината (Shared): loc=%s rot=%s (старый scale=%s -> станет (1,1,1))"),
		*PoseLocation.ToString(), *PoseRotation.ToString(), *Source->RelativeScale.ToString());

	// Масштаб сокета на позу прикреплённого оружия не влияет (SnapToTargetNotIncludingScale:
	// rel loc/rot обнуляются, мировой масштаб оружия сохраняется; мировая поза сокета от
	// его RelativeScale не зависит) — поэтому scale=(1,1,1) позу пистолета НЕ сдвигает.
	USkeleton* Skeletons[] = { BanditSkeleton, PlayerSkeleton };
	for (USkeleton* Skeleton : Skeletons)
	{
		USkeletalMeshSocket* Socket = FindGripSocket(Skeleton);
		if (!Socket)
		{
			UE_LOG(LogAddGripSocket, Error, TEXT("NORMALIZE: на %s нет сокета %s — сначала основной проход."),
				*Skeleton->GetPathName(), GGripSocketName);
			return 1;
		}
		const bool bChanged = !Socket->RelativeLocation.Equals(PoseLocation)
			|| !Socket->RelativeRotation.Equals(PoseRotation)
			|| !Socket->RelativeScale.Equals(FVector::OneVector);
		if (!bChanged)
		{
			UE_LOG(LogAddGripSocket, Display, TEXT("NORMALIZE: %s уже в порядке — не трогаю."),
				*Skeleton->GetPathName());
			continue;
		}
		Skeleton->Modify();
		Socket->RelativeLocation = PoseLocation;
		Socket->RelativeRotation = PoseRotation;
		Socket->RelativeScale = FVector::OneVector;
		if (!SavePackageOf(Skeleton))
		{
			return 1;
		}
		LogSocket(TEXT("NORMALIZE ГОТОВО: "), FindGripSocket(Skeleton));
	}

	// Дубли сокета прямо на мешах — удалить (сокет меша брони терялся бы при переодевании).
	TArray<USkeletalMesh*> Meshes;
	if (!CollectGameSkeletalMeshes(Meshes))
	{
		return 1;
	}
	for (USkeletalMesh* Mesh : Meshes)
	{
		TArray<TObjectPtr<USkeletalMeshSocket>>& MeshSockets = Mesh->GetMeshOnlySocketList();
		const bool bHasDuplicate = MeshSockets.ContainsByPredicate(
			[](const TObjectPtr<USkeletalMeshSocket>& S)
			{ return S && S->SocketName == FName(GGripSocketName); });
		if (!bHasDuplicate)
		{
			continue; // меш чистый — пакет не трогаем и не пересохраняем
		}
		Mesh->Modify();
		MeshSockets.RemoveAll([](const TObjectPtr<USkeletalMeshSocket>& S)
			{ return S && S->SocketName == FName(GGripSocketName); });
		if (!SavePackageOf(Mesh))
		{
			return 1;
		}
		UE_LOG(LogAddGripSocket, Display, TEXT("NORMALIZE: удалён дубль сокета с меша %s."),
			*Mesh->GetPathName());
	}

	// Точная поправка ножа = ИНВЕРСИЯ сокет-трансформа (при scale=1): поправка ∘ сокет =
	// тождество -> нож снова лежит ровно по кости R_Hand, как до правки сокета.
	const FTransform SocketTM(PoseRotation, PoseLocation, FVector::OneVector);
	const FTransform KnifeFix = SocketTM.Inverse();
	UE_LOG(LogAddGripSocket, Display,
		TEXT("ПОПРАВКА НОЖА (в конструктор AMeleeWeapon): GripOffsetLocation=%s GripOffsetRotation=%s"),
		*KnifeFix.GetLocation().ToString(), *KnifeFix.Rotator().ToString());
	const FTransform Check = KnifeFix * SocketTM;
	UE_LOG(LogAddGripSocket, Display,
		TEXT("Самопроверка (поправка ∘ сокет, должно быть ~тождество): loc=%s rot=%s"),
		*Check.GetLocation().ToString(), *Check.Rotator().ToString());
	return 0;
}
