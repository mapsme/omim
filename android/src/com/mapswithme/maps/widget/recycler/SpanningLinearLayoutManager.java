package com.mapswithme.maps.widget.recycler;

import android.content.Context;
import android.support.annotation.NonNull;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.view.ViewGroup;

public class SpanningLinearLayoutManager extends LinearLayoutManager
{
  public SpanningLinearLayoutManager(@NonNull Context context)
  {
    super(context);
  }

  public SpanningLinearLayoutManager(@NonNull Context context, int orientation, boolean reverseLayout)
  {
    super(context, orientation, reverseLayout);
  }

  public SpanningLinearLayoutManager(@NonNull Context context,
                                     @NonNull AttributeSet attrs,
                                     int defStyleAttr, int defStyleRes)
  {
    super(context, attrs, defStyleAttr, defStyleRes);
  }

  @Override
  public RecyclerView.LayoutParams generateDefaultLayoutParams()
  {
    return spanLayoutSize(super.generateDefaultLayoutParams());
  }

  @Override
  public RecyclerView.LayoutParams generateLayoutParams(@NonNull Context c,
                                                        @NonNull AttributeSet attrs)
  {
    return spanLayoutSize(super.generateLayoutParams(c, attrs));
  }

  @Override
  public RecyclerView.LayoutParams generateLayoutParams(@NonNull ViewGroup.LayoutParams lp)
  {
    return spanLayoutSize(super.generateLayoutParams(lp));
  }

  @Override
  public boolean checkLayoutParams(@NonNull RecyclerView.LayoutParams lp)
  {
    return super.checkLayoutParams(lp);
  }

  private RecyclerView.LayoutParams spanLayoutSize(@NonNull RecyclerView.LayoutParams layoutParams)
  {
    if (getOrientation() == HORIZONTAL)
    {
      layoutParams.width = (int) Math.round(getHorizontalSpace() / (double) getItemCount());
    }
    else if (getOrientation() == VERTICAL)
    {
      layoutParams.height = (int) Math.round(getVerticalSpace() / (double) getItemCount());
    }
    return layoutParams;
  }

  @Override
  public boolean canScrollVertically()
  {
    return false;
  }

  @Override
  public boolean canScrollHorizontally()
  {
    return false;
  }

  private int getHorizontalSpace()
  {
    return getWidth() - getPaddingRight() - getPaddingLeft();
  }

  private int getVerticalSpace()
  {
    return getHeight() - getPaddingBottom() - getPaddingTop();
  }
}
