package com.mapswithme.maps.gallery;

import com.bumptech.glide.Glide;
import com.mapswithme.maps.R;
import com.mapswithme.maps.widget.recycler.RecyclerClickListener;

import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import java.util.ArrayList;

class ImageAdapter extends RecyclerView.Adapter<ImageAdapter.ViewHolder> {
  private final ArrayList<Image> mItems;
  private final RecyclerClickListener mListener;

  ImageAdapter(ArrayList<Image> images, RecyclerClickListener listener) {
    mItems = images;
    mListener = listener;
  }

  @Override
  public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
    return new ViewHolder(LayoutInflater.from(parent.getContext())
            .inflate(R.layout.item_image, parent, false), mListener);
  }

  @Override
  public void onBindViewHolder(ViewHolder holder, int position) {
    holder.bind(mItems.get(position), position);
  }

  @Override
  public int getItemCount() {
    if (mItems == null) {
      return 0;
    }
    return mItems.size();
  }

  static class ViewHolder extends RecyclerView.ViewHolder implements View.OnClickListener {
    private ImageView mImage;
    private final RecyclerClickListener mListener;
    private int mPosition;

    public ViewHolder(View itemView, RecyclerClickListener listener) {
      super(itemView);
      mListener = listener;
      itemView.setOnClickListener(this);
      mImage = (ImageView) itemView.findViewById(R.id.iv__image);
    }

    @Override
    public void onClick(View v) {
      if (mListener != null) {
        mListener.onItemClick(v, mPosition);
      }
    }

    public void bind(Image image, int position) {
      mPosition = position;
      Glide.with(mImage.getContext())
              .load(image.getSmallUrl())
              .centerCrop()
              .into(mImage);
    }
  }
}
