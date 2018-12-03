package com.mapswithme.maps.purchase;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.SkuDetails;
import com.android.billingclient.api.SkuDetailsParams;

import java.util.Collections;
import java.util.List;

import static com.mapswithme.maps.purchase.PlayStoreBillingManager.LOGGER;
import static com.mapswithme.maps.purchase.PlayStoreBillingManager.TAG;

class QueryProductDetailsRequest extends PlayStoreBillingRequest<PlayStoreBillingCallback>
{
  @NonNull
  private final List<String> mProductIds;

  QueryProductDetailsRequest(@NonNull BillingClient client, @NonNull String productType,
                             @Nullable PlayStoreBillingCallback callback,
                             @NonNull List<String> productIds)
  {
    super(client, productType, callback);
    mProductIds = Collections.unmodifiableList(productIds);
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

    if (skuDetails == null || skuDetails.isEmpty() || hasIncorrectSkuDetails(skuDetails))
    {
      LOGGER.w(TAG, "Purchase details not found");
      if (getCallback() != null)
        getCallback().onPurchaseDetailsFailure();
      return;
    }

    LOGGER.i(TAG, "Purchase details obtained: " + skuDetails);
    if (getCallback() != null)
      getCallback().onPurchaseDetailsLoaded(skuDetails);
  }

  private boolean hasIncorrectSkuDetails(@NonNull List<SkuDetails> skuDetails)
  {
    for (SkuDetails each : skuDetails)
    {
      if (AdsRemovalPurchaseDialog.Period.getInstance(each.getSubscriptionPeriod()) == null)
        return true;
    }
    return false;
  }
}
