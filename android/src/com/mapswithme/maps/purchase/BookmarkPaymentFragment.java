package com.mapswithme.maps.purchase;

import android.app.Activity;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.Purchase;
import com.android.billingclient.api.SkuDetails;
import com.bumptech.glide.Glide;
import com.mapswithme.maps.Framework;
import com.mapswithme.maps.PrivateVariables;
import com.mapswithme.maps.PurchaseOperationObservable;
import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmFragment;
import com.mapswithme.maps.base.Detachable;
import com.mapswithme.maps.bookmarks.data.PaymentData;
import com.mapswithme.maps.dialog.AlertDialogCallback;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.Utils;
import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.LoggerFactory;
import com.mapswithme.util.statistics.Statistics;

import java.util.Collections;
import java.util.List;

public class BookmarkPaymentFragment extends BaseMwmFragment
    implements AlertDialogCallback, PurchaseStateActivator<BookmarkPaymentState>
{
  static final String ARG_PAYMENT_DATA = "arg_payment_data";
  private static final Logger LOGGER = LoggerFactory.INSTANCE.getLogger(LoggerFactory.Type.BILLING);
  private static final String TAG = BookmarkPaymentFragment.class.getSimpleName();
  private static final String EXTRA_CURRENT_STATE = "extra_current_state";
  private static final String EXTRA_PRODUCT_DETAILS = "extra_product_details";
  private static final String EXTRA_VALIDATION_RESULT = "extra_validation_result";

  @SuppressWarnings("NullableProblems")
  @NonNull
  private PurchaseController<PurchaseCallback> mPurchaseController;
  @SuppressWarnings("NullableProblems")
  @NonNull
  private BookmarkPurchaseCallback mPurchaseCallback;
  @SuppressWarnings("NullableProblems")
  @NonNull
  private PaymentData mPaymentData;
  @Nullable
  private ProductDetails mProductDetails;
  private boolean mValidationResult;
  @NonNull
  private BookmarkPaymentState mState = BookmarkPaymentState.NONE;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private BillingManager<PlayStoreBillingCallback> mProductDetailsLoadingManager;

  @NonNull
  private final SubsProductDetailsCallback mSubsProductDetailsCallback
      = new SubsProductDetailsCallback();

  @SuppressWarnings("NullableProblems")
  @NonNull
  private View mProgress;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private TextView mBuySubscriptionButton;

  @Override
  public void onCreate(@Nullable Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    Bundle args = getArguments();
    if (args == null)
      throw new IllegalStateException("Args must be provided for payment fragment!");

    PaymentData paymentData = args.getParcelable(ARG_PAYMENT_DATA);
    if (paymentData == null)
      throw new IllegalStateException("Payment data must be provided for payment fragment!");

    mPaymentData = paymentData;
    mPurchaseCallback = new BookmarkPurchaseCallback(mPaymentData.getServerId());
  }

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable
      Bundle savedInstanceState)
  {
    mPurchaseController = PurchaseFactory.createBookmarkPurchaseController(requireContext(),
                                                                           mPaymentData.getProductId(),
                                                                           mPaymentData.getServerId());
    if (savedInstanceState != null)
      mPurchaseController.onRestore(savedInstanceState);

    mPurchaseController.initialize(requireActivity());

    mProductDetailsLoadingManager = PurchaseFactory.createSubscriptionBillingManager();
    mProductDetailsLoadingManager.initialize(requireActivity());
    List<String> productIds =
        Collections.singletonList(PrivateVariables.bookmarksSubscriptionMonthlyProductId());
    mSubsProductDetailsCallback.attach(this);
    mProductDetailsLoadingManager.addCallback(mSubsProductDetailsCallback);
    mProductDetailsLoadingManager.queryProductDetails(productIds);

    View root = inflater.inflate(R.layout.fragment_bookmark_payment, container, false);
    mProgress = root.findViewById(R.id.progress);
    mBuySubscriptionButton = root.findViewById(R.id.buy_btn);
    mBuySubscriptionButton.setOnClickListener(v -> onBuySubscriptionClicked());
    TextView buyInappBtn = root.findViewById(R.id.buy_inapp_btn);
    buyInappBtn.setOnClickListener(v -> onBuyInappClicked());
    return root;
  }

  private void onBuySubscriptionClicked()
  {
    BookmarkSubscriptionActivity.start(requireActivity());
  }

  private void onBuyInappClicked()
  {
    Statistics.INSTANCE.trackPurchasePreviewSelect(mPaymentData.getServerId(),
                                                   mPaymentData.getProductId());
    Statistics.INSTANCE.trackPurchaseEvent(Statistics.EventName.INAPP_PURCHASE_PREVIEW_PAY,
                                           mPaymentData.getServerId());
    startPurchaseTransaction();
  }

  @Override
  public boolean onBackPressed()
  {
    if (mState == BookmarkPaymentState.VALIDATION)
    {
      Toast.makeText(requireContext(), R.string.purchase_please_wait_toast, Toast.LENGTH_SHORT)
           .show();
      return true;
    }

    Statistics.INSTANCE.trackPurchaseEvent(Statistics.EventName.INAPP_PURCHASE_PREVIEW_CANCEL,
                                           mPaymentData.getServerId());
    return super.onBackPressed();
  }

  @Override
  public void onViewCreated(View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);
    if (savedInstanceState == null)
      Statistics.INSTANCE.trackPurchasePreviewShow(mPaymentData.getServerId(),
                                                   PrivateVariables.bookmarksVendor(),
                                                   mPaymentData.getProductId());
    LOGGER.d(TAG, "onViewCreated savedInstanceState = " + savedInstanceState);
    setInitialPaymentData();
    loadImage();
    if (savedInstanceState != null)
    {
      mProductDetails = savedInstanceState.getParcelable(EXTRA_PRODUCT_DETAILS);
      if (mProductDetails != null)
        updateProductDetails();
      mValidationResult = savedInstanceState.getBoolean(EXTRA_VALIDATION_RESULT);
      BookmarkPaymentState savedState
          = BookmarkPaymentState.values()[savedInstanceState.getInt(EXTRA_CURRENT_STATE)];
      activateState(savedState);
      return;
    }

    activateState(BookmarkPaymentState.PRODUCT_DETAILS_LOADING);
    mPurchaseController.queryProductDetails();
  }

  @Override
  public void onDestroyView()
  {
    super.onDestroyView();
    mPurchaseController.destroy();
    mProductDetailsLoadingManager.removeCallback(mSubsProductDetailsCallback);
    mSubsProductDetailsCallback.detach();
    mProductDetailsLoadingManager.destroy();
  }

  private void startPurchaseTransaction()
  {
    activateState(BookmarkPaymentState.TRANSACTION_STARTING);
    Framework.nativeStartPurchaseTransaction(mPaymentData.getServerId(),
                                             PrivateVariables.bookmarksVendor());
  }

  void launchBillingFlow()
  {
    mPurchaseController.launchPurchaseFlow(mPaymentData.getProductId());
    activateState(BookmarkPaymentState.PAYMENT_IN_PROGRESS);
  }

  @Override
  public void onStart()
  {
    super.onStart();
    PurchaseOperationObservable observable = PurchaseOperationObservable.from(requireContext());
    observable.addTransactionObserver(mPurchaseCallback);
    mPurchaseController.addCallback(mPurchaseCallback);
    mPurchaseCallback.attach(this);
  }

  @Override
  public void onStop()
  {
    super.onStop();
    PurchaseOperationObservable observable = PurchaseOperationObservable.from(requireContext());
    observable.removeTransactionObserver(mPurchaseCallback);
    mPurchaseController.removeCallback();
    mPurchaseCallback.detach();
  }

  @Override
  public void onSaveInstanceState(Bundle outState)
  {
    super.onSaveInstanceState(outState);
    LOGGER.d(TAG, "onSaveInstanceState");
    outState.putInt(EXTRA_CURRENT_STATE, mState.ordinal());
    outState.putParcelable(EXTRA_PRODUCT_DETAILS, mProductDetails);
    mPurchaseController.onSave(outState);
  }

  @Override
  public void activateState(@NonNull BookmarkPaymentState state)
  {
    if (state == mState)
      return;

    LOGGER.i(TAG, "Activate state: " + state);
    mState = state;
    mState.activate(this);
  }

  private void loadImage()
  {
    if (TextUtils.isEmpty(mPaymentData.getImgUrl()))
      return;

    ImageView imageView = getViewOrThrow().findViewById(R.id.image);
    Glide.with(imageView.getContext())
         .load(mPaymentData.getImgUrl())
         .centerCrop()
         .into(imageView);
  }

  private void setInitialPaymentData()
  {
    TextView name = getViewOrThrow().findViewById(R.id.product_catalog_name);
    name.setText(mPaymentData.getName());
    TextView author = getViewOrThrow().findViewById(R.id.author_name);
    author.setText(mPaymentData.getAuthorName());
  }

  private void handleProductDetails(@NonNull List<SkuDetails> details)
  {
    if (details.isEmpty())
      return;

    SkuDetails skuDetails = details.get(0);
    mProductDetails = PurchaseUtils.toProductDetails(skuDetails);
  }

  private void handleValidationResult(boolean validationResult)
  {
    mValidationResult = validationResult;
  }

  @Override
  public void onAlertDialogPositiveClick(int requestCode, int which)
  {
    handleErrorDialogEvent(requestCode);
  }

  @Override
  public void onAlertDialogNegativeClick(int requestCode, int which)
  {
    // Do nothing by default.
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
      case PurchaseUtils.REQ_CODE_PRODUCT_DETAILS_FAILURE:
        requireActivity().finish();
        break;
      case PurchaseUtils.REQ_CODE_START_TRANSACTION_FAILURE:
      case PurchaseUtils.REQ_CODE_PAYMENT_FAILURE:
        activateState(BookmarkPaymentState.PRODUCT_DETAILS_LOADED);
        break;
    }
  }

  public void updateProductDetails()
  {
    if (mProductDetails == null)
      throw new AssertionError("Product details must be obtained at this moment!");

    TextView buyButton = getViewOrThrow().findViewById(R.id.cancel_btn);
    String price = Utils.formatCurrencyString(mProductDetails.getPrice(),
                                              mProductDetails.getCurrencyCode());
    buyButton.setText(getString(R.string.buy_btn, price));
    TextView storeName = getViewOrThrow().findViewById(R.id.product_store_name);
    storeName.setText(mProductDetails.getTitle());
  }

  public void finishValidation()
  {
    if (mValidationResult)
      requireActivity().setResult(Activity.RESULT_OK);

    requireActivity().finish();
  }

  private void onProductDetailsLoaded(@NonNull ProductDetails productDetails)
  {
    String formattedPrice = Utils.formatCurrencyString(productDetails.getPrice(),
                                                       productDetails.getCurrencyCode());
    UiUtils.hide(mProgress);
    mBuySubscriptionButton.setText(getString(R.string.buy_btn_for_subscription, formattedPrice));
  }

  private static class BookmarkPurchaseCallback
      extends StatefulPurchaseCallback<BookmarkPaymentState, BookmarkPaymentFragment>
      implements PurchaseCallback, Detachable<BookmarkPaymentFragment>, CoreStartTransactionObserver
  {
    @Nullable
    private List<SkuDetails> mPendingDetails;
    private Boolean mPendingValidationResult;
    @NonNull
    private final String mServerId;

    private BookmarkPurchaseCallback(@NonNull String serverId)
    {
      mServerId = serverId;
    }

    @Override
    public void onStartTransaction(boolean success, @NonNull String serverId, @NonNull String
        vendorId)
    {
      if (!success)
      {
        activateStateSafely(BookmarkPaymentState.TRANSACTION_FAILURE);
        return;
      }

      activateStateSafely(BookmarkPaymentState.TRANSACTION_STARTED);
    }

    @Override
    public void onProductDetailsLoaded(@NonNull List<SkuDetails> details)
    {
      if (getUiObject() == null)
        mPendingDetails = Collections.unmodifiableList(details);
      else
        getUiObject().handleProductDetails(details);
      activateStateSafely(BookmarkPaymentState.PRODUCT_DETAILS_LOADED);
    }

    @Override
    public void onPaymentFailure(@BillingClient.BillingResponse int error)
    {
      activateStateSafely(BookmarkPaymentState.PAYMENT_FAILURE);
    }

    @Override
    public void onProductDetailsFailure()
    {
      activateStateSafely(BookmarkPaymentState.PRODUCT_DETAILS_FAILURE);
    }

    @Override
    public void onStoreConnectionFailed()
    {
      activateStateSafely(BookmarkPaymentState.PRODUCT_DETAILS_FAILURE);
    }

    @Override
    public void onValidationStarted()
    {
      Statistics.INSTANCE.trackPurchaseEvent(Statistics.EventName.INAPP_PURCHASE_STORE_SUCCESS,
                                             mServerId);
      activateStateSafely(BookmarkPaymentState.VALIDATION);
    }

    @Override
    public void onValidationFinish(boolean success)
    {
      if (getUiObject() == null)
        mPendingValidationResult = success;
      else
        getUiObject().handleValidationResult(success);

      activateStateSafely(BookmarkPaymentState.VALIDATION_FINISH);
    }

    @Override
    void onAttach(@NonNull BookmarkPaymentFragment uiObject)
    {
      if (mPendingDetails != null)
      {
        uiObject.handleProductDetails(mPendingDetails);
        mPendingDetails = null;
      }

      if (mPendingValidationResult != null)
      {
        uiObject.handleValidationResult(mPendingValidationResult);
        mPendingValidationResult = null;
      }
    }
  }

  private static class SubsProductDetailsCallback
      extends StatefulPurchaseCallback<BookmarkPaymentState, BookmarkPaymentFragment>
      implements PlayStoreBillingCallback
  {
    @Nullable
    private List<SkuDetails> mPendingDetails;

    @Override
    public void onProductDetailsLoaded(@NonNull List<SkuDetails> details)
    {
      if (details.isEmpty() || PurchaseUtils.hasIncorrectSkuDetails(details))
      {
        activateStateSafely(BookmarkPaymentState.PRODUCT_DETAILS_FAILURE);
        return;
      }

      if (getUiObject() == null)
      {
        mPendingDetails = Collections.unmodifiableList(details);
        return;
      }

      handleProductDetails(getUiObject(), details);
    }

    private static void handleProductDetails(@NonNull BookmarkPaymentFragment uiObject,
                                      @NonNull List<SkuDetails> details)
    {
      ProductDetails productDetails = PurchaseUtils.toProductDetails(details.get(0));
      uiObject.onProductDetailsLoaded(productDetails);
    }

    @Override
    void onAttach(@NonNull BookmarkPaymentFragment bookmarkPaymentFragment)
    {
      if (mPendingDetails == null)
        return;

      handleProductDetails(bookmarkPaymentFragment, mPendingDetails);
      mPendingDetails = null;
    }

    @Override
    public void onPurchaseSuccessful(@NonNull List<Purchase> purchases)
    {
      // Do nothing.
    }

    @Override
    public void onPurchaseFailure(int error)
    {
      // Do nothing.
    }


    @Override
    public void onProductDetailsFailure()
    {
      // Do nothing.
    }

    @Override
    public void onStoreConnectionFailed()
    {
      // Do nothing.
    }

    @Override
    public void onPurchasesLoaded(@NonNull List<Purchase> purchases)
    {
      // Do nothing.
    }

    @Override
    public void onConsumptionSuccess()
    {
      throw new UnsupportedOperationException();
    }

    @Override
    public void onConsumptionFailure()
    {
      throw new UnsupportedOperationException();
    }
  }
}
