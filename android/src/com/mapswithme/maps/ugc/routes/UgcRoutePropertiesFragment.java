package com.mapswithme.maps.ugc.routes;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmFragment;
import com.mapswithme.maps.bookmarks.data.BookmarkManager;
import com.mapswithme.maps.bookmarks.data.CatalogCustomProperty;
import com.mapswithme.maps.bookmarks.data.CatalogCustomPropertyOption;
import com.mapswithme.maps.bookmarks.data.CatalogPropertyOptionAndKey;
import com.mapswithme.maps.bookmarks.data.CatalogTagsGroup;
import com.mapswithme.maps.dialog.AlertDialog;
import com.mapswithme.maps.dialog.AlertDialogCallback;
import com.mapswithme.util.UiUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Objects;

public class UgcRoutePropertiesFragment extends BaseMwmFragment implements BookmarkManager.BookmarksCatalogListener, AlertDialogCallback
{
  public static final String EXTRA_TAGS_ACTIVITY_RESULT = "tags_activity_result";
  public static final String EXTRA_CATEGORY_OPTIONS = "category_options";
  private static final String BUNDLE_SELECTED_OPTION = "selected_property";
  private static final String BUNDLE_CUSTOM_PROPS = "custom_props";
  private static final String ERROR_LOADING_DIALOG_TAG = "error_loading_dialog";
  private static final int REQ_CODE_LOAD_FAILED = 101;
  private static final int MAGIC_MAX_PROPS_COUNT = 1;
  private static final int MAGIC_MAX_OPTIONS_COUNT = 2;
  private static final int FIRST_OPTION_INDEX = 0;
  private static final int SECOND_OPTION_INDEX = 1;

  @NonNull
  private List<CatalogCustomProperty> mProps = Collections.emptyList();

  @Nullable
  private CatalogPropertyOptionAndKey mSelectedOption;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private View mProgress;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private View mPropsContainer;

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
                           @Nullable Bundle savedInstanceState)
  {
    View root = inflater.inflate(R.layout.fragment_ugc_routes_properties, container, false);
    mSelectedOption = getSelectedOption(savedInstanceState);
    mProps = getCustomProps(savedInstanceState);
    initViews(root);
    if (mProps.isEmpty())
      BookmarkManager.INSTANCE.requestCustomProperties();
    return root;
  }

  private void initViews(View root)
  {
    View leftBtn = root.findViewById(R.id.left_btn);
    leftBtn.setOnClickListener(v -> onLeftBtnClicked());
    View rightBtn = root.findViewById(R.id.right_btn);
    rightBtn.setOnClickListener(v -> onRightBtnClicked());
    mPropsContainer = root.findViewById(R.id.properties_container);
    UiUtils.hideIf(mProps.isEmpty(), mPropsContainer);
    mProgress = root.findViewById(R.id.progress);
    UiUtils.showIf(mProps.isEmpty(), mProgress);
  }

  @Nullable
  private static CatalogPropertyOptionAndKey getSelectedOption(@Nullable Bundle savedInstanceState)
  {
    if (savedInstanceState == null)
      return null;
    return savedInstanceState.getParcelable(BUNDLE_SELECTED_OPTION);
  }

  @NonNull
  private static List<CatalogCustomProperty> getCustomProps(@Nullable Bundle savedInstanceState)
  {
    if (savedInstanceState == null)
      return Collections.emptyList();
    return Objects.requireNonNull(savedInstanceState.getParcelableArrayList(BUNDLE_CUSTOM_PROPS));
  }

  @Override
  public void onSaveInstanceState(Bundle outState)
  {
    super.onSaveInstanceState(outState);
    if (mSelectedOption != null)
      outState.putParcelable(BUNDLE_SELECTED_OPTION, mSelectedOption);
    outState.putParcelableArrayList(BUNDLE_CUSTOM_PROPS, new ArrayList<>(mProps));
  }

  private void onBtnClicked(int index)
  {
    checkPropsSize(mProps);
    CatalogCustomProperty property = mProps.get(0);
    CatalogCustomPropertyOption option = property.getOptions().get(index);
    mSelectedOption = new CatalogPropertyOptionAndKey(property.getKey(), option);
    Intent intent = new Intent(getContext(), UgcRouteTagsActivity.class);
    startActivityForResult(intent, UgcSharingOptionsFragment.REQ_CODE_TAGS_ACTIVITY);
  }

  private void onRightBtnClicked()
  {
    onBtnClicked(SECOND_OPTION_INDEX);
  }

  private void onLeftBtnClicked()
  {
    onBtnClicked(FIRST_OPTION_INDEX);
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
  public void onImportStarted(@NonNull String serverId)
  {
    /* Do noting by default */
  }

  @Override
  public void onImportFinished(@NonNull String serverId, long catId, boolean successful)
  {
    /* Do noting by default */
  }

  @Override
  public void onTagsReceived(boolean successful, @NonNull List<CatalogTagsGroup> tagsGroups)
  {
    /* Do noting by default */
  }

  @Override
  public void onCustomPropertiesReceived(boolean successful,
                                         @NonNull List<CatalogCustomProperty> properties)
  {
    if (successful)
      onLoadSuccess(properties);
    else
      onLoadFailed();
  }

  private void onLoadFailed()
  {
    showLoadFailedDialog();
    mProgress.setVisibility(View.GONE);
    mPropsContainer.setVisibility(View.GONE);
  }

  private void showLoadFailedDialog()
  {
    AlertDialog dialog = new AlertDialog.Builder()
        .setTitleId(R.string.discovery_button_viator_error_title)
        .setMessageId(R.string.properties_loading_error_subtitle)
        .setPositiveBtnId(R.string.try_again)
        .setNegativeBtnId(R.string.cancel)
        .setReqCode(REQ_CODE_LOAD_FAILED)
        .setFragManagerStrategy(new AlertDialog.ActivityFragmentManagerStrategy())
        .build();
    dialog.setTargetFragment(this, REQ_CODE_LOAD_FAILED);
    dialog.show(this, ERROR_LOADING_DIALOG_TAG);
  }

  private void onLoadSuccess(@NonNull List<CatalogCustomProperty> properties)
  {
    checkPropsSize(properties);
    mProps = properties;
    mPropsContainer.setVisibility(View.VISIBLE);
    mProgress.setVisibility(View.GONE);
  }

  private static void checkPropsSize(@NonNull List<CatalogCustomProperty> properties)
  {
    if ((properties.size() > MAGIC_MAX_PROPS_COUNT
                       || properties.get(0).getOptions().size() != MAGIC_MAX_OPTIONS_COUNT))
    {
      throw makeException(properties);
    }
  }

  @NonNull
  private static IllegalArgumentException makeException(@NonNull List<CatalogCustomProperty> properties)
  {
    return new IllegalArgumentException("Workaround - multiple selection restricted, because " +
                                       "interface doesn't supported that, " +
                                       "and backend and core can not return single POJO " +
                                       "instance. " +
                                       "Props.size() = " + properties.size() +
                                       (properties.isEmpty() ? ""
                                                             : " Options.size() = "
                                                               + properties.get(0).getOptions()));
  }

  @Override
  public void onUploadStarted(long originCategoryId)
  {
    /* Do noting by default */
  }

  @Override
  public void onUploadFinished(@NonNull BookmarkManager.UploadResult uploadResult,
                               @NonNull String description, long originCategoryId,
                               long resultCategoryId)
  {
    /* Do noting by default */
  }

  @Override
  public void onActivityResult(int requestCode, int resultCode, Intent data)
  {
    super.onActivityResult(requestCode, resultCode, data);
    if (requestCode == UgcSharingOptionsFragment.REQ_CODE_TAGS_ACTIVITY)
    {
      if (resultCode == Activity.RESULT_OK)
      {
        Intent intent = new Intent();
        ArrayList<CatalogPropertyOptionAndKey> options = new ArrayList<>(prepareSelectedOptions());
        intent.putParcelableArrayListExtra(EXTRA_CATEGORY_OPTIONS, options);
        intent.putExtra(EXTRA_TAGS_ACTIVITY_RESULT, data.getExtras());
        getActivity().setResult(Activity.RESULT_OK, intent);
        getActivity().finish();
      }
      else if (resultCode == Activity.RESULT_CANCELED)
      {
        getActivity().setResult(Activity.RESULT_CANCELED);
        getActivity().finish();
      }
    }
  }

  @NonNull
  private List<CatalogPropertyOptionAndKey> prepareSelectedOptions()
  {
    return mSelectedOption == null ? Collections.emptyList()
                                   : Collections.singletonList(mSelectedOption);
  }

  @Override
  public void onAlertDialogPositiveClick(int requestCode, int which)
  {
    mProgress.setVisibility(View.VISIBLE);
    mPropsContainer.setVisibility(View.GONE);
    BookmarkManager.INSTANCE.requestCustomProperties();
  }

  @Override
  public void onAlertDialogNegativeClick(int requestCode, int which)
  {
    getActivity().setResult(Activity.RESULT_CANCELED);
    getActivity().finish();
  }

  @Override
  public void onAlertDialogCancel(int requestCode)
  {
    getActivity().setResult(Activity.RESULT_CANCELED);
    getActivity().finish();
  }
}
