package com.mapswithme.maps.bookmarks;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.CheckBox;
import android.widget.TextView;

import com.mapswithme.maps.R;
import com.mapswithme.util.UiUtils;

public class Holders
{
  static class GeneralViewHolder extends RecyclerView.ViewHolder
  {

    GeneralViewHolder(@NonNull View itemView)
    {
      super(itemView);
    }
  }

  static class HeaderViewHolder extends RecyclerView.ViewHolder
  {
    @NonNull
    private TextView mButton;

    HeaderViewHolder(@NonNull View itemView)
    {
      super(itemView);
      mButton = itemView.findViewById(R.id.button);
    }

    void setAction(@Nullable HeaderAction action, final boolean showAll)
    {
      mButton.setText(showAll ? R.string.bookmarks_groups_show_all :
                      R.string.bookmarks_groups_hide_all);
      mButton.setOnClickListener
          (v ->
           {
             if (action == null)
               return;

             if (showAll)
               action.onShowAll();
             else
               action.onHideAll();
           });
    }

    public interface HeaderAction
    {
      void onHideAll();
      void onShowAll();
    }
  }

  static class CategoryViewHolder extends RecyclerView.ViewHolder
  {
    @NonNull
    private final TextView mName;
    @NonNull
    CheckBox mVisibilityMarker;
    @NonNull
    TextView mSize;
    @NonNull
    View mMore;

    CategoryViewHolder(@NonNull View root)
    {
      super(root);
      mName = root.findViewById(R.id.name);
      mVisibilityMarker = root.findViewById(R.id.checkbox);
      int left = root.getResources().getDimensionPixelOffset(R.dimen.margin_half_plus);
      int right = root.getResources().getDimensionPixelOffset(R.dimen.margin_base_plus);
      UiUtils.expandTouchAreaForView(mVisibilityMarker, 0, left, 0, right);
      mSize = root.findViewById(R.id.size);
      mMore = root.findViewById(R.id.more);
    }

    void setVisibilityState(boolean visible)
    {
      mVisibilityMarker.setChecked(visible);
    }

    void setVisibilityListener(@Nullable View.OnClickListener listener)
    {
      mVisibilityMarker.setOnClickListener(listener);
    }

    void setMoreListener(@Nullable View.OnClickListener listener)
    {
      mMore.setOnClickListener(listener);
    }

    void setName(@NonNull String name)
    {
      mName.setText(name);
    }

    void setSize(int size)
    {
      mSize.setText(mSize.getResources().getQuantityString(R.plurals.bookmarks_places, size, size));
    }
  }
}
