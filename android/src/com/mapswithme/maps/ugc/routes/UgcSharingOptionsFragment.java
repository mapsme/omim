package com.mapswithme.maps.ugc.routes;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
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

import com.mapswithme.maps.Framework;
import com.mapswithme.maps.R;
import com.mapswithme.maps.auth.BaseMwmAuthorizationFragment;
import com.mapswithme.maps.bookmarks.data.BookmarkManager;
import com.mapswithme.maps.bookmarks.data.CatalogCustomProperty;
import com.mapswithme.maps.bookmarks.data.CatalogTagsGroup;
import com.mapswithme.maps.bookmarks.data.BookmarkCategory;
import com.mapswithme.maps.dialog.AlertDialog;
import com.mapswithme.maps.dialog.ProgressDialogFragment;
import com.mapswithme.maps.widget.ToolbarController;
import com.mapswithme.util.ConnectionState;
import com.mapswithme.util.UiUtils;

import java.util.List;
import java.util.Objects;

public class UgcSharingOptionsFragment extends BaseMwmAuthorizationFragment implements BookmarkManager.BookmarksCatalogListener
{
  public static final String EXTRA_BOOKMARK_CATEGORY = "bookmark_category";
  private static final String NO_NETWORK_CONNECTION_DIALOG_TAG = "no_network_connection_dialog";
  private static final String UPLOADING_PROGRESS_DIALOG_TAG = "uploading_progress_dialog";

  @SuppressWarnings("NullableProblems")
  @NonNull
  private View mGetDirectLinkContainer;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private BookmarkCategory mCategory;

  @Override
  public void onCreate(@Nullable Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    Bundle args = Objects.requireNonNull(getArguments());
    mCategory = Objects.requireNonNull(args.getParcelable(EXTRA_BOOKMARK_CATEGORY));
  }

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
                           @Nullable Bundle savedInstanceState)
  {
    View root = inflater.inflate(R.layout.fragment_ugc_routes_sharing_options, container, false);
    mGetDirectLinkContainer = root.findViewById(R.id.get_direct_link_container);
    initClickListeners(root);
    TextView licenceAgreementText = root.findViewById(R.id.license_agreement_message);

    String src = getResources().getString(R.string.ugc_routes_user_agreement,
                                          Framework.nativeGetPrivacyPolicyLink());
    Spanned spanned = Html.fromHtml(src);
    licenceAgreementText.setMovementMethod(LinkMovementMethod.getInstance());
    licenceAgreementText.setText(spanned);
    return root;
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
    return new SharingOptionsController(root);
  }

  private void initClickListeners(@NonNull View root)
  {
    View getDirectLinkView = root.findViewById(R.id.get_direct_link_text);
    getDirectLinkView.setOnClickListener(directLinkListener -> onGetDirectLinkClicked());
    View uploadAndPublishView = root.findViewById(R.id.upload_and_publish_text);
    uploadAndPublishView.setOnClickListener(uploadListener -> onUploadAndPublishBtnClicked());
  }

  private void onUploadAndPublishBtnClicked()
  {
    if (isNetworkConnectionAbsent())
    {
      showNoNetworkConnectionDialog();
      return;
    }

    if (isAuthorized())
      openTagsScreen();
    else
      authorize();
  }

  private void showNoNetworkConnectionDialog()
  {
    Fragment fragment = getFragmentManager().findFragmentByTag(NO_NETWORK_CONNECTION_DIALOG_TAG);
    if (fragment != null)
      return;

    AlertDialog dialog = new AlertDialog.Builder()
        .setTitleId(R.string.common_check_internet_connection_dialog_title)
        .setMessageId(R.string.common_check_internet_connection_dialog)
        .setPositiveBtnId(R.string.ok)
        .build();
    dialog.show(dialog, NO_NETWORK_CONNECTION_DIALOG_TAG);
  }

  private boolean isNetworkConnectionAbsent()
  {
    return !ConnectionState.isConnected();
  }

  private void openTagsScreen()
  {

  }

  private void onGetDirectLinkClicked()
  {
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
    return true;
  }

  private void requestDirectLink()
  {
    showProgress();
    BookmarkManager.INSTANCE.uploadRoutes(1, null);
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
  public void onActivityResult(int requestCode, int resultCode, Intent data)
  {
    super.onActivityResult(requestCode, resultCode, data);
    if (requestCode == 1 && resultCode == Activity.RESULT_OK)
      requestPublishing(data);
  }

  private void requestPublishing(@NonNull Intent data)
  {
    showProgress();
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
  public void onUploadFinished(int uploadResult, @NonNull String description,
                               long originCategoryId, long resultCategoryId)
  {

    if (isOkResult(uploadResult))
    {
      hideProgress();
      if (isDirectLinkUploadMode())
      {
        /* not implemented yet */
      }
      else
      {
        UiUtils.hide(mGetDirectLinkContainer);
      }
    }
  }

  private boolean isOkResult(int uploadResult)
  {
    return true;
  }

  private class SharingOptionsController extends ToolbarController
  {
    SharingOptionsController(@NonNull View root)
    {
      super(root, getActivity());
    }

    @Override
    public void onUpClick()
    {
      if (getFragmentManager().getBackStackEntryCount() == 0)
        getActivity().finish();
      else
        getFragmentManager().popBackStackImmediate();
    }
  }
}
