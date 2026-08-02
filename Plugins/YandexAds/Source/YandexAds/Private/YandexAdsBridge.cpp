// Copyright ContrarySurvivor. Yandex Mobile Ads bridge module.

#include "YandexAdsBridge.h"

FOnYandexAdEvent& FYandexAdsBridge::OnAdEvent()
{
	static FOnYandexAdEvent Event;
	return Event;
}

#if PLATFORM_ANDROID

#include "Android/AndroidApplication.h"
#include "Android/AndroidJavaEnv.h"
#include "Async/Async.h"

namespace YandexAdsBridgeInternal
{
	// Путь к Java-прослойке. Должен совпадать с package+именем класса в YandexAdBridge.java
	// и с папкой назначения в YandexAds_UPL.xml (prebuildCopies).
	static const char* const BridgeClassName = "com/contrarysurvivor/ads/YandexAdBridge";

	// Ищем класс через загрузчик классов игровой активности: обычный FindClass у JNI видит
	// только системные классы, если зовётся не из Java-потока.
	static jclass GetBridgeClass()
	{
		static jclass BridgeClass = AndroidJavaEnv::FindJavaClassGlobalRef(BridgeClassName);
		return BridgeClass;
	}

	// Идентификаторы методов не кешируем: инициализация, заказ и показ ролика случаются
	// считанные разы за сессию, а кеш пришлось бы защищать от неудачного первого поиска.
	static bool GetStaticMethod(const char* Name, const char* Signature, JNIEnv*& OutEnv, jclass& OutClass, jmethodID& OutMethod)
	{
		OutEnv = AndroidJavaEnv::GetJavaEnv();
		OutClass = GetBridgeClass();
		if (!OutEnv || !OutClass)
		{
			return false;
		}

		OutMethod = OutEnv->GetStaticMethodID(OutClass, Name, Signature);
		AndroidJavaEnv::CheckJavaException();
		return OutMethod != nullptr;
	}
}

bool FYandexAdsBridge::IsSupported()
{
	return true;
}

void FYandexAdsBridge::Initialize(bool bUserConsent, bool bEnableSdkLogging)
{
	JNIEnv* Env = nullptr;
	jclass Class = nullptr;
	jmethodID Method = nullptr;
	if (!YandexAdsBridgeInternal::GetStaticMethod("initialize", "(Landroid/app/Activity;ZZ)V", Env, Class, Method))
	{
		return;
	}

	Env->CallStaticVoidMethod(Class, Method, FAndroidApplication::GetGameActivityThis(),
		static_cast<jboolean>(bUserConsent), static_cast<jboolean>(bEnableSdkLogging));
	AndroidJavaEnv::CheckJavaException();
}

void FYandexAdsBridge::LoadRewarded(const FString& AdUnitId)
{
	JNIEnv* Env = nullptr;
	jclass Class = nullptr;
	jmethodID Method = nullptr;
	if (!YandexAdsBridgeInternal::GetStaticMethod("load", "(Ljava/lang/String;)V", Env, Class, Method))
	{
		return;
	}

	auto JavaAdUnitId = FJavaHelper::ToJavaString(Env, AdUnitId);
	Env->CallStaticVoidMethod(Class, Method, *JavaAdUnitId);
	AndroidJavaEnv::CheckJavaException();
}

void FYandexAdsBridge::ShowRewarded(const FString& AdUnitId)
{
	JNIEnv* Env = nullptr;
	jclass Class = nullptr;
	jmethodID Method = nullptr;
	if (!YandexAdsBridgeInternal::GetStaticMethod("show", "(Ljava/lang/String;)V", Env, Class, Method))
	{
		return;
	}

	auto JavaAdUnitId = FJavaHelper::ToJavaString(Env, AdUnitId);
	Env->CallStaticVoidMethod(Class, Method, *JavaAdUnitId);
	AndroidJavaEnv::CheckJavaException();
}

// Обратный вызов из Java. Приходит в потоке интерфейса Android, поэтому событие
// перекладываем в игровой поток — подписчики живут там.
JNI_METHOD void Java_com_contrarysurvivor_ads_YandexAdBridge_nativeOnAdEvent(
	JNIEnv* Env, jclass /*Clazz*/, jstring AdUnitId, jint EventId, jstring Detail)
{
	if (EventId < static_cast<jint>(EYandexAdEvent::Initialized) || EventId > static_cast<jint>(EYandexAdEvent::NoAdToShow))
	{
		return;
	}

	const FString UnitId = FJavaHelper::FStringFromParam(Env, AdUnitId);
	const FString DetailText = FJavaHelper::FStringFromParam(Env, Detail);
	const EYandexAdEvent Event = static_cast<EYandexAdEvent>(EventId);

	AsyncTask(ENamedThreads::GameThread, [UnitId, Event, DetailText]()
	{
		FYandexAdsBridge::OnAdEvent().Broadcast(UnitId, Event, DetailText);
	});
}

#else // PLATFORM_ANDROID

bool FYandexAdsBridge::IsSupported()
{
	return false;
}

void FYandexAdsBridge::Initialize(bool /*bUserConsent*/, bool /*bEnableSdkLogging*/)
{
}

void FYandexAdsBridge::LoadRewarded(const FString& /*AdUnitId*/)
{
}

void FYandexAdsBridge::ShowRewarded(const FString& /*AdUnitId*/)
{
}

#endif // PLATFORM_ANDROID
