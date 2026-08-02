// Copyright ContrarySurvivor. Java-прослойка к Yandex Mobile Ads SDK (вознаграждаемые ролики).
//
// Классы и подписи сверены по байт-коду артефакта com.yandex.android:mobileads:8.2.0
// (javap по распакованному classes.jar), а не по документации:
//   RewardedAdLoader(Context) / loadAd(AdRequest, RewardedAdLoadListener) / cancelLoading()
//   RewardedAd: setAdEventListener(RewardedAdEventListener), show(Activity)
//   RewardedAdEventListener: onAdShown, onAdFailedToShow(AdError), onAdDismissed,
//                            onAdClicked, onAdImpression(ImpressionData), onRewarded(Reward)
//   RewardedAdLoadListener: onAdLoaded(RewardedAd), onAdFailedToLoad(AdRequestError)
//   YandexAds.initialize(Context, InitializationListener) — в 8.x класс называется YandexAds,
//   классов MobileAds и AdRequestConfiguration в этой линейке НЕТ.
//
// Игровой логики здесь нет: класс только поднимает SDK, заказывает ролик, показывает его
// и пересылает голые события в C++. Решения принимает UYandexAdService.
//
// Все обращения к SDK идут в потоке интерфейса Android (runOnUiThread), поэтому карта
// роликов правится ровно одним потоком и синхронизация ей не нужна.

package com.contrarysurvivor.ads;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import com.yandex.mobile.ads.common.AdError;
import com.yandex.mobile.ads.common.AdRequest;
import com.yandex.mobile.ads.common.AdRequestError;
import com.yandex.mobile.ads.common.ImpressionData;
import com.yandex.mobile.ads.common.InitializationListener;
import com.yandex.mobile.ads.common.YandexAds;
import com.yandex.mobile.ads.rewarded.Reward;
import com.yandex.mobile.ads.rewarded.RewardedAd;
import com.yandex.mobile.ads.rewarded.RewardedAdEventListener;
import com.yandex.mobile.ads.rewarded.RewardedAdLoadListener;
import com.yandex.mobile.ads.rewarded.RewardedAdLoader;

import java.util.HashMap;

public class YandexAdBridge
{
	private static final String TAG = "YandexAdBridge";

	// Значения обязаны совпадать с EYandexAdEvent в YandexAdsBridge.h.
	private static final int EVENT_INITIALIZED = 0;
	private static final int EVENT_LOADED = 1;
	private static final int EVENT_LOAD_FAILED = 2;
	private static final int EVENT_SHOWN = 3;
	private static final int EVENT_SHOW_FAILED = 4;
	private static final int EVENT_DISMISSED = 5;
	private static final int EVENT_REWARDED = 6;
	private static final int EVENT_NO_AD_TO_SHOW = 7;

	private static final class Slot
	{
		RewardedAdLoader loader;
		RewardedAd ad;
		boolean loading;
	}

	private static volatile Activity activity = null;
	private static boolean initialized = false;
	private static boolean initializing = false;
	private static final HashMap<String, Slot> slots = new HashMap<String, Slot>();

	private static native void nativeOnAdEvent(String adUnitId, int event, String detail);

	public static void initialize(final Activity gameActivity, final boolean userConsent, final boolean enableLogging)
	{
		if (gameActivity == null)
		{
			Log.e(TAG, "initialize: game activity is null");
			return;
		}

		activity = gameActivity;
		gameActivity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				if (initialized)
				{
					nativeOnAdEvent("", EVENT_INITIALIZED, YandexAds.getLibraryVersion());
					return;
				}
				if (initializing)
				{
					return;
				}
				initializing = true;

				// Согласия выставляются ДО инициализации — так это сделано в примере Яндекса.
				YandexAds.setUserConsent(userConsent);
				if (enableLogging)
				{
					YandexAds.enableLogging(true);
				}

				YandexAds.initialize(gameActivity.getApplicationContext(), new InitializationListener()
				{
					@Override
					public void onInitializationCompleted()
					{
						initialized = true;
						initializing = false;
						Log.d(TAG, "Yandex Mobile Ads SDK initialized, version " + YandexAds.getLibraryVersion());
						nativeOnAdEvent("", EVENT_INITIALIZED, YandexAds.getLibraryVersion());
					}
				});
			}
		});
	}

	public static void load(final String adUnitId)
	{
		final Activity gameActivity = activity;
		if (gameActivity == null)
		{
			nativeOnAdEvent(adUnitId, EVENT_LOAD_FAILED, "no activity");
			return;
		}

		gameActivity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				Slot slot = slots.get(adUnitId);
				if (slot == null)
				{
					slot = new Slot();
					slot.loader = new RewardedAdLoader(gameActivity);
					slots.put(adUnitId, slot);
				}

				// Ролик уже в пути или уже лежит готовым — второй заказ не нужен.
				if (slot.loading || slot.ad != null)
				{
					return;
				}

				slot.loading = true;
				final Slot loadingSlot = slot;
				final AdRequest request = new AdRequest.Builder(adUnitId).build();
				slot.loader.loadAd(request, new RewardedAdLoadListener()
				{
					@Override
					public void onAdLoaded(final RewardedAd rewardedAd)
					{
						loadingSlot.loading = false;
						loadingSlot.ad = rewardedAd;
						nativeOnAdEvent(adUnitId, EVENT_LOADED, "");
					}

					@Override
					public void onAdFailedToLoad(final AdRequestError error)
					{
						loadingSlot.loading = false;
						final String detail = (error != null)
							? (error.getCode() + ": " + error.getDescription())
							: "unknown";
						Log.w(TAG, "load failed for " + adUnitId + " - " + detail);
						nativeOnAdEvent(adUnitId, EVENT_LOAD_FAILED, detail);
					}
				});
			}
		});
	}

	public static void show(final String adUnitId)
	{
		final Activity gameActivity = activity;
		if (gameActivity == null)
		{
			nativeOnAdEvent(adUnitId, EVENT_NO_AD_TO_SHOW, "no activity");
			return;
		}

		gameActivity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				final Slot slot = slots.get(adUnitId);
				if (slot == null || slot.ad == null)
				{
					nativeOnAdEvent(adUnitId, EVENT_NO_AD_TO_SHOW, "no preloaded ad");
					return;
				}

				// Ролик одноразовый: забираем его из ячейки сразу, чтобы повторный показ
				// не поднял тот же объект второй раз.
				final RewardedAd shownAd = slot.ad;
				slot.ad = null;

				shownAd.setAdEventListener(new RewardedAdEventListener()
				{
					@Override
					public void onAdShown()
					{
						nativeOnAdEvent(adUnitId, EVENT_SHOWN, "");
					}

					@Override
					public void onAdFailedToShow(final AdError adError)
					{
						final String detail = (adError != null) ? adError.getDescription() : "unknown";
						Log.w(TAG, "show failed for " + adUnitId + " - " + detail);
						release(shownAd);
						nativeOnAdEvent(adUnitId, EVENT_SHOW_FAILED, detail);
					}

					@Override
					public void onAdDismissed()
					{
						release(shownAd);
						nativeOnAdEvent(adUnitId, EVENT_DISMISSED, "");
					}

					@Override
					public void onAdClicked()
					{
					}

					@Override
					public void onAdImpression(final ImpressionData impressionData)
					{
					}

					@Override
					public void onRewarded(final Reward reward)
					{
						final String detail = (reward != null)
							? (reward.getAmount() + " " + reward.getType())
							: "";
						nativeOnAdEvent(adUnitId, EVENT_REWARDED, detail);
					}
				});

				shownAd.show(gameActivity);
			}
		});
	}

	// Штатная отладочная панель SDK: показывает подхваченные адаптеры медиации с версиями
	// и отмечает проблемы подключения. Нужна, чтобы на устройстве доказать, что адаптер
	// VK реально попал в сборку. Вызывается консольной командой "ya.DebugPanel".
	public static void showDebugPanel()
	{
		final Activity gameActivity = activity;
		if (gameActivity == null)
		{
			Log.e(TAG, "showDebugPanel: game activity is null");
			return;
		}

		gameActivity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				YandexAds.showDebugPanel(gameActivity);
			}
		});
	}

	// Слушателя снимаем следующим шагом очереди сообщений, а не прямо внутри его же
	// обратного вызова: SDK в этот момент ещё разбирает событие.
	private static void release(final RewardedAd ad)
	{
		new Handler(Looper.getMainLooper()).post(new Runnable()
		{
			@Override
			public void run()
			{
				ad.setAdEventListener(null);
			}
		});
	}
}
