package com.mapswithme.maps.purchase;

import androidx.annotation.NonNull;

import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.Purchase;
import com.android.billingclient.api.SkuDetails;

import java.util.List;

public interface PlayStoreBillingCallback
{
  void onProductDetailsLoaded(@NonNull List<SkuDetails> details);
  void onPurchaseSuccessful(@NonNull List<Purchase> purchases);
  void onPurchaseFailure(@BillingClient.BillingResponse int error);
  void onProductDetailsFailure();
  void onStoreConnectionFailed();
  void onPurchasesLoaded(@NonNull List<Purchase> purchases);
  void onConsumptionSuccess();
  void onConsumptionFailure();
}
