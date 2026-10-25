// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"
#include "Chapter2Settings.generated.h"

class UTexture2D;
class USoundBase;

/**
 * Глава 2 «Вера», задание «Образцы из аномалий» (утверждено Ринатом 23.10.2026, поправка
 * 25.10.2026). Все числа, надписи, картинки и звук задания — здесь, на странице настроек
 * проекта «Contrary Survivor: глава 2» (раздел [/Script/ContrarySurvivor.Chapter2Settings]
 * в Config/DefaultGame.ini). Правятся без кода.
 *
 * Тексты — дословно из РЕПЛИКИ.txt (номера в квадратных скобках у каждого поля).
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Contrary Survivor: глава 2"))
class CONTRARYSURVIVOR_API UChapter2Settings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return FName(TEXT("Game")); }

	static const UChapter2Settings* Get() { return GetDefault<UChapter2Settings>(); }

	// --- Отравленная низина ---

	// Решение лида 23.10: «Без аппарата в зоне — урон каждую секунду, с аппаратом без воздуха — тоже».
	UPROPERTY(config, EditAnywhere, Category = "Низина", meta = (ClampMin = "0.0", DisplayPriority = "1",
		DisplayName = "Урон в низине без аппарата (здоровья в секунду)"))
	float ZoneDamageNoMaskPerSecond = 8.0f;

	UPROPERTY(config, EditAnywhere, Category = "Низина", meta = (ClampMin = "0.0", DisplayPriority = "2",
		DisplayName = "Урон в низине, когда воздух кончился (здоровья в секунду)"))
	float ZoneDamageNoAirPerSecond = 8.0f;

	// Насколько близко к краю низины (см) появляется кнопка «Надеть дыхательный аппарат».
	UPROPERTY(config, EditAnywhere, Category = "Низина", meta = (ClampMin = "0.0", DisplayPriority = "3",
		DisplayName = "Кнопка аппарата: расстояние до края низины (см)"))
	float MaskButtonEdgeDistance = 800.0f;

	// --- Воздух ---

	// Решение лида 23.10: «Воздуха в баллоне 3 минуты».
	UPROPERTY(config, EditAnywhere, Category = "Воздух", meta = (ClampMin = "1.0", DisplayPriority = "1",
		DisplayName = "Воздуха в полном баллоне (секунды)"))
	float AirCapacitySeconds = 180.0f;

	// Сколько секунд воздуха осталось, когда выходит сообщение [222] «Воздух на исходе».
	UPROPERTY(config, EditAnywhere, Category = "Воздух", meta = (ClampMin = "0.0", DisplayPriority = "2",
		DisplayName = "Порог «воздух на исходе» (секунды)"))
	float AirLowThresholdSeconds = 30.0f;

	// --- Детектор ---

	// Дальше этого (см) от ближайшего образца детектор молчит и полоска пуста.
	UPROPERTY(config, EditAnywhere, Category = "Детектор", meta = (ClampMin = "100.0", DisplayPriority = "1",
		DisplayName = "Дальность детектора (см)"))
	float DetectorRange = 3000.0f;

	// Писк у самого образца и на краю дальности (секунды между писками).
	UPROPERTY(config, EditAnywhere, Category = "Детектор", meta = (ClampMin = "0.05", DisplayPriority = "2",
		DisplayName = "Пауза между писками вплотную (секунды)"))
	float BeepIntervalNear = 0.15f;

	UPROPERTY(config, EditAnywhere, Category = "Детектор", meta = (ClampMin = "0.05", DisplayPriority = "3",
		DisplayName = "Пауза между писками на краю дальности (секунды)"))
	float BeepIntervalFar = 1.6f;

	// Короткий звук писка. Временно — щелчок оружия без патронов из проекта; заменить здесь.
	UPROPERTY(config, EditAnywhere, Category = "Детектор", meta = (DisplayPriority = "4",
		DisplayName = "Звук писка детектора"))
	TSoftObjectPtr<USoundBase> BeepSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/Weapons/weapon_dry_fire.weapon_dry_fire")));

	UPROPERTY(config, EditAnywhere, Category = "Детектор", meta = (ClampMin = "0.0", DisplayPriority = "5",
		DisplayName = "Громкость писка"))
	float BeepVolume = 0.6f;

	// --- Картинки ---

	// Стекло маски поверх экрана (слегка запотевшее и поцарапанное). Пусто — игра сама берёт
	// /Game/UI/Chapter2/T_MaskGlass, если такой файл есть; нет и его — стекла не видно.
	UPROPERTY(config, EditAnywhere, Category = "Картинки", meta = (DisplayPriority = "1",
		DisplayName = "Стекло маски (картинка поверх экрана)"))
	TSoftObjectPtr<UTexture2D> MaskGlassTexture;

	// Фотография героя рядом с Верой из ноутбука. Пусто — /Game/UI/Chapter2/T_PhotoHeroVera.
	UPROPERTY(config, EditAnywhere, Category = "Картинки", meta = (DisplayPriority = "2",
		DisplayName = "Фотография героя и Веры"))
	TSoftObjectPtr<UTexture2D> PhotoTexture;

	// Размер фотографии на экране (в точках раскладки 1920x1080) и её центр в долях экрана.
	UPROPERTY(config, EditAnywhere, Category = "Картинки", meta = (DisplayPriority = "3",
		DisplayName = "Размер фотографии на экране"))
	FVector2D PhotoSize = FVector2D(640.0f, 427.0f);

	UPROPERTY(config, EditAnywhere, Category = "Картинки", meta = (DisplayPriority = "4",
		DisplayName = "Центр фотографии (доли экрана)"))
	FVector2D PhotoCenter = FVector2D(0.5f, 0.32f);

	// Значок слева от надписи «Воздух» [220]. Пусто — /Game/UI/Chapter2/T_IconAir; нет и его —
	// шкала без значка.
	UPROPERTY(config, EditAnywhere, Category = "Картинки", meta = (DisplayPriority = "5",
		DisplayName = "Значок шкалы воздуха"))
	TSoftObjectPtr<UTexture2D> AirIconTexture;

	// Значок слева от надписи «Детектор» [225]. Пусто — /Game/UI/Chapter2/T_IconDetector.
	UPROPERTY(config, EditAnywhere, Category = "Картинки", meta = (DisplayPriority = "6",
		DisplayName = "Значок детектора"))
	TSoftObjectPtr<UTexture2D> DetectorIconTexture;

	// Сторона значка у шкалы (в точках раскладки 1920x1080).
	UPROPERTY(config, EditAnywhere, Category = "Картинки", meta = (ClampMin = "8.0", DisplayPriority = "7",
		DisplayName = "Размер значка у шкалы"))
	float BarIconSize = 28.0f;

	// --- Экран конца главы ---

	UPROPERTY(config, EditAnywhere, Category = "Конец главы", meta = (ClampMin = "1.0", DisplayPriority = "1",
		DisplayName = "Сколько секунд виден экран конца главы"))
	float ChapterEndCardSeconds = 7.0f;

	// --- Шкалы на экране ---

	// Верх-центр экрана, ниже строки задания. Смещение — в точках раскладки 1920x1080.
	UPROPERTY(config, EditAnywhere, Category = "Шкалы", meta = (DisplayPriority = "1",
		DisplayName = "Якорь шкал (доли экрана)"))
	FVector2D BarsAnchor = FVector2D(0.5f, 0.0f);

	UPROPERTY(config, EditAnywhere, Category = "Шкалы", meta = (DisplayPriority = "2",
		DisplayName = "Смещение шкал от якоря"))
	FVector2D BarsOffset = FVector2D(0.0f, 170.0f);

	UPROPERTY(config, EditAnywhere, Category = "Шкалы", meta = (ClampMin = "60.0", DisplayPriority = "3",
		DisplayName = "Ширина шкалы"))
	float BarWidth = 320.0f;

	UPROPERTY(config, EditAnywhere, Category = "Шкалы", meta = (DisplayPriority = "4", DisplayName = "Цвет шкалы воздуха"))
	FLinearColor AirBarColor = FLinearColor(0.35f, 0.75f, 1.0f, 1.0f);

	UPROPERTY(config, EditAnywhere, Category = "Шкалы", meta = (DisplayPriority = "5", DisplayName = "Цвет шкалы детектора"))
	FLinearColor DetectorBarColor = FLinearColor(0.45f, 1.0f, 0.35f, 1.0f);

	// --- Надписи (дословно из РЕПЛИКИ.txt) ---

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[218] Кнопка у края отравленной зоны"))
	FText PutOnMaskText = NSLOCTEXT("Chapter2", "PutOnMask", "Надеть дыхательный аппарат");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[219] Кнопка снять аппарат вне зоны"))
	FText TakeOffMaskText = NSLOCTEXT("Chapter2", "TakeOffMask", "Снять дыхательный аппарат");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[220] Надпись у шкалы воздуха"))
	FText AirLabelText = NSLOCTEXT("Chapter2", "AirLabel", "Воздух");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[221] Вход в зону без аппарата"))
	FText NoMaskInZoneText = NSLOCTEXT("Chapter2", "NoMaskInZone", "Здесь нечем дышать! Наденьте дыхательный аппарат или уходите.");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[222] Воздух на исходе"))
	FText AirLowText = NSLOCTEXT("Chapter2", "AirLow", "Воздух на исходе — выбирайтесь из низины.");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[223] Воздух кончился"))
	FText AirEmptyText = NSLOCTEXT("Chapter2", "AirEmpty", "Воздух кончился!");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[224] Баллон заправлен в лагере"))
	FText AirRefilledText = NSLOCTEXT("Chapter2", "AirRefilled", "Баллон снова полон.");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[225] Надпись на детекторе"))
	FText DetectorLabelText = NSLOCTEXT("Chapter2", "DetectorLabel", "Детектор");

	UPROPERTY(config, EditAnywhere, Category = "Надписи", meta = (DisplayName = "[253] Экран конца главы"))
	FText ChapterEndText = NSLOCTEXT("Chapter2", "ChapterEnd", "Конец второй главы. Продолжение следует…");

	// Картинка: поле, если задано; иначе файл по ожидаемому пути, если он есть в проекте.
	// Нет ни того, ни другого — null (картинки нет, игра не падает).
	static UTexture2D* ResolveTexture(const TSoftObjectPtr<UTexture2D>& Field, const TCHAR* ExpectedPackage);

	static constexpr const TCHAR* ExpectedMaskGlassPath = TEXT("/Game/UI/Chapter2/T_MaskGlass");
	static constexpr const TCHAR* ExpectedPhotoPath = TEXT("/Game/UI/Chapter2/T_PhotoHeroVera");
	static constexpr const TCHAR* ExpectedAirIconPath = TEXT("/Game/UI/Chapter2/T_IconAir");
	static constexpr const TCHAR* ExpectedDetectorIconPath = TEXT("/Game/UI/Chapter2/T_IconDetector");
};
