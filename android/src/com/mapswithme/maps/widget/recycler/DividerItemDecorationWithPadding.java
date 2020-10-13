package com.mapswithme.maps.widget.recycler;

import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.Dimension;
import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;
import com.mapswithme.maps.bookmarks.Holders;

public class DividerItemDecorationWithPadding extends RecyclerView.ItemDecoration
{
  @Dimension
  private final int mStartMargin;
  @NonNull
  private final Drawable mDivider;

  public DividerItemDecorationWithPadding(@NonNull Drawable divider, @Dimension int startMargin)
  {
    mDivider = divider;
    mStartMargin = startMargin;
  }

  @Override
  public void onDrawOver(@NonNull Canvas c, @NonNull RecyclerView parent,
                         @NonNull RecyclerView.State state)
  {
    if (state.isMeasuring())
      return;

    final int right = parent.getWidth();
    final int dividerHeight = mDivider.getIntrinsicHeight();

    final int childCount = parent.getChildCount();
    for (int i = 0; i < childCount; i++)
    {
      View child = parent.getChildAt(i);
      View nextChild = parent.getChildAt(i + 1);
      RecyclerView.ViewHolder viewHolder = parent.getChildViewHolder(child);
      RecyclerView.ViewHolder viewHolderNext = null;
      if (nextChild != null)
        viewHolderNext = parent.getChildViewHolder(nextChild);

      int top = child.getBottom();
      int bottom = top + dividerHeight;

      if (viewHolder instanceof Holders.SectionViewHolder
          || viewHolder instanceof Holders.HeaderViewHolder
          || viewHolderNext instanceof Holders.SectionViewHolder
          || viewHolderNext instanceof Holders.HeaderViewHolder
          || viewHolderNext instanceof Holders.GeneralViewHolder)
        mDivider.setBounds(0, top, right, bottom);
      else if (i == childCount - 1)
        mDivider.setBounds(0, top - dividerHeight, right, bottom);
      else
        mDivider.setBounds(mStartMargin, top, right, bottom);

      mDivider.draw(c);
    }
  }
}
