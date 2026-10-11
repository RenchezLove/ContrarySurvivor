// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/QuestTrackerWidget.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/UI/QuestObjectiveText.h"
#include "ContrarySurvivor/Actors/ElderNPC.h"  // ожидание старосты после сценки про ноутбук
#include "ContrarySurvivor/Actors/TalkerNPC.h" // «взято ли задание, которым кончается сюжет»
#include "ContrarySurvivor/Data/ContraryPdaSettings.h" // цвета основных и побочных заданий
#include "ContrarySurvivor/UI/QuestTargets.h" // куда ведёт задание и подпись метки — общие с меткой основного
#include "Blueprint/WidgetLayoutLibrary.h" // масштаб экрана: точки экрана -> единицы раскладки
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

FText UQuestTrackerWidget::BuildElderWaitLine(EElderNotebookWaitPhase Phase, float RemainingSeconds,
	const FText& WaitFormat, const FText& ReadyText)
{
	switch (Phase)
	{
		case EElderNotebookWaitPhase::Waiting:
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Minutes"), FText::AsNumber(AElderNPC::GetNotebookWaitMinutesLeft(RemainingSeconds)));
			return FText::Format(WaitFormat, Args);
		}
		case EElderNotebookWaitPhase::Ready:
			return ReadyText;
		default:
			return FText::GetEmpty();
	}
}

void UQuestTrackerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!TrackerText)
	{
		return;
	}

	AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(GetOwningPlayer());
	APlayerCharacter* Player = PC ? Cast<APlayerCharacter>(PC->GetPawn()) : nullptr;
	UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	const FQuest* Tracked = Quests ? Quests->GetTrackedQuest() : nullptr;

	// Строка ожидания старосты (10.10.2026): живёт в этом же блоке — отдельно, когда активного
	// задания нет, и второй строкой под ним, когда есть.
	EElderNotebookWaitPhase WaitPhase = EElderNotebookWaitPhase::None;
	FText WaitLine;
	if (Player && Quests)
	{
		const float Remaining = Player->GetElderNotebookWaitRemaining();
		WaitPhase = AElderNPC::GetNotebookWaitPhase(Player->IsElderNotebookHintShown(),
			AQuestGiverNPC::IsStoryEndingQuestTaken(Quests->GetQuests()), Remaining);
		WaitLine = BuildElderWaitLine(WaitPhase, Remaining, ElderWaitFormat, ElderWaitReadyText);
	}

	// Прячемся без квеста и на модальных экранах (там квест виден в самом диалоге) —
	// перенос поведения Canvas DrawHUD. Именно содержимое, не сам виджет: Collapsed
	// на себе убил бы собственный тик (виджет не смог бы развернуться обратно).
	if ((!Tracked && WaitLine.IsEmpty()) || (PC && PC->IsAnyModalUIOpen()))
	{
		SetContentVisible(false);
		return;
	}
	SetContentVisible(true);

	// Синяя метка побочного задания: на экране вместе со строкой, прячется вместе с ней.
	RefreshSideQuestMarker(Player);
	PlaceSideQuestMarker(MyGeometry);

	if (!Tracked)
	{
		TrackerText->SetText(WaitLine);
		TrackerText->SetColorAndOpacity(FSlateColor(WaitPhase == EElderNotebookWaitPhase::Ready ? DoneColor : ActiveColor));
		return;
	}

	// Строка целей — общий сборщик (тот же, что у реплики старосты): человеческие подписи
	// целей вместо служебных тегов, единый формат в одном месте (ADR-050, порция 2).
	FFormatNamedArguments Args;
	Args.Add(TEXT("Title"), Tracked->Title);
	Args.Add(TEXT("Objectives"),
		QuestObjectiveText::BuildObjectives(*Tracked, ObjectiveFormat, ObjectiveSeparator));

	const bool bDone = (Tracked->State == EQuestState::Completed);
	// У задания своя подсказка, куда нести готовое, — общая строка «вернись к старосте» соврала бы.
	const bool bOwnHint = bDone && !Tracked->TurnInHintText.IsEmpty();
	Args.Add(TEXT("Hint"), Tracked->TurnInHintText);
	const FText QuestLine = FText::Format(bOwnHint ? DoneWithHintFormat : (bDone ? DoneFormat : TrackerFormat), Args);
	TrackerText->SetText(WaitLine.IsEmpty() ? QuestLine
		: FText::Join(FText::FromString(TEXT("\n")), QuestLine, WaitLine));
	// Идущее задание красится цветом своего вида (10.10.2026): основное оранжевым, побочное синим.
	TrackerText->SetColorAndOpacity(FSlateColor(bDone
		? DoneColor : UContraryPdaSettings::Get()->GetQuestKindColor(Tracked->Kind)));
}

// ===========================================================================
// Метка побочного задания в мире
// ===========================================================================

void UQuestTrackerWidget::ShowSideQuestMarker(const FVector& WorldPoint, const FText& Label)
{
	bSideMarkerWanted = true;
	SideMarkerWorldPoint = WorldPoint;
	if (!SideMarkerLabel.EqualTo(Label))
	{
		SideMarkerLabel = Label;
		if (SideQuestMarkerText)
		{
			SideQuestMarkerText->SetText(Label);
		}
	}
}

void UQuestTrackerWidget::HideSideQuestMarker()
{
	bSideMarkerWanted = false;
	for (UWidget* Element : { SideQuestMarker.Get(), SideQuestEdgeArrow.Get() })
	{
		if (Element && Element->GetVisibility() != ESlateVisibility::Collapsed)
		{
			Element->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UQuestTrackerWidget::RefreshSideQuestMarker(APlayerCharacter* Player)
{
	UWorld* World = Player ? Player->GetWorld() : nullptr;
	const UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	const FQuest* Side = Quests
		? QuestTargets::PickMarkerQuest(Quests->GetQuests(), Quests->GetTrackedQuest(), EQuestKind::Side) : nullptr;
	if (!World || !Side || Side->MapMarkerTag.IsNone())
	{
		SideTargetCache.Reset();
		SideTargetKey = 0;
		HideSideQuestMarker();
		return;
	}

	// Ключ поиска: задание, его метка и стадия (цель, шаг подготовки, сдача) — как у метки основного.
	uint32 Key = HashCombine(GetTypeHash(Side->QuestId), GetTypeHash(Side->MapMarkerTag));
	Key = HashCombine(Key, GetTypeHash(static_cast<uint8>(Side->State)));
	Key = HashCombine(Key, GetTypeHash(UQuestComponent::IsPrepStepShown(*Side) ? static_cast<uint8>(Side->PrepStep) : uint8(0)));

	AActor* Target = SideTargetCache.Get();
	if (!Target || Key != SideTargetKey)
	{
		const float Now = World->GetTimeSeconds();
		if (!Target && Key == SideTargetKey && Now < SideTargetNextSearchTime)
		{
			HideSideQuestMarker(); // недавно искали и не нашли — ждём
			return;
		}
		Target = QuestTargets::FindTargetActor(World, *Side);
		SideTargetCache = Target;
		SideTargetKey = Key;
		SideTargetNextSearchTime = Now + 0.5f;
	}
	if (!Target)
	{
		HideSideQuestMarker();
		return;
	}
	ShowSideQuestMarker(
		Target->GetActorLocation() + FVector(0.0, 0.0, QuestTargets::GetMarkerZOffset(*Side, Target, SideMarkerZOffset)),
		FText::FromString(QuestTargets::BuildMarkerLabel(*Side, SideMarkerTurnInPrefix.ToString())));
}

UQuestTrackerWidget::FMarkerPlacement UQuestTrackerWidget::ComputeMarkerPlacement(bool bInFront,
	const FVector2D& Projected, const FVector2D& BehindDir, const FVector2D& Area, float Margin)
{
	FMarkerPlacement Result;
	const FVector2D Center = Area * 0.5;
	if (bInFront && Projected.X >= Margin && Projected.X <= Area.X - Margin
		&& Projected.Y >= Margin && Projected.Y <= Area.Y - Margin)
	{
		Result.bInView = true;
		Result.Position = Projected;
		return Result;
	}

	// За кадром: точка у края экрана на луче из середины в сторону цели.
	FVector2D Dir = bInFront ? Projected - Center : BehindDir;
	if (Dir.IsNearlyZero())
	{
		Dir = FVector2D(0.0, -1.0);
	}
	const double HalfW = FMath::Max(1.0, Center.X - Margin);
	const double HalfH = FMath::Max(1.0, Center.Y - Margin);
	const double ScaleX = FMath::Abs(Dir.X) > KINDA_SMALL_NUMBER ? HalfW / FMath::Abs(Dir.X) : TNumericLimits<double>::Max();
	const double ScaleY = FMath::Abs(Dir.Y) > KINDA_SMALL_NUMBER ? HalfH / FMath::Abs(Dir.Y) : TNumericLimits<double>::Max();
	Result.Position = Center + Dir * FMath::Min(ScaleX, ScaleY);
	// Стрелка на картинке смотрит вверх; ось Y экрана направлена вниз.
	Result.ArrowAngle = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Dir.X, -Dir.Y)));
	return Result;
}

void UQuestTrackerWidget::PlaceSideQuestMarker(const FGeometry& MyGeometry)
{
	const APlayerController* PC = GetOwningPlayer();
	if (!bSideMarkerWanted || !PC || (!SideQuestMarker && !SideQuestEdgeArrow))
	{
		return; // спрятано в HideSideQuestMarker либо меток в файле окна нет
	}

	// Точка цели на экране игрока — в точках экрана; раскладка окна — в своих единицах.
	FVector2D ScreenPoint = FVector2D::ZeroVector;
	const bool bInFront = PC->ProjectWorldLocationToScreen(SideMarkerWorldPoint, ScreenPoint, /*bPlayerViewportRelative=*/true);
	FVector2D BehindDir(0.0, -1.0);
	if (!bInFront)
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
		const FRotationMatrix ViewAxes(ViewRotation);
		const FVector ToTarget = SideMarkerWorldPoint - ViewLocation;
		BehindDir = FVector2D(ToTarget | ViewAxes.GetUnitAxis(EAxis::Y), -(ToTarget | ViewAxes.GetUnitAxis(EAxis::Z)));
	}
	const float ViewportScale = FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(this));
	const FMarkerPlacement Placement = ComputeMarkerPlacement(bInFront, ScreenPoint / ViewportScale,
		BehindDir, MyGeometry.GetLocalSize(), SideMarkerEdgeMargin);

	auto Place = [&Placement](UWidget* Element, bool bShow)
	{
		if (!Element)
		{
			return;
		}
		const ESlateVisibility Wanted = bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		if (Element->GetVisibility() != Wanted)
		{
			Element->SetVisibility(Wanted);
		}
		UCanvasPanelSlot* ElementSlot = bShow ? Cast<UCanvasPanelSlot>(Element->Slot) : nullptr;
		if (ElementSlot && !ElementSlot->GetPosition().Equals(Placement.Position, 0.05))
		{
			ElementSlot->SetAnchors(FAnchors(0.0f, 0.0f));
			ElementSlot->SetPosition(Placement.Position);
		}
	};
	Place(SideQuestMarker, Placement.bInView);
	Place(SideQuestEdgeArrow, !Placement.bInView);
	if (SideQuestEdgeArrow && !Placement.bInView)
	{
		SideQuestEdgeArrow->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		SideQuestEdgeArrow->SetRenderTransformAngle(Placement.ArrowAngle);
	}
}
