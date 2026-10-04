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
// Полноэкранный показ без награды и баннер (04.10.2026) — сверено тем же способом, javap
// по classes.jar того же артефакта 8.2.0 из кэша Gradle:
//   InterstitialAdLoader(Context) / loadAd(AdRequest, InterstitialAdLoadListener) / cancelLoading()
//   InterstitialAd (интерфейс): setAdEventListener(InterstitialAdEventListener), show(Activity)
//   InterstitialAdEventListener: onAdShown, onAdFailedToShow(AdError), onAdDismissed,
//                                onAdClicked, onAdImpression(ImpressionData)
//   InterstitialAdLoadListener: onAdLoaded(InterstitialAd), onAdFailedToLoad(AdRequestError)
//   BannerAdView(Context) — потомок android.widget.RelativeLayout (через yads.pn3 -> yads.o05);
//     setAdSize(BannerAdSize), setBannerAdEventListener(BannerAdEventListener),
//     loadAd(AdRequest), destroy(). Метода setAdUnitId у него НЕТ — идентификатор места
//     едет в AdRequest.Builder(String), как у ролика.
//   BannerAdSize: статические sticky(Context, int), inline(Context, int, int), fixed(Context, int, int)
//   BannerAdEventListener: onAdLoaded(), onAdFailedToLoad(AdRequestError), onAdClicked(),
//                          onImpression(ImpressionData) — именно onImpression, не onAdImpression.
// НЕ сверено байт-кодом (имена параметров в артефакте стёрты): единицы измерения чисел у
// BannerAdSize.inline — здесь принято «ширина и предельная высота в dp», как в документации.
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
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Gravity;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.FrameLayout;

import com.yandex.mobile.ads.banner.BannerAdEventListener;
import com.yandex.mobile.ads.banner.BannerAdSize;
import com.yandex.mobile.ads.banner.BannerAdView;
import com.yandex.mobile.ads.common.AdError;
import com.yandex.mobile.ads.common.AdRequest;
import com.yandex.mobile.ads.common.AdRequestError;
import com.yandex.mobile.ads.common.ImpressionData;
import com.yandex.mobile.ads.common.InitializationListener;
import com.yandex.mobile.ads.common.YandexAds;
import com.yandex.mobile.ads.interstitial.InterstitialAd;
import com.yandex.mobile.ads.interstitial.InterstitialAdEventListener;
import com.yandex.mobile.ads.interstitial.InterstitialAdLoadListener;
import com.yandex.mobile.ads.interstitial.InterstitialAdLoader;
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
	private static final int EVENT_BANNER_LOADED = 8;
	private static final int EVENT_BANNER_LOAD_FAILED = 9;

	// Ниже этой высоты баннеров не бывает (обычный баннер — 320x50 dp), поэтому предельная
	// высота, посчитанная от доли экрана, снизу подпирается этим числом.
	private static final int BANNER_MIN_HEIGHT_DP = 50;

	private static final class Slot
	{
		RewardedAdLoader loader;
		RewardedAd ad;
		boolean loading;
	}

	private static final class InterstitialSlot
	{
		InterstitialAdLoader loader;
		InterstitialAd ad;
		boolean loading;
	}

	private static volatile Activity activity = null;
	private static boolean initialized = false;
	private static boolean initializing = false;
	private static final HashMap<String, Slot> slots = new HashMap<String, Slot>();
	private static final HashMap<String, InterstitialSlot> interstitialSlots = new HashMap<String, InterstitialSlot>();

	// Баннер на экране один. Трогается только из потока интерфейса Android.
	private static BannerAdView bannerView = null;

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

	// --- Полноэкранный показ без награды (межстраничный). Устроен как ролик, только без
	// события награды; события уходят те же, C++ различает форматы по идентификатору места. ---

	public static void loadInterstitial(final String adUnitId)
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
				InterstitialSlot slot = interstitialSlots.get(adUnitId);
				if (slot == null)
				{
					slot = new InterstitialSlot();
					slot.loader = new InterstitialAdLoader(gameActivity);
					interstitialSlots.put(adUnitId, slot);
				}

				if (slot.loading || slot.ad != null)
				{
					return;
				}

				slot.loading = true;
				final InterstitialSlot loadingSlot = slot;
				final AdRequest request = new AdRequest.Builder(adUnitId).build();
				slot.loader.loadAd(request, new InterstitialAdLoadListener()
				{
					@Override
					public void onAdLoaded(final InterstitialAd interstitialAd)
					{
						loadingSlot.loading = false;
						loadingSlot.ad = interstitialAd;
						nativeOnAdEvent(adUnitId, EVENT_LOADED, "");
					}

					@Override
					public void onAdFailedToLoad(final AdRequestError error)
					{
						loadingSlot.loading = false;
						final String detail = (error != null)
							? (error.getCode() + ": " + error.getDescription())
							: "unknown";
						Log.w(TAG, "interstitial load failed for " + adUnitId + " - " + detail);
						nativeOnAdEvent(adUnitId, EVENT_LOAD_FAILED, detail);
					}
				});
			}
		});
	}

	public static void showInterstitial(final String adUnitId)
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
				final InterstitialSlot slot = interstitialSlots.get(adUnitId);
				if (slot == null || slot.ad == null)
				{
					nativeOnAdEvent(adUnitId, EVENT_NO_AD_TO_SHOW, "no preloaded ad");
					return;
				}

				final InterstitialAd shownAd = slot.ad;
				slot.ad = null;

				shownAd.setAdEventListener(new InterstitialAdEventListener()
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
						Log.w(TAG, "interstitial show failed for " + adUnitId + " - " + detail);
						releaseInterstitial(shownAd);
						nativeOnAdEvent(adUnitId, EVENT_SHOW_FAILED, detail);
					}

					@Override
					public void onAdDismissed()
					{
						releaseInterstitial(shownAd);
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
				});

				shownAd.show(gameActivity);
			}
		});
	}

	private static void releaseInterstitial(final InterstitialAd ad)
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

	// --- Баннер: родной вид Android поверх игры, прижатый к верху или низу экрана. Высоту
	// выбирает SDK сам, но не больше maxHeightFraction высоты экрана (и не меньше 50 dp —
	// ниже баннеров не бывает). Пока баннер не загрузился, вид пуст и ничего не закрывает. ---

	public static void showBanner(final String adUnitId, final boolean atTop, final float maxHeightFraction)
	{
		final Activity gameActivity = activity;
		if (gameActivity == null)
		{
			nativeOnAdEvent(adUnitId, EVENT_BANNER_LOAD_FAILED, "no activity");
			return;
		}

		gameActivity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				removeBannerOnUiThread();

				final DisplayMetrics metrics = gameActivity.getResources().getDisplayMetrics();
				final int widthDp = Math.round(metrics.widthPixels / metrics.density);
				final int maxHeightDp = Math.max(BANNER_MIN_HEIGHT_DP,
					Math.round(metrics.heightPixels * maxHeightFraction / metrics.density));

				final BannerAdView view = new BannerAdView(gameActivity);
				view.setAdSize(BannerAdSize.inline(gameActivity, widthDp, maxHeightDp));
				view.setBannerAdEventListener(new BannerAdEventListener()
				{
					@Override
					public void onAdLoaded()
					{
						// Баннер успели убрать, пока он грузился, — молчим.
						if (bannerView == view)
						{
							nativeOnAdEvent(adUnitId, EVENT_BANNER_LOADED, "");
						}
					}

					@Override
					public void onAdFailedToLoad(final AdRequestError error)
					{
						final String detail = (error != null)
							? (error.getCode() + ": " + error.getDescription())
							: "unknown";
						Log.w(TAG, "banner load failed for " + adUnitId + " - " + detail);
						if (bannerView == view)
						{
							nativeOnAdEvent(adUnitId, EVENT_BANNER_LOAD_FAILED, detail);
						}
					}

					@Override
					public void onAdClicked()
					{
					}

					@Override
					public void onImpression(final ImpressionData impressionData)
					{
					}
				});

				final FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
					ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT,
					(atTop ? Gravity.TOP : Gravity.BOTTOM) | Gravity.CENTER_HORIZONTAL);
				gameActivity.addContentView(view, params);
				bannerView = view;

				view.loadAd(new AdRequest.Builder(adUnitId).build());
			}
		});
	}

	public static void hideBanner()
	{
		final Activity gameActivity = activity;
		if (gameActivity == null)
		{
			return;
		}

		gameActivity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				removeBannerOnUiThread();
			}
		});
	}

	private static void removeBannerOnUiThread()
	{
		final BannerAdView view = bannerView;
		if (view == null)
		{
			return;
		}
		bannerView = null;

		view.setBannerAdEventListener(null);
		final ViewParent parent = view.getParent();
		if (parent instanceof ViewGroup)
		{
			((ViewGroup) parent).removeView(view);
		}
		view.destroy();
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
