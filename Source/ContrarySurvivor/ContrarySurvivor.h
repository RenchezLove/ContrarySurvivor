// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Категория логов QA-харнесса (Фаза 4 раунд 2). Все ключевые игровые события
// (подбор/броня/торговля/смена цели/еда-вода/реген/выброс) логируются через неё
// с префиксом "QA:" — чтобы автотестер (Computer Use) верифицировал по логу.
CONTRARYSURVIVOR_API DECLARE_LOG_CATEGORY_EXTERN(LogQA, Log, All);

