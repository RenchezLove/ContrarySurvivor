// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Analytics/DataConsentSettings.h"

const UDataConsentSettings* UDataConsentSettings::Get()
{
	// GetDefault читает уже разобранный конфиг класса, отдельной загрузки не требуется.
	return GetDefault<UDataConsentSettings>();
}
