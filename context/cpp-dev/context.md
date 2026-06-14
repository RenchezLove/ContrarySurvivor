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
- Текст: хелпер `DrawShadowedText(text,color,x,y,font,scale)` = FCanvasTextItem с EnableShadow + bOutlined/OutlineColor + Scale (подтв. CanvasItem.h UE5.5). Растровый шрифт `GEngine->GetMediumFont()`; scale>1 слегка мылит — ок.
- #18 читаемость магазина/инвентаря (коммит 5252c12): подписи плиток/кнопок (`DrawInvBox`) и заголовки/деньги через `DrawShadowedText`; деньги/цены золотом на плашке (`DrawLabelWithPlate` = тёмный DrawRect под текстом + shadowed); рамки панелей (`DrawRectOutline` = 4 DrawLine). Параметры в `UPROPERTY(EditAnywhere) Category="HUD|Readability"` (scale заголовков/денег/слайдера, цвета, толщина рамки) — Ринат твикает без пересборки. Слайдер: панель PH 260, заголовки КУПИТЬ/ПРОДАТЬ, Итого/Выручка. Хитбоксы (Inv/Shop/DialogHitRegions) геометрически НЕ менялись — клики целы.
- Маркеры интерактивных NPC (`DrawInteractiveNPCMarkers`): перебор актёров с `IInteractableNPCInterface`; в кадре — зелёный ромб+подпись, за кадром — краевая стрелка (зеркалит проекцию при bBehind). Отличается от ретикла врага цветом и формой. DRAFT-perf: перебор всех актёров (для MVP ок).
- Интерфейс: `Source/ContrarySurvivor/Actors/InteractableNPCInterface.h` (нативный C++, плейс для старосты Фазы 5).

## Допущения / DRAFT
- Размеры/позиции плейсхолдера торговца, цвет, размеры маркеров — DRAFT-тюнинг.
- Перебор всех актёров в DrawHUD под маркеры NPC — приемлемо для MVP, позже реестр/тег.
- Финальный скин торговца — за modeler/operator; коллизия тела намеренно выключена.

## AI-погоня (`AEnemyAIController`, общий; волк/бандит наследуют — оба ACharacter с CharacterMovement)
- Idle->Chase->Attack, восприятие dist+LineOfSightTo. Атака через TakeDamage по кулдауну.
- Chase: сначала nav (`MoveToActor`, переотдача по RepathInterval/при не-Moving). FALLBACK прямой ход (коммит 224fbc9):
  условие `bUseDirect` = `LastMoveResult==Failed` ИЛИ `!selfNav` ИЛИ `!targetNav` ИЛИ `bNotConverging`
  (не сближается StuckConvergeTime сек, прогресс = убывание dist > ChaseConvergeEpsilon).
  В direct: StopMovement (если Moving) + `Self->AddMovementInput((PlayerLoc-PawnLoc).GetSafeNormal2D(),1)` каждый тик +
  периодическая проба MoveToActor (восстановление на nav, когда навмеш доступен — деревня).
  На входе в Chase LastMoveResult=RequestSuccessful (nav честный первый шанс).
- QA-лог (дроссель ChaseLogInterval): `QA: <pawn> chase mode=<nav|direct> dist=<..> (selfNav/targetNav/moveResult)`.
- API сверен с 5.5: APawn::AddMovementInput, FVector::GetSafeNormal2D, UCharacterMovementComponent — все есть.

## Последнее
Ветка `feature/phase5-quests`, коммит `5252c12` (#18 читаемость HUD торговца+инвентаря). Сборка ContrarySurvivorEditor Win64
Development прошла (6 action, линк base DLL), редактор был закрыт; базовый `Binaries/Win64/UnrealEditor-ContrarySurvivor.dll`
00:20→00:51, 1130496→1135104 б (НЕ -NNNN патч). Merge/push НЕ делал. Числа панелей/scale — DRAFT-дефолты, ждут оценки Рината.
