package com.mapswithme.maps.purchase;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.SkuDetails;
import com.android.billingclient.api.SkuDetailsParams;
import com.mapswithme.util.CrashlyticsUtils;

import java.util.Collections;
import java.util.List;

import static com.mapswithme.maps.purchase.PlayStoreBillingManager.LOGGER;
import static com.mapswithme.maps.purchase.PlayStoreBillingManager.TAG;

class QueryProductDetailsRequest extends PlayStoreBillingRequest<PlayStoreBillingCallback>
{
  @NonNull
  private final List<String> mProductIds;
  @NonNull
  private final SkuDetailsValidationStrategy mSkuDetailsValidationStrategy;

  QueryProductDetailsRequest(@NonNull BillingClient client, @NonNull String productType,
                             @Nullable PlayStoreBillingCallback callback,
                             @NonNull List<String> productIds,
                             @NonNull SkuDetailsValidationStrategy strategy)
  {
    super(client, productType, callback);
    mProductIds = Collections.unmodifiableList(productIds);
    mSkuDetailsValidationStrategy = strategy;
  }

  @Override
  public void execute()
  {
    SkuDetailsParams.Builder builder = SkuDetailsParams.newBuilder()
                                                       .setSkusList(mProductIds)
                                                       .setType(getProductType());
    getClient().querySkuDetailsAsync(builder.build(), this::onSkuDetailsResponseInternal);
  }

  private void onSkuDetailsResponseInternal(@BillingClient.BillingResponse int responseCode,
                                            @Nullable List<SkuDetails> skuDetails)
  {
    LOGGER.i(TAG, "Purchase details response code: " + responseCode
                  + ". Type: " + getProductType());
    if (responseCode != BillingClient.BillingResponse.OK)
    {
      LOGGER.w(TAG, "Unsuccessful request");
      if (getCallback() != null)
        getCallback().onPurchaseDetailsFailure();
      return;
    }

    if (getCallback() == null)
      return;

    boolean isSkuValid = mSkuDetailsValidationStrategy.isValid(skuDetails);

    if (isSkuValid)
      getCallback().onPurchaseDetailsLoaded(skuDetails);
    else
      getCallback().onPurchaseDetailsFailure();
  }
}
