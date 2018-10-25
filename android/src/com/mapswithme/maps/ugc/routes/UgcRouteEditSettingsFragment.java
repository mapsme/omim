package com.mapswithme.maps.ugc.routes;

import android.content.Context;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.Fragment;
import android.support.v7.widget.RecyclerView;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.TextView;

import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmToolbarFragment;
import com.mapswithme.maps.base.DataObservable;
import com.mapswithme.maps.base.ObservableHost;
import com.mapswithme.maps.bookmarks.data.BookmarkCategory;
import com.mapswithme.maps.bookmarks.data.BookmarkManager;
import com.mapswithme.maps.widget.ToolbarController;

import java.util.Objects;

public class UgcRouteEditSettingsFragment extends BaseMwmToolbarFragment
{
  private static final String SHARING_OPTIONS_FRAGMENT_TAG = "sharing_options_fragment";

  @SuppressWarnings("NullableProblems")
  @NonNull
  private BookmarkCategory mCategory;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private RecyclerView.AdapterDataObserver mObserver;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private TextView mAccessRulesView;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private ObservableHost<DataObservable> mObserverHost;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private EditText mEditDescView;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private EditText mEditCategoryNameView;

  @Override
  public void onAttach(Context context)
  {
    super.onAttach(context);
    mObserverHost = (ObservableHost<DataObservable>) context;
  }

  @Override
  public void onCreate(@Nullable Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    Bundle args = getArguments();
    if (args == null)
      throw new IllegalArgumentException("Args must be not null");
    mCategory = Objects.requireNonNull(args.getParcelable(UgcRouteEditSettingsActivity.EXTRA_BOOKMARK_CATEGORY));
  }

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
                           @Nullable Bundle savedInstanceState)
  {
    View root = inflater.inflate(R.layout.fragment_ugc_route_edit, container, false);
    initViews(root);
    mObserver = new CategoryObserver();
    mObserverHost.getObservable().registerObserver(mObserver);
    return root;
  }

  private void initViews(@NonNull View root)
  {
    View sharingOptionsBtn = root.findViewById(R.id.open_sharing_options_screen_btn_container);
    mEditCategoryNameView = root.findViewById(R.id.edit_category_name_view);
    mEditCategoryNameView.setText(mCategory.getName());
    mEditCategoryNameView.requestFocus();
    mAccessRulesView = root.findViewById(R.id.sharing_options_desc);
    mAccessRulesView.setText(mCategory.getAccessRules().getResId());
    mEditDescView = root.findViewById(R.id.edit_description);
    mEditDescView.setText(mCategory.getDescription());
    View clearNameBtn = root.findViewById(R.id.edit_text_clear_btn);
    clearNameBtn.setOnClickListener(v -> mEditCategoryNameView.getEditableText().clear());
    sharingOptionsBtn.setOnClickListener(v -> onSharingOptionsClicked());
  }

  @Override
  public void onDestroyView()
  {
    super.onDestroyView();
    mObserverHost.getObservable().unregisterObserver(mObserver);
  }

  private void onSharingOptionsClicked()
  {
    openSharingOptionsScreen();
  }

  private void openSharingOptionsScreen()
  {
    Fragment fragment = UgcSharingOptionsFragment.makeInstance(mCategory);
    getFragmentManager()
        .beginTransaction()
        .replace(R.id.fragment_container, fragment, SHARING_OPTIONS_FRAGMENT_TAG)
        .addToBackStack(null)
        .commit();
  }

  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);
    getToolbarController().setTitle(R.string.edit_list);
  }

  @NonNull
  @Override
  protected ToolbarController onCreateToolbarController(@NonNull View root)
  {
    return new EditSettingsController(root);
  }

  @Override
  public boolean onOptionsItemSelected(MenuItem item)
  {
    if (item.getItemId() == R.id.done)
    {
      onEditDoneClicked();
      return true;
    }
    return super.onOptionsItemSelected(item);
  }

  private void onEditDoneClicked()
  {
    String categoryName = mEditCategoryNameView.getEditableText().toString().trim();

    if (!TextUtils.equals(categoryName, mCategory.getName()))
      BookmarkManager.INSTANCE.setCategoryName(mCategory.getId(),  categoryName);

    String categoryDesc = mEditDescView.getEditableText().toString().trim();
    if (!TextUtils.equals(mCategory.getDescription(), categoryDesc))
      BookmarkManager.INSTANCE.setCategoryDesc(mCategory.getId(), categoryDesc);

  }

  @NonNull
  public static UgcRouteEditSettingsFragment makeInstance(@Nullable Bundle extras)
  {
    UgcRouteEditSettingsFragment fragment = new UgcRouteEditSettingsFragment();
    fragment.setArguments(extras);
    return fragment;
  }

  public class CategoryObserver extends RecyclerView.AdapterDataObserver
  {
    @Override
    public void onChanged()
    {
      mCategory = BookmarkManager.INSTANCE.getAllCategoriesSnapshot().refresh(mCategory);
      mAccessRulesView.setText(mCategory.getAccessRules().getResId());
    }
  }

  private class EditSettingsController extends ToolbarController
  {
    EditSettingsController(@NonNull View root)
    {
      super(root, getActivity());
    }

    @Override
    public void onUpClick()
    {
      getActivity().finish();
    }
  }
}
