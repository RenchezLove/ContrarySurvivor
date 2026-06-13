# cpp-dev — состояние

Движок: UE 5.5. Установлен в `E:/UnrealEngine/UE_5.5`. Сборка редактора:
`E:/UnrealEngine/UE_5.5/Engine/Build/BatchFiles/Build.bat ContrarySurvivorEditor Win64 Development -Project=<.uproject> -WaitMutex -FromMsBuild`.
Модуль: `Source/ContrarySurvivor` (Public/Private + include по `ContrarySurvivor/...` от Source root).

## Боевая цель / лок (вариант A, решение Рината — Фаза 4)
`AContrarySurvivorPlayerController`:
- `CurrentTarget` + `bManualLock`.
- Авто (bManualLock=false): `UpdateAutoTarget()` каждый тик ставит `FindNearestLivingTarget()` — динамический переброс на ближайшую.
- Ручной фокус: тап по врагу (`TrySelectTarget`) -> bManualLock=true, держится до смерти/смены/тапа по пустоте.
- Смерть цели или невалидность -> сброс bManualLock -> авто. Тап по пустому -> сброс bManualLock + авто-ближайшая.
- Цель определяется ТИП-АГНОСТИЧНО: Pawn со `UStatsComponent`, не игрок, жив. Торговец НЕ Pawn и без Stats — не попадает.

## Торговец (Фаза 4)
- `ATraderNPC` (AActor, не Pawn) + `UTraderSpawnSubsystem` (WorldSubsystem, спавн по кольцу направлений на навмеше, ~600u от игрока). По логам спавнится надёжно (navmesh=yes).
- Невидимость была: серый стаб-цилиндр движка, тонул в деревне. Фикс: тело крупнее/выше (scale 1.1/1.1/2.6, relZ 40) + шар-маяк сверху; оба залиты dynamic material из `/Engine/BasicShapes/BasicShapeMaterial` (вектор-параметр `Color`, подтверждён по ассету) ярко-мадженты. MID создаётся в `BeginPlay`.
- Реализует `IInteractableNPCInterface` (метка "Trader", ZOffset 320).

## HUD (`AContrarySurvivorHUD`, immediate-mode, без UMG)
- `DrawHUD`: хелсбары врагов (Pawn+Stats), маркер-ретикл текущей цели (жёлтые угловые скобки), статы игрока, инвентарь/магазин (модальные), подсказка E.
- Маркеры интерактивных NPC (`DrawInteractiveNPCMarkers`): перебор актёров с `IInteractableNPCInterface`; в кадре — зелёный ромб+подпись, за кадром — краевая стрелка (зеркалит проекцию при bBehind). Отличается от ретикла врага цветом и формой. DRAFT-perf: перебор всех актёров (для MVP ок).
- Интерфейс: `Source/ContrarySurvivor/Actors/InteractableNPCInterface.h` (нативный C++, плейс для старосты Фазы 5).

## Допущения / DRAFT
- Размеры/позиции плейсхолдера торговца, цвет, размеры маркеров — DRAFT-тюнинг.
- Перебор всех актёров в DrawHUD под маркеры NPC — приемлемо для MVP, позже реестр/тег.
- Финальный скин торговца — за modeler/operator; коллизия тела намеренно выключена.

## Последнее
Ветка `feature/phase4-inventory`, коммит `d189b84`. Сборка exit 0, лог `/e/game-dev-team/logs/phase4-cpp-lockA-build.log`. Merge/push НЕ делал.
