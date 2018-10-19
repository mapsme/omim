package com.mapswithme.maps.purchase;

import android.content.Context;
import android.content.DialogInterface;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CompoundButton;
import android.widget.TextView;

import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.SkuDetails;
import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmDialogFragment;
import com.mapswithme.maps.dialog.AlertDialogCallback;
import com.mapswithme.maps.dialog.Detachable;
import com.mapswithme.util.Graphics;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.Utils;
import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.LoggerFactory;
import com.mapswithme.util.statistics.Statistics;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class AdsRemovalPurchaseDialog extends BaseMwmDialogFragment implements AlertDialogCallback
{
  private final static Logger LOGGER = LoggerFactory.INSTANCE.getLogger(LoggerFactory.Type.BILLING);
  private final static String TAG = AdsRemovalPurchaseDialog.class.getSimpleName();
  private final static String EXTRA_CURRENT_STATE = "extra_current_state";
  private final static String EXTRA_PRODUCT_DETAILS = "extra_product_details";
  private final static String EXTRA_ACTIVATION_RESULT = "extra_activation_result";
  private final static int WEEKS_IN_YEAR = 52;
  private final static int MONTHS_IN_YEAR = 12;
  final static int REQ_CODE_PRODUCT_DETAILS_FAILURE = 1;
  final static int REQ_CODE_PAYMENT_FAILURE = 2;
  final static int REQ_CODE_VALIDATION_SERVER_ERROR = 3;

  private boolean mActivationResult;
  @Nullable
  private ProductDetails[] mProductDetails;
  @NonNull
  private AdsRemovalPaymentState mState = AdsRemovalPaymentState.NONE;
  @Nullable
  private AdsRemovalPurchaseControllerProvider mControllerProvider;
  @NonNull
  private PurchaseCallback mPurchaseCallback = new PurchaseCallback();
  @NonNull
  private List<AdsRemovalActivationCallback> mActivationCallbacks = new ArrayList<>();
  @SuppressWarnings("NullableProblems")
  @NonNull
  private View mYearlyButton;
  @SuppressWarnings("NullableProblems")
  @NonNull
  private TextView mMonthlyButton;
  @SuppressWarnings("NullableProblems")
  @NonNull
  private TextView mWeeklyButton;
  @SuppressWarnings("NullableProblems")
  @NonNull
  private CompoundButton mOptionsButton;

  @Override
  public void onCreate(@Nullable Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    LOGGER.d(TAG, "onCreate savedInstanceState = " + savedInstanceState);
  }

  @Override
  public void onAttach(Context context)
  {
    super.onAttach(context);
    LOGGER.d(TAG, "onAttach");
    mControllerProvider = (AdsRemovalPurchaseControllerProvider) context;
    if (context instanceof AdsRemovalActivationCallback)
      mActivationCallbacks.add((AdsRemovalActivationCallback) context);
    if (getParentFragment() instanceof AdsRemovalActivationCallback)
      mActivationCallbacks.add((AdsRemovalActivationCallback) getParentFragment());
  }

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
                           @Nullable Bundle savedInstanceState)
  {
    LOGGER.d(TAG, "onCreateView savedInstanceState = " + savedInstanceState + "this " + this);
    View view = inflater.inflate(R.layout.fragment_ads_removal_purchase_dialog, container, false);
    View payButtonContainer = view.findViewById(R.id.pay_button_container);
    mYearlyButton = payButtonContainer.findViewById(R.id.yearly_button);
    mYearlyButton.setOnClickListener(v -> onYearlyProductClicked());
    mMonthlyButton = payButtonContainer.findViewById(R.id.monthly_button);
    mMonthlyButton.setOnClickListener(v -> onMonthlyProductClicked());
    mWeeklyButton = payButtonContainer.findViewById(R.id.weekly_button);
    mWeeklyButton.setOnClickListener(v -> onWeeklyProductClicked());
    mOptionsButton = payButtonContainer.findViewById(R.id.options);
    mOptionsButton.setCompoundDrawablesWithIntrinsicBounds(null, null, getOptionsToggle(), null);
    mOptionsButton.setOnCheckedChangeListener((buttonView, isChecked) -> onOptionsClicked());
    view.findViewById(R.id.explanation).setOnClickListener(v -> onExplanationClick());
    return view;
  }

  @Override
  public void onViewCreated(View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);
    LOGGER.d(TAG, "onViewCreated savedInstanceState = " + savedInstanceState);
    if (savedInstanceState != null)
    {
      AdsRemovalPaymentState savedState
          = AdsRemovalPaymentState.values()[savedInstanceState.getInt(EXTRA_CURRENT_STATE)];
      ProductDetails[] productDetails
          = (ProductDetails[]) savedInstanceState.getParcelableArray(EXTRA_PRODUCT_DETAILS);
      if (productDetails != null)
        mProductDetails = productDetails;
      mActivationResult = savedInstanceState.getBoolean(EXTRA_ACTIVATION_RESULT);

      activateState(savedState);
      return;
    }

    activateState(AdsRemovalPaymentState.LOADING);
  }

  @NonNull
  private Drawable getOptionsToggle()
  {
    return Graphics.tint(getContext(), mOptionsButton.isChecked() ? R.drawable.ic_expand_less
                                                                  : R.drawable.ic_expand_more,
                         android.R.attr.textColorSecondary);
  }

  @Override
  public void onStart()
  {
    super.onStart();
    getControllerOrThrow().addCallback(mPurchaseCallback);
    mPurchaseCallback.attach(this);
  }

  @Override
  public void onStop()
  {
    super.onStop();
    getControllerOrThrow().removeCallback();
    mPurchaseCallback.detach();
  }

  void queryPurchaseDetails()
  {
    getControllerOrThrow().queryPurchaseDetails();
  }

  void onYearlyProductClicked()
  {
    launchPurchaseFlowForPeriod(Period.P1Y);
  }

  void onMonthlyProductClicked()
  {
    launchPurchaseFlowForPeriod(Period.P1M);
  }

  void onWeeklyProductClicked()
  {
    launchPurchaseFlowForPeriod(Period.P1W);
  }

  void onOptionsClicked()
  {
    mOptionsButton.setCompoundDrawablesWithIntrinsicBounds(null, null, getOptionsToggle(), null);
    View payContainer = getViewOrThrow().findViewById(R.id.pay_button_container);
    UiUtils.showIf(mOptionsButton.isChecked(), payContainer, R.id.monthly_button,
                   R.id.weekly_button);
  }

  private void launchPurchaseFlowForPeriod(@NonNull Period period)
  {
    ProductDetails details = getProductDetailsForPeriod(period);
    getControllerOrThrow().launchPurchaseFlow(details.getProductId());
    Statistics.INSTANCE.trackPurchasePreviewSelect(details.getProductId());
    Statistics.INSTANCE.trackEvent(Statistics.EventName.INAPP_PURCHASE_PREVIEW_PAY);
  }

  void onExplanationClick()
  {
    activateState(AdsRemovalPaymentState.EXPLANATION);
  }

  private void activateState(@NonNull AdsRemovalPaymentState state)
  {
    if (state == mState)
      return;

    LOGGER.i(TAG, "Activate state: " + state);
    mState = state;
    mState.activate(this);
  }

  @NonNull
  private PurchaseController<AdsRemovalPurchaseCallback> getControllerOrThrow()
  {
    if (mControllerProvider == null)
      throw new IllegalStateException("Controller provider must be non-null at this point!");

    PurchaseController<AdsRemovalPurchaseCallback> controller
        = mControllerProvider.getAdsRemovalPurchaseController();
    if (controller == null)
      throw new IllegalStateException("Controller must be non-null at this point!");

    return controller;
  }

  @Override
  public void onSaveInstanceState(Bundle outState)
  {
    super.onSaveInstanceState(outState);
    LOGGER.d(TAG, "onSaveInstanceState");
    outState.putInt(EXTRA_CURRENT_STATE, mState.ordinal());
    outState.putParcelableArray(EXTRA_PRODUCT_DETAILS, mProductDetails);
    outState.putBoolean(EXTRA_ACTIVATION_RESULT, mActivationResult);
  }

  @Override
  public void onCancel(DialogInterface dialog)
  {
    super.onCancel(dialog);
    Statistics.INSTANCE.trackEvent(Statistics.EventName.INAPP_PURCHASE_PREVIEW_CANCEL);
  }

  @Override
  public void onDetach()
  {
    LOGGER.d(TAG, "onDetach");
    mControllerProvider = null;
    mActivationCallbacks.clear();
    super.onDetach();
  }

  void finishValidation()
  {
    if (mActivationResult)
    {
      for (AdsRemovalActivationCallback callback : mActivationCallbacks)
        callback.onAdsRemovalActivation();
    }

    dismissAllowingStateLoss();
  }

  void updatePaymentButtons()
  {
    updateYearlyButton();
    updateMonthlyButton();
    updateWeeklyButton();
  }

  private void updateYearlyButton()
  {
    ProductDetails details = getProductDetailsForPeriod(Period.P1Y);
    String price = Utils.formatCurrencyString(details.getPrice(), details.getCurrencyCode());
    TextView priceView = mYearlyButton.findViewById(R.id.price);
    priceView.setText(getString(R.string.paybtn_title, price));
    TextView savingView = mYearlyButton.findViewById(R.id.saving);
    String saving = Utils.formatCurrencyString(calculateYearlySaving(), details.getCurrencyCode());
    savingView.setText(getString(R.string.paybtn_subtitle, saving));
  }

  private void updateMonthlyButton()
  {
    ProductDetails details = getProductDetailsForPeriod(Period.P1M);
    String price = Utils.formatCurrencyString(details.getPrice(), details.getCurrencyCode());
    String saving = Utils.formatCurrencyString(calculateMonthlySaving(), details.getCurrencyCode());
    mMonthlyButton.setText(getString(R.string.options_dropdown_item1, price, saving));
  }

  private void updateWeeklyButton()
  {
    ProductDetails details = getProductDetailsForPeriod(Period.P1W);
    String price = Utils.formatCurrencyString(details.getPrice(), details.getCurrencyCode());
    mWeeklyButton.setText(getString(R.string.options_dropdown_item2, price));
  }

  @NonNull
  private ProductDetails getProductDetailsForPeriod(@NonNull Period period)
  {
    if (mProductDetails == null)
      throw new AssertionError("Product details must be exist at this moment!");
    return mProductDetails[period.ordinal()];
  }

  private float calculateYearlySaving()
  {
    float pricePerWeek = getProductDetailsForPeriod(Period.P1W).getPrice();
    float pricePerYear = getProductDetailsForPeriod(Period.P1Y).getPrice();
    return pricePerWeek * WEEKS_IN_YEAR - pricePerYear;
  }

  private float calculateMonthlySaving()
  {
    float pricePerWeek = getProductDetailsForPeriod(Period.P1W).getPrice();
    float pricePerMonth = getProductDetailsForPeriod(Period.P1M).getPrice();
    return pricePerWeek * WEEKS_IN_YEAR - pricePerMonth * MONTHS_IN_YEAR;
  }

  @Override
  public void onAlertDialogClick(int requestCode, int which)
  {
    handleErrorDialogEvent(requestCode);
  }

  @Override
  public void onAlertDialogCancel(int requestCode)
  {
    handleErrorDialogEvent(requestCode);
  }

  private void handleErrorDialogEvent(int requestCode)
  {
    switch (requestCode)
    {
      case REQ_CODE_PRODUCT_DETAILS_FAILURE:
      case REQ_CODE_VALIDATION_SERVER_ERROR:
        dismissAllowingStateLoss();
        break;
      case REQ_CODE_PAYMENT_FAILURE:
        activateState(AdsRemovalPaymentState.PRICE_SELECTION);
        break;
    }
  }

  private void handleProductDetails(@NonNull List<SkuDetails> details)
  {
    mProductDetails = new ProductDetails[Period.values().length];
    for (SkuDetails sku: details)
    {
      float price = sku.getPriceAmountMicros() / 1000000;
      String currencyCode = sku.getPriceCurrencyCode();
      Period period = Period.valueOf(sku.getSubscriptionPeriod());
      mProductDetails[period.ordinal()] = new ProductDetails(sku.getSku(), price, currencyCode);
    }
  }

  private void handleActivationResult(boolean result)
  {
    mActivationResult = result;
  }

  private static class PurchaseCallback implements AdsRemovalPurchaseCallback,
                                                   Detachable<AdsRemovalPurchaseDialog>
  {
    @Nullable
    private AdsRemovalPurchaseDialog mDialog;
    @Nullable
    private List<SkuDetails> mPendingDetails;
    @Nullable
    private AdsRemovalPaymentState mPendingState;
    private Boolean mPendingActivationResult;

    @Override
    public void onProductDetailsLoaded(@NonNull List<SkuDetails> details)
    {
      if (mDialog == null)
        mPendingDetails = Collections.unmodifiableList(details);
      else
        mDialog.handleProductDetails(details);
      activateStateSafely(AdsRemovalPaymentState.PRICE_SELECTION);
    }

    @Override
    public void onPaymentFailure(@BillingClient.BillingResponse int error)
    {
      Statistics.INSTANCE.trackPurchaseStoreError(error);
      activateStateSafely(AdsRemovalPaymentState.PAYMENT_FAILURE);
    }

    @Override
    public void onProductDetailsFailure()
    {
      activateStateSafely(AdsRemovalPaymentState.PRODUCT_DETAILS_FAILURE);
    }

    @Override
    public void onStoreConnectionFailed()
    {
      activateStateSafely(AdsRemovalPaymentState.PRODUCT_DETAILS_FAILURE);
    }

    @Override
    public void onValidationStarted()
    {
      Statistics.INSTANCE.trackEvent(Statistics.EventName.INAPP_PURCHASE_STORE_SUCCESS);
      activateStateSafely(AdsRemovalPaymentState.VALIDATION);
    }

    @Override
    public void onValidationFinish(boolean success)
    {
      if (mDialog == null)
        mPendingActivationResult = success;
      else
        mDialog.handleActivationResult(success);

      activateStateSafely(AdsRemovalPaymentState.VALIDATION_FINISH);
    }

    void activateStateSafely(@NonNull AdsRemovalPaymentState state)
    {
      if (mDialog == null)
      {
        mPendingState = state;
        return;
      }

      mDialog.activateState(state);
    }

    @Override
    public void attach(@NonNull AdsRemovalPurchaseDialog object)
    {
      mDialog = object;
      if (mPendingDetails != null)
      {
        mDialog.handleProductDetails(mPendingDetails);
        mPendingDetails = null;
      }

      if (mPendingActivationResult != null)
      {
        mDialog.handleActivationResult(mPendingActivationResult);
        mPendingActivationResult = null;
      }

      if (mPendingState != null)
      {
        mDialog.activateState(mPendingState);
        mPendingState = null;
      }
    }

    @Override
    public void detach()
    {
      mDialog = null;
    }
  }

  enum Period
  {
    // Order is important.
    P1Y,
    P1M,
    P1W
  }
}
