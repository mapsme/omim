package com.mapswithme.maps.base;

import android.content.Context;
import android.os.Bundle;
import androidx.annotation.CallSuper;
import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.appcompat.widget.Toolbar;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import com.mapswithme.maps.R;
import com.mapswithme.maps.widget.PlaceholderView;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.Utils;

public abstract class BaseMwmRecyclerFragment<T extends RecyclerView.Adapter> extends Fragment
{
  private Toolbar mToolbar;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private RecyclerView mRecycler;

  @Nullable
  private PlaceholderView mPlaceholder;

  @SuppressWarnings("NullableProblems")
  @NonNull
  private T mAdapter;

  @NonNull
  protected abstract T createAdapter();

  @LayoutRes
  protected int getLayoutRes()
  {
    return R.layout.fragment_recycler;
  }

  @NonNull
  protected T getAdapter()
  {
    return mAdapter;
  }

  @Override
  public void onAttach(Context context)
  {
    super.onAttach(context);
    Utils.detachFragmentIfCoreNotInitialized(context, this);
  }

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState)
  {
    return inflater.inflate(getLayoutRes(), container, false);
  }

  @CallSuper
  @Override
  public void onViewCreated(View view, Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);

    mToolbar = view.findViewById(R.id.toolbar);
    if (mToolbar != null)
    {
      UiUtils.showHomeUpButton(mToolbar);
      mToolbar.setNavigationOnClickListener(v -> Utils.navigateToParent(getActivity()));
    }

    mRecycler = view.findViewById(R.id.recycler);
    if (mRecycler == null)
      throw new IllegalStateException("RecyclerView not found in layout");

    LinearLayoutManager manager = new LinearLayoutManager(view.getContext());
    manager.setSmoothScrollbarEnabled(true);
    mRecycler.setLayoutManager(manager);
    mAdapter = createAdapter();
    mRecycler.setAdapter(mAdapter);

    mPlaceholder = view.findViewById(R.id.placeholder);
    setupPlaceholder(mPlaceholder);
  }

  public Toolbar getToolbar()
  {
    return mToolbar;
  }

  public RecyclerView getRecyclerView()
  {
    return mRecycler;
  }

  @NonNull
  public PlaceholderView requirePlaceholder()
  {
    if (mPlaceholder != null)
      return mPlaceholder;
    throw new IllegalStateException("Placeholder not found in layout");
  }

  @Override
  public void onResume()
  {
    super.onResume();
    org.alohalytics.Statistics.logEvent("$onResume", this.getClass().getSimpleName()
        + ":" + UiUtils.deviceOrientationAsString(getActivity()));
  }

  @Override
  public void onPause()
  {
    super.onPause();
    org.alohalytics.Statistics.logEvent("$onPause", this.getClass().getSimpleName());
  }

  protected void setupPlaceholder(@Nullable PlaceholderView placeholder) {}

  public void setupPlaceholder()
  {
    setupPlaceholder(mPlaceholder);
  }

  public void showPlaceholder(boolean show)
  {
    if (mPlaceholder != null)
      UiUtils.showIf(show, mPlaceholder);
  }
}
