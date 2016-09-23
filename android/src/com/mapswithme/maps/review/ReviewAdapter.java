package com.mapswithme.maps.review;

import android.support.annotation.CallSuper;
import android.support.v7.widget.RecyclerView;
import android.text.TextUtils;
import android.text.format.DateFormat;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import com.mapswithme.maps.R;
import com.mapswithme.maps.widget.recycler.RecyclerClickListener;
import com.mapswithme.util.UiUtils;

import java.util.ArrayList;
import java.util.Date;
import java.util.Locale;

class ReviewAdapter extends RecyclerView.Adapter<ReviewAdapter.BaseViewHolder>
{
  private static final int MAX_COUNT = 15;
  private static final int VIEW_TYPE_REVIEW = 0;
  private static final int VIEW_TYPE_MORE = 1;
  private static final int VIEW_TYPE_RATING = 2;

  private final ArrayList<Review> mItems;
  private final RecyclerClickListener mListener;
  private final String mRating;
  private final int mRatingBase;

  ReviewAdapter(ArrayList<Review> images, RecyclerClickListener listener, String rating,
                int ratingBase)
  {
    mItems = images;
    mListener = listener;
    mRating = rating;
    mRatingBase = ratingBase;
  }

  @Override
  public BaseViewHolder onCreateViewHolder(ViewGroup parent, int viewType)
  {
    if (viewType == VIEW_TYPE_REVIEW)
    {
      return new ReviewHolder(LayoutInflater.from(parent.getContext())
                                            .inflate(R.layout.item_comment, parent, false), mListener);
    }

    if (viewType == VIEW_TYPE_MORE)
    {
      return new MoreHolder(LayoutInflater.from(parent.getContext())
                                          .inflate(R.layout.item_more_button, parent, false), mListener);
    }

    return new RatingHolder(LayoutInflater.from(parent.getContext())
                                          .inflate(R.layout.item_rating, parent, false), mListener);
  }

  @Override
  public void onBindViewHolder(BaseViewHolder holder, int position)
  {
    int positionNoHeader = position - 1;

    if (position == 0)
    {
      ((RatingHolder) holder).bind(mRating, mRatingBase);
    } else if (positionNoHeader < mItems.size())
    {
      holder.bind(mItems.get(positionNoHeader), positionNoHeader);
    } else
    {
      holder.bind(null, positionNoHeader);
    }
  }

  @Override
  public int getItemCount()
  {
    if (mItems == null)
    {
      return 1;
    }
    if (mItems.size() > MAX_COUNT)
    {
      return MAX_COUNT + 2;
    }
    return mItems.size() + 2;
  }

  @Override
  public int getItemViewType(int position)
  {
    int positionNoHeader = position - 1;

    if (position == 0)
    {
      return VIEW_TYPE_RATING;
    }
    if (positionNoHeader == mItems.size())
    {
      return VIEW_TYPE_MORE;
    }

    return VIEW_TYPE_REVIEW;
  }

  static abstract class BaseViewHolder extends RecyclerView.ViewHolder implements View.OnClickListener
  {
    private final RecyclerClickListener mListener;
    private int mPosition;

    BaseViewHolder(View itemView, RecyclerClickListener listener)
    {
      super(itemView);
      mListener = listener;
    }

    @Override
    public void onClick(View v)
    {
      if (mListener != null)
      {
        mListener.onItemClick(v, mPosition);
      }
    }

    @CallSuper
    public void bind(Review item, int position)
    {
      mPosition = position;
    }
  }

  private static class ReviewHolder extends BaseViewHolder
  {
    final View mDivider;
    final TextView mUserName;
    final TextView mCommentDate;
    final TextView mRating;
    final TextView mReview;
    final View mPositiveReview;
    final TextView mTvPositiveReview;
    final View mNegativeReview;
    final TextView mTvNegativeReview;

    ReviewHolder(View itemView, RecyclerClickListener listener)
    {
      super(itemView, listener);
      mDivider = itemView.findViewById(R.id.v__divider);
      mUserName = (TextView) itemView.findViewById(R.id.tv__user_name);
      mCommentDate = (TextView) itemView.findViewById(R.id.tv__comment_date);
      mRating = (TextView) itemView.findViewById(R.id.tv__user_rating);
      mReview = (TextView) itemView.findViewById(R.id.tv__review);
      mPositiveReview = itemView.findViewById(R.id.ll__positive_review);
      mTvPositiveReview = (TextView) itemView.findViewById(R.id.tv__positive_review);
      mNegativeReview = itemView.findViewById(R.id.ll__negative_review);
      mTvNegativeReview = (TextView) itemView.findViewById(R.id.tv__negative_review);
    }

    @Override
    public void bind(Review item, int position)
    {
      super.bind(item, position);
      UiUtils.showIf(position > 0, mDivider);
      mUserName.setText(item.getAuthor());
      Date date = new Date(item.getDate());
      mCommentDate.setText(DateFormat.getMediumDateFormat(mCommentDate.getContext()).format(date));
      mRating.setText(String.format(Locale.getDefault(), "%.1f", item.getRating()));
      if (TextUtils.isEmpty(item.getReviewPositive()))
      {
        UiUtils.hide(mPositiveReview);
      } else
      {
        UiUtils.show(mPositiveReview);
        mTvPositiveReview.setText(item.getReviewPositive());
      }
      if (TextUtils.isEmpty(item.getReviewNegative()))
      {
        UiUtils.hide(mNegativeReview);
      } else
      {
        UiUtils.show(mNegativeReview);
        mTvNegativeReview.setText(item.getReviewNegative());
      }
      if (UiUtils.isHidden(mNegativeReview) && UiUtils.isHidden(mPositiveReview))
      {
        UiUtils.showIf(!TextUtils.isEmpty(item.getReview()), mReview);
      } else
      {
        UiUtils.hide(mReview);
      }
    }
  }

  private static class MoreHolder extends BaseViewHolder
  {

    MoreHolder(View itemView, RecyclerClickListener listener)
    {
      super(itemView, listener);
      itemView.setOnClickListener(this);
    }
  }

  private static class RatingHolder extends BaseViewHolder
  {
    final TextView mHotelRating;
    final TextView mHotelRatingBase;

    RatingHolder(View itemView, RecyclerClickListener listener)
    {
      super(itemView, listener);
      mHotelRating = (TextView) itemView.findViewById(R.id.tv__place_hotel_rating);
      mHotelRatingBase = (TextView) itemView.findViewById(R.id.tv__place_hotel_rating_base);
    }

    public void bind(String rating, int ratingBase)
    {
      mHotelRating.setText(rating);
      mHotelRatingBase.setText(mHotelRatingBase.getContext().getResources()
                                               .getQuantityString(R.plurals.place_page_booking_rating_base,
                                                                  ratingBase, ratingBase));
    }
  }
}
