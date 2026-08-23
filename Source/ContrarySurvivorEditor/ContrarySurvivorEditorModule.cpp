// Fill out your copyright notice in the Description page of Project Settings.

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

#include "AbandonedCarDetails.h"

// Содержимое модуля — editor-коммандлеты (GenerateWbpCommandlet — окна интерфейса,
// PatchAnimBpCommandlet — граф анимации; отдельной регистрации не требуют: движок находит
// их по имени класса, -run=GenerateWbp -> UGenerateWbpCommandlet) и кастомизации панели
// Details (Report1 п.9 — таблица деталей машины), которым регистрация как раз нужна.
class FContrarySurvivorEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FPropertyEditorModule& PropertyModule =
			FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		// Имя КЛАССА без префикса A; действует и на BP-потомков (BP_AbandonedCar).
		PropertyModule.RegisterCustomClassLayout(TEXT("AbandonedCar"),
			FOnGetDetailCustomizationInstance::CreateStatic(&FAbandonedCarDetails::MakeInstance));
	}

	virtual void ShutdownModule() override
	{
		if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
		{
			FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor")
				.UnregisterCustomClassLayout(TEXT("AbandonedCar"));
		}
	}
};

IMPLEMENT_MODULE(FContrarySurvivorEditorModule, ContrarySurvivorEditor);
