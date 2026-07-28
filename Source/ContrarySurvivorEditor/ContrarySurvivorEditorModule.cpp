// Fill out your copyright notice in the Description page of Project Settings.

#include "Modules/ModuleManager.h"

// Модуль без собственной логики запуска: содержимое — editor-коммандлеты
// (GenerateWbpCommandlet — окна интерфейса, PatchAnimBpCommandlet — граф анимации).
// Отдельной регистрации коммандлеты не требуют: движок находит их по имени класса
// (-run=GenerateWbp -> UGenerateWbpCommandlet, -run=PatchAnimBp -> UPatchAnimBpCommandlet).
IMPLEMENT_MODULE(FDefaultModuleImpl, ContrarySurvivorEditor);
