package com.mapswithme.maps.ugc.routes;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.StringRes;
import android.support.v4.app.DialogFragment;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.text.Html;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import android.widget.Toast;

import com.mapswithme.maps.Framework;
import com.mapswithme.maps.R;
import com.mapswithme.maps.auth.BaseMwmAuthorizationFragment;
import com.mapswithme.maps.base.FinishActivityToolbarController;
import com.mapswithme.maps.bookmarks.data.AbstractCategoriesSnapshot;
import com.mapswithme.maps.bookmarks.data.BookmarkCategory;
import com.mapswithme.maps.bookmarks.data.BookmarkManager;
import com.mapswithme.maps.bookmarks.data.CatalogCustomProperty;
import com.mapswithme.maps.bookmarks.data.CatalogTag;
import com.mapswithme.maps.bookmarks.data.CatalogTagsGroup;
import com.mapswithme.maps.dialog.AlertDialog;
import com.mapswithme.maps.dialog.AlertDialogCallback;
import com.mapswithme.maps.dialog.ProgressDialogFragment;
import com.mapswithme.maps.ugc.route.SendLinkPlaceholderActivity;
import com.mapswithme.maps.ugc.route.SendLinkPlaceholderFragment;
import com.mapswithme.maps.widget.ToolbarController;
import com.mapswithme.util.ConnectionState;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.Utils;
import com.mapswithme.util.sharing.TargetUtils;

import java.util.List;
import java.util.Objects;

public class UgcSharingOptionsFragment extends BaseMwmAuthorizationFragment implements BookmarkManager.BookmarksCatalogListener,
                                                                                       AlertDialogCallback
{
  private static final String BUNDLE_CURRENT_MODE = "current_mode";
  private static final String UPLOADING_PROGRESS_DIALOG_TAG = "uploading_progress_dialog";
  private static final String NO_NETWORK_CONNECTION_DIALOG_TAG = "no_network_connection_dialog";
  private static final String ERROR_BROKEN_FILE_DIALOG_TAG = "error_broken_file_dialog";
  private static final String ERROR_EDITED_ON_WEB_DIALOG_REQ_TAG = "error_edited_on_web_dialog";
  private static final String ERROR_COMMON_DIALOG_TAG = "error_common_dialog";
  private static final int NO_NETWORK_CONNECTION_DIALOG_REQ_CODE = 200;
  private static final int ERROR_BROKEN_FILE_DIALOG_REQ_CODE = 201;
  private static final int ERROR_EDITED_ON_WEB_DIALOG_REQ_CODE = 202;
  private static final int ERROR_COMMON_REQ_CODE = 203;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private View mGetDirectLinkContainer;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private BookmarkCategory mCategory;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private View mPublishCategoryImage;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private View mGetDirectLinkImage;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private View mEditOnWebBtn;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private View mPublishingCompletedStatusContainer;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private View mGetDirectLinkCompletedStatusContainer;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private View mUploadAndPublishText;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private View mGetDirectLinkText;

  @Nullable
  private BookmarkCategory.AccessRules mCurrentMode;

  @Override
  public void onCreate(@Nullable Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    Bundle args = Objects.requireNonNull(getArguments());
    BookmarkCategory rawCategory = args.getParcelable(UgcRouteSharingOptionsActivity.EXTRA_BOOKMARK_CATEGORY);
    AbstractCategoriesSnapshot.Default snapshot = BookmarkManager.INSTANCE.getAllCategoriesSnapshot();
    mCategory = snapshot.refresh(Objects.requireNonNull(rawCategory));
  }

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
                           @Nullable Bundle savedInstanceState)
  {
    View root = inflater.inflate(R.layout.fragment_ugc_routes_sharing_options, container, false);
    initViews(root);
    initClickListeners(root);
    mCurrentMode = getCurrentMode(savedInstanceState);
    toggleViewsVisibility();
    return root;
  }

  private void initViews(View root)
  {
    mGetDirectLinkContainer = root.findViewById(R.id.get_direct_link_container);
    View publishCategoryContainer = root.findViewById(R.id.upload_and_publish_container);
    mPublishCategoryImage = publishCategoryContainer.findViewById(R.id.upload_and_publish_image);
    mGetDirectLinkImage = mGetDirectLinkContainer.findViewById(R.id.get_direct_link_image);
    mEditOnWebBtn = root.findViewById(R.id.edit_on_web_btn);
    mPublishingCompletedStatusContainer = root.findViewById(R.id.publishing_completed_status_container);
    mGetDirectLinkCompletedStatusContainer = root.findViewById(R.id.get_direct_link_completed_status_container);
    mUploadAndPublishText = root.findViewById(R.id.upload_and_publish_text);
    mGetDirectLinkText = root.findViewById(R.id.get_direct_link_text);
    TextView licenceAgreementText = root.findViewById(R.id.license_agreement_message);

    String src = getResources().getString(R.string.ugc_routes_user_agreement,
                                          Framework.nativeGetPrivacyPolicyLink());
    Spanned spanned = Html.fromHtml(src);
    licenceAgreementText.setMovementMethod(LinkMovementMethod.getInstance());
    licenceAgreementText.setText(spanned);
  }

  private void toggleViewsVisibility()
  {
    boolean isPublished = mCategory.getAccessRules() == BookmarkCategory.AccessRules.ACCESS_RULES_PUBLIC;
    UiUtils.hideIf(isPublished, mUploadAndPublishText, mGetDirectLinkContainer);
    UiUtils.showIf(isPublished, mPublishingCompletedStatusContainer, mEditOnWebBtn);
    mPublishCategoryImage.setSelected(!isPublished);

    boolean isLinkSuccessFormed = mCategory.getAccessRules() == BookmarkCategory.AccessRules.ACCESS_RULES_DIRECT_LINK;
    UiUtils.hideIf(isLinkSuccessFormed, mGetDirectLinkText);
    UiUtils.showIf(isLinkSuccessFormed, mGetDirectLinkCompletedStatusContainer);
    mGetDirectLinkImage.setSelected(!isLinkSuccessFormed);
  }

  @Nullable
  private static BookmarkCategory.AccessRules getCurrentMode(@Nullable Bundle bundle)
  {
    return bundle != null && bundle.containsKey(BUNDLE_CURRENT_MODE)
                   ? BookmarkCategory.AccessRules.values()[bundle.getInt(BUNDLE_CURRENT_MODE)]
                   : null;
  }

  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);
    getToolbarController().setTitle(R.string.sharing_options);
  }

  @NonNull
  @Override
  protected ToolbarController onCreateToolbarController(@NonNull View root)
  {
    return new FinishActivityToolbarController(root, getActivity());
  }

  private void initClickListeners(@NonNull View root)
  {
    View getDirectLinkView = root.findViewById(R.id.get_direct_link_text);
    getDirectLinkView.setOnClickListener(directLinkListener -> onGetDirectLinkClicked());
    View uploadAndPublishView = root.findViewById(R.id.upload_and_publish_text);
    uploadAndPublishView.setOnClickListener(uploadListener -> onUploadAndPublishBtnClicked());
    View shareDirectLinkBtn = mGetDirectLinkCompletedStatusContainer.findViewById(R.id.share_direct_link_btn);
    shareDirectLinkBtn.setOnClickListener(v -> onDirectLinkShared());
    View sharePublishedBtn = mPublishingCompletedStatusContainer.findViewById(R.id.share_published_category_btn);
    sharePublishedBtn.setOnClickListener(v -> onPublishedCategoryShared());
    View editOnWebBtn = root.findViewById(R.id.edit_on_web_btn);
    editOnWebBtn.setOnClickListener(v -> onEditOnWebClicked());
  }

  private void onEditOnWebClicked()
  {
    String deepLink = BookmarkManager.INSTANCE.getCatalogDeeplink(mCategory.getId());
    Intent intent = new Intent(getContext(), SendLinkPlaceholderActivity.class)
        .putExtra(SendLinkPlaceholderFragment.EXTRA_SHARED_LINK, deepLink);
    startActivity(intent);
  }

  private void onPublishedCategoryShared()
  {
    shareDefault();
  }

  private void shareDefault()
  {
    String deepLink = BookmarkManager.INSTANCE.getCatalogDeeplink(mCategory.getId());
    Intent intent = new Intent(Intent.ACTION_SEND)
        .setType(TargetUtils.TYPE_TEXT_PLAIN)
        .putExtra(Intent.EXTRA_SUBJECT, deepLink)
        .putExtra(Intent.EXTRA_TEXT, getString(R.string.share_bookmarks_email_body));
    startActivity(Intent.createChooser(intent, getString(R.string.share)));
  }

  private void onDirectLinkShared()
  {
    shareDefault();
  }

  private void showNoNetworkConnectionDialog()
  {
    Fragment fragment = getFragmentManager().findFragmentByTag(NO_NETWORK_CONNECTION_DIALOG_TAG);
    if (fragment != null)
      return;

    AlertDialog dialog = new AlertDialog.Builder()
        .setTitleId(R.string.common_check_internet_connection_dialog_title)
        .setMessageId(R.string.common_check_internet_connection_dialog)
        .setPositiveBtnId(R.string.try_again)
        .setNegativeBtnId(R.string.cancel)
        .setFragManagerStrategy(new AlertDialog.ActivityFragmentManagerStrategy())
        .setReqCode(NO_NETWORK_CONNECTION_DIALOG_REQ_CODE)
        .build();
    dialog.setTargetFragment(this, NO_NETWORK_CONNECTION_DIALOG_REQ_CODE);
    dialog.show(this, NO_NETWORK_CONNECTION_DIALOG_TAG);
  }

  private boolean isNetworkConnectionAbsent()
  {
    return !ConnectionState.isConnected();
  }

  private void openTagsScreen()
  {
    Intent intent = new Intent(getContext(), UgcRouteTagsActivity.class);
    startActivityForResult(intent, UgcRouteTagsActivity.REQUEST_CODE);
  }

  private void onUploadAndPublishBtnClicked()
  {
    mCurrentMode = BookmarkCategory.AccessRules.ACCESS_RULES_PUBLIC;
    onUploadBtnClicked();
  }

  private void onGetDirectLinkClicked()
  {
    mCurrentMode = BookmarkCategory.AccessRules.ACCESS_RULES_DIRECT_LINK;
    onUploadBtnClicked();
  }

  private void onUploadBtnClicked()
  {
    if (isNetworkConnectionAbsent())
    {
      showNoNetworkConnectionDialog();
      return;
    }

    if (isAuthorized())
      onPostAuthCompleted();
    else
      authorize();
  }

  private void onPostAuthCompleted()
  {
    if (isDirectLinkUploadMode())
      requestDirectLink();
    else
      openTagsScreen();
  }

  private boolean isDirectLinkUploadMode()
  {
    return mCurrentMode == BookmarkCategory.AccessRules.ACCESS_RULES_DIRECT_LINK;
  }

  private void requestDirectLink()
  {
    if (mCurrentMode == null)
      throw new IllegalStateException("CurrentMode must be initialized");
    showProgress();
    BookmarkManager.INSTANCE.uploadRoutes(mCurrentMode.ordinal(), mCategory);
  }

  private void showProgress()
  {
    String title = getString(R.string.upload_and_publish_progress_text);
    ProgressDialogFragment dialog = ProgressDialogFragment.newInstance(title);
    getFragmentManager()
        .beginTransaction()
        .add(dialog, UPLOADING_PROGRESS_DIALOG_TAG)
        .commitAllowingStateLoss();
  }


  private void hideProgress()
  {
    FragmentManager fm = getFragmentManager();
    DialogFragment frag = (DialogFragment) fm.findFragmentByTag(UPLOADING_PROGRESS_DIALOG_TAG);
    if (frag != null)
      frag.dismissAllowingStateLoss();
  }

  @Override
  public void onSaveInstanceState(Bundle outState)
  {
    super.onSaveInstanceState(outState);
    if (mCurrentMode != null)
      outState.putInt(BUNDLE_CURRENT_MODE, mCurrentMode.ordinal());
  }

  @Override
  public void onActivityResult(int requestCode, int resultCode, Intent data)
  {
    super.onActivityResult(requestCode, resultCode, data);
    if (requestCode == UgcRouteTagsActivity.REQUEST_CODE && resultCode == Activity.RESULT_OK)
      requestPublishing(data);
  }

  private void requestPublishing(@NonNull Intent data)
  {
    showProgress();
    List<CatalogTag> tags = data.getParcelableArrayListExtra(UgcRouteTagsActivity.EXTRA_TAGS);
    BookmarkManager.INSTANCE.setCategoryTags(mCategory, tags);
    BookmarkManager.INSTANCE.uploadToCatalog(BookmarkCategory.AccessRules.ACCESS_RULES_PUBLIC, mCategory);
  }

  @Override
  public void onStart()
  {
    super.onStart();
    BookmarkManager.INSTANCE.addCatalogListener(this);
  }

  @Override
  public void onStop()
  {
    super.onStop();
    BookmarkManager.INSTANCE.removeCatalogListener(this);
  }

  @Override
  public void onAuthorizationFinish(boolean success)
  {
    if (success)
      onPostAuthCompleted();
  }

  @Override
  public void onAuthorizationStart()
  {

  }

  @Override
  public void onSocialAuthenticationCancel(int type)
  {

  }

  @Override
  public void onSocialAuthenticationError(int type, @Nullable String error)
  {

  }

  @Override
  public void onImportStarted(@NonNull String serverId)
  {

  }

  @Override
  public void onImportFinished(@NonNull String serverId, long catId, boolean successful)
  {

  }

  @Override
  public void onTagsReceived(boolean successful, @NonNull List<CatalogTagsGroup> tagsGroups)
  {

  }

  @Override
  public void onCustomPropertiesReceived(boolean successful,
                                         @NonNull List<CatalogCustomProperty> properties)
  {

  }

  @Override
  public void onUploadStarted(long originCategoryId)
  {

  }

  @Override
  public void onUploadFinished(@NonNull BookmarkManager.UploadResult uploadResult,
                               @NonNull String description, long originCategoryId,
                               long resultCategoryId)
  {
    hideProgress();
    if (isOkResult(uploadResult))
      onUploadSuccess();
    else
      onUploadError(uploadResult);
  }

  private void onUploadError(@NonNull BookmarkManager.UploadResult uploadResult)
  {
    /* FIXME later */
    if (uploadResult == BookmarkManager.UploadResult.UPLOAD_RESULT_MALFORMED_DATA_ERROR)
    {
      showErrorBrokenFileDialog();
      return;
    }

    if (uploadResult == BookmarkManager.UploadResult.UPLOAD_RESULT_ACCESS_ERROR)
    {
      showErrorEditedOnWebDialog();
      return;
    }

    showCommonErrorDialog();
  }

  private void showCommonErrorDialog()
  {
    showErrorDialog(R.string.unable_upload_error_subtitle_broken,
                    ERROR_COMMON_REQ_CODE,
                    ERROR_COMMON_DIALOG_TAG);
  }

  private void onUploadSuccess()
  {
    mCategory = BookmarkManager.INSTANCE.getAllCategoriesSnapshot().refresh(mCategory);
    checkSuccessUploadedCategoryAccessRules();

    boolean isDirectLinkMode = mCategory.getAccessRules() == BookmarkCategory.AccessRules.ACCESS_RULES_DIRECT_LINK;

    int successMsgResId = isDirectLinkMode ? R.string.direct_link_success
                                           : R.string.upload_and_publish_success;
    Toast.makeText(getContext(), successMsgResId, Toast.LENGTH_SHORT).show();
    toggleViewsVisibility();
  }

  private void checkSuccessUploadedCategoryAccessRules()
  {
    if (mCategory.getAccessRules() != BookmarkCategory.AccessRules.ACCESS_RULES_PUBLIC
        && mCategory.getAccessRules() != BookmarkCategory.AccessRules.ACCESS_RULES_DIRECT_LINK)
      throw new IllegalStateException("Access rules must be ACCESS_RULES_PUBLIC or ACCESS_RULES_DIRECT_LINK." +
                                      " Current value = " + mCategory.getAccessRules());
  }


  private void showErrorEditedOnWebDialog()
  {
    showErrorDialog(R.string.unable_upload_error_subtitle_edited,
                    ERROR_EDITED_ON_WEB_DIALOG_REQ_CODE,
                    ERROR_EDITED_ON_WEB_DIALOG_REQ_TAG);
  }


  private void showErrorBrokenFileDialog()
  {
    showErrorDialog(R.string.unable_upload_error_subtitle_broken,
                    ERROR_BROKEN_FILE_DIALOG_REQ_CODE,
                    ERROR_BROKEN_FILE_DIALOG_TAG);
  }

  private void showErrorDialog(@StringRes int subtitle, int reqCode, @NonNull String tag)
  {
    AlertDialog dialog = new AlertDialog.Builder()
        .setTitleId(R.string.unable_upload_errorr_title)
        .setMessageId(subtitle)
        .setPositiveBtnId(R.string.ok)
        .setReqCode(reqCode)
        .setFragManagerStrategy(new AlertDialog.ActivityFragmentManagerStrategy())
        .build();
    dialog.setTargetFragment(this, reqCode);
    dialog.show(this, tag);
  }

  private boolean isOkResult(@NonNull BookmarkManager.UploadResult uploadResult)
  {
    return uploadResult == BookmarkManager.UploadResult.UPLOAD_RESULT_SUCCESS;
  }

  @Override
  public void onAlertDialogPositiveClick(int requestCode, int which)
  {
    if (requestCode == NO_NETWORK_CONNECTION_DIALOG_REQ_CODE)
      Utils.showSystemSettings(getContext());

  }

  @Override
  public void onAlertDialogNegativeClick(int requestCode, int which)
  {

  }

  @Override
  public void onAlertDialogCancel(int requestCode)
  {

  }
}
