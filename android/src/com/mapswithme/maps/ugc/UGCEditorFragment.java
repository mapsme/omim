package com.mapswithme.maps.ugc;

import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import com.mapswithme.maps.R;
import com.mapswithme.maps.auth.BaseMwmAuthorizationFragment;
import com.mapswithme.maps.bookmarks.data.FeatureId;
import com.mapswithme.util.log.Logger;
import com.mapswithme.util.log.LoggerFactory;

import java.util.ArrayList;
import java.util.List;

public class UGCEditorFragment extends BaseMwmAuthorizationFragment
{
  private static final Logger LOGGER = LoggerFactory.INSTANCE.getLogger(LoggerFactory.Type.MISC);
  private static final String TAG = UGCEditorFragment.class.getSimpleName();
  @NonNull
  private final UGCRatingAdapter mUGCRatingAdapter = new UGCRatingAdapter();
  //TODO: field just for testing.
  private List<UGC.Rating> mRatings;

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState)
  {
    View root = inflater.inflate(R.layout.fragment_ugc_editor, container, false);
    RecyclerView rvRatingView = (RecyclerView) root.findViewById(R.id.ratings);
    rvRatingView.setLayoutManager(new LinearLayoutManager(getContext()));
    rvRatingView.getLayoutManager().setAutoMeasureEnabled(true);
    rvRatingView.setNestedScrollingEnabled(false);
    rvRatingView.setHasFixedSize(false);
    rvRatingView.setAdapter(mUGCRatingAdapter);

    //TODO: use parcelable instead of serializable
    Intent intent = getActivity().getIntent();
    UGC ugc = (UGC) intent.getSerializableExtra(UGCEditorActivity.EXTRA_UGC);
    mRatings = new ArrayList<>(ugc.getRatings());
    for (UGC.Rating rating: mRatings)
      rating.setValue(getActivity().getIntent().getIntExtra(UGCEditorActivity.EXTRA_AVG_RATING, 3));
    mUGCRatingAdapter.setItems(mRatings);

    return root;
  }

  @Override
  public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState)
  {
    super.onViewCreated(view, savedInstanceState);
    Intent intent = getActivity().getIntent();
    mToolbarController.setTitle(intent.getStringExtra(UGCEditorActivity.EXTRA_TITLE));
  }

  @Override
  protected void onSubmitButtonClick()
  {
    super.onSubmitButtonClick();
    UGC.Rating[] ratings = new UGC.Rating[mRatings.size()];
    for (int i = 0; i < ratings.length; i++)
      ratings[i] = mRatings.get(i);
    //TODO: just for testing
    UGCUpdate update = new UGCUpdate(ratings, "Test text");
    UGC.setUGCUpdate((FeatureId) getActivity()
        .getIntent().getParcelableExtra(UGCEditorActivity.EXTRA_FEATURE_ID), update);
  }

  @Override
  protected void onPreSocialAuthentication()
  {
    LOGGER.i(TAG, "onPreSocialAuthentication()");
  }

  @Override
  protected void onAuthorized()
  {
    LOGGER.i(TAG, "onAuthorized()");
  }

  @Override
  protected void onStartAuthorization()
  {
    LOGGER.i(TAG, "onStartAuthorization()");
  }
}
