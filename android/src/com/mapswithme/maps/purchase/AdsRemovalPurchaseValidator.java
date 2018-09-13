package com.mapswithme.maps.purchase;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import com.mapswithme.maps.Framework;
import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.LoggerFactory;

class AdsRemovalPurchaseValidator implements PurchaseValidator<AdsRemovalValidationCallback>,
                                             Framework.PurchaseValidationListener
{
  private static final Logger LOGGER = LoggerFactory.INSTANCE.getLogger(LoggerFactory.Type.BILLING);
  private static final String TAG = AdsRemovalPurchaseValidator.class.getSimpleName();
  @Nullable
  private AdsRemovalValidationCallback mCallback;

  @Override
  public void initialize()
  {
    Framework.nativeSetPurchaseValidationListener(this);
    LOGGER.i(TAG, "Initializing purchase validator...");
  }

  @Override
  public void destroy()
  {
    Framework.nativeSetPurchaseValidationListener(null);
    LOGGER.i(TAG, "Destroying purchase validator...");
  }

  @Override
  public void validate(@NonNull String purchaseToken)
  {
    //TODO (@alexzatsepin): Paste serverId and vendorId.
    Framework.nativeValidatePurchase("", "", purchaseToken);
  }

  @Override
  public boolean hasActivePurchase()
  {
    return Framework.nativeHasActiveRemoveAdsSubscription();
  }

  @Override
  public void addCallback(@NonNull AdsRemovalValidationCallback callback)
  {
    mCallback = callback;
  }

  @Override
  public void removeCallback()
  {
    mCallback = null;
  }

  @Override
  public void onValidatePurchase(@Framework.PurchaseValidationCode int code,
                                 @NonNull String serverId, @NonNull String vendorId,
                                 @NonNull String purchaseToken)
  {
    LOGGER.i(TAG, "Validation code: " + code);
    if (mCallback != null)
      mCallback.onValidate(AdsRemovalValidationStatus.values()[code]);
  }
}
