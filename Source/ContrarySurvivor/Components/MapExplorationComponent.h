// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ContrarySurvivor/UI/PdaMapFog.h"
#include "MapExplorationComponent.generated.h"

class UContrarySaveGame;

/**
 * Память героя о том, какую часть карты он уже видел (слова Рината 10.10.2026: «изначально на
 * карте ничего не должно быть видно, кроме деревни… карта появляется в каком-то радиусе от него»).
 *
 * Герой раз в короткий промежуток открывает круг вокруг себя; открытое обратно не закрывается.
 * С самого начала открыта деревня. Сама сетка и её правила — FPdaMapFog; размеры, радиусы и
 * период — UContraryPdaSettings (Project Settings → «Contrary Survivor: КПК и карта»).
 *
 * Окно КПК состояние только читает: по счётчику изменений (GetRevision) оно понимает, что
 * черноту и подписи пора перерисовать, и не делает этого каждый кадр.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CONTRARYSURVIVOR_API UMapExplorationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMapExplorationComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Состояние начала игры: открыта только деревня.
	void ResetToVillage();

	// Открыть круг вокруг точки мира. true — открылось что-то новое.
	bool RevealAroundWorld(const FVector2D& WorldLocation, float RadiusWorld);

	// Открыть круг вокруг героя радиусом из настроек (то же делает сам компонент по таймеру).
	bool RevealAroundOwner();

	// Меню отладки: открыть всю карту.
	void RevealAll();

	// Открыта ли клетка карты под точкой мира.
	bool IsWorldLocationRevealed(const FVector2D& WorldLocation) const;

	// Центр деревни: зона деревни на уровне, а без неё — точка из настроек.
	FVector2D FindVillageCenter() const;

	const FPdaMapFog& GetFog() const { return Fog; }

	// Растёт при каждом изменении открытого (и при сбросе, и при загрузке).
	int32 GetRevision() const { return Revision; }

	// Сохранение. Запись без этих данных (сделана раньше) читается как «деревня и место, где
	// герой стоит сейчас».
	void WriteToSave(UContrarySaveGame* Save) const;
	void RestoreFromSave(const UContrarySaveGame* Save);

	// --- Район поиска на карте (слова Рината 11.10.2026: староста говорит «Я отметил на твоей
	// карте», после разговора мигает кнопка смартфона, в смартфоне — вкладка «Карта», на карте
	// зелёный круг района южного логова с подписью; «по приходу игрока в логово и (или) по
	// прошествии 3 минут пропадает и отметка района поиска, и подпись»). Здесь только состояние:
	// круг и подпись лежат в файле окна смартфона, мигание — в файлах окон. ---

	// Отметить район южного логова волков. ЭТУ функцию зовёт код разговора со старостой после
	// реплики про логово. Логово ищется на уровне по метке из настроек
	// (UContraryPdaSettings::SouthWolfDenMarkerTag); false — такого логова на уровне нет.
	UFUNCTION(BlueprintCallable, Category = "Map")
	bool MarkSouthWolfDenArea();

	// Отметить район вокруг точки мира на столько секунд игрового времени.
	void MarkSearchArea(const FVector2D& WorldCenter, float Seconds);
	void ClearSearchArea();

	bool IsSearchAreaMarked() const { return SearchAreaRemaining > 0.0f; }
	FVector2D GetSearchAreaCenter() const { return SearchAreaCenter; }
	float GetSearchAreaRemaining() const { return SearchAreaRemaining; }

	// Мигать ли кнопке смартфона на экране и вкладке «Карта» в окне смартфона.
	bool IsPhoneAttentionOn() const { return IsSearchAreaMarked() && bSearchAreaPhoneUnseen; }
	bool IsMapTabAttentionOn() const { return IsSearchAreaMarked() && bSearchAreaMapUnseen; }

	// Игрок открыл смартфон — кнопка на экране больше не мигает.
	void NotePhoneOpened();
	// Игрок увидел вкладку «Карта» — не мигает ни вкладка, ни кнопка.
	void NoteMapTabShown();

	// Шаг отсчёта и проверка «герой пришёл». В игре зовёт сам компонент; автопроверки — напрямую.
	void AdvanceSearchArea(float DeltaSeconds);

private:
	// Сетка нужного размера есть? Нет — заводим и открываем деревню.
	void EnsureFog();

	FPdaMapFog Fog;
	int32 Revision = 0;

	// Район поиска: сколько секунд осталось (0 — отметки нет), середина в мире, что ещё мигает.
	float SearchAreaRemaining = 0.0f;
	FVector2D SearchAreaCenter = FVector2D::ZeroVector;
	bool bSearchAreaPhoneUnseen = false;
	bool bSearchAreaMapUnseen = false;
};
