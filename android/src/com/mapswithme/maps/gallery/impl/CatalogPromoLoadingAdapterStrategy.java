package com.mapswithme.maps.gallery.impl;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import com.mapswithme.maps.R;
import com.mapswithme.maps.gallery.Holders;
import com.mapswithme.maps.gallery.ItemSelectedListener;
import com.mapswithme.maps.gallery.Items;

class CatalogPromoLoadingAdapterStrategy extends SimpleLoadingAdapterStrategy
{
  CatalogPromoLoadingAdapterStrategy(@Nullable ItemSelectedListener<Items.Item> listener,
                                     @NonNull String url)
  {
    super(listener, url);
  }

  @NonNull
  @Override
  protected View inflateView(@NonNull LayoutInflater inflater, @NonNull ViewGroup parent)
  {
    return inflater.inflate(R.layout.catalog_promo_placeholder_card, parent, false);
  }

  @Override
  protected Holders.SimpleViewHolder createViewHolder(@NonNull View itemView)
  {
    return new Holders.CrossPromoLoadingHolder(itemView, mItems, getListener());
  }
}
