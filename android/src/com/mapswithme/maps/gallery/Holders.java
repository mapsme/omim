package com.mapswithme.maps.gallery;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.ShapeDrawable;
import android.graphics.drawable.shapes.RectShape;
import android.net.Uri;
import android.support.annotation.CallSuper;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawableFactory;
import android.support.v7.widget.RecyclerView;
import android.text.SpannableStringBuilder;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import com.bumptech.glide.Glide;
import com.bumptech.glide.request.target.BitmapImageViewTarget;
import com.mapswithme.HotelUtils;
import com.mapswithme.maps.R;
import com.mapswithme.maps.promo.PromoCityGallery;
import com.mapswithme.maps.promo.PromoEntity;
import com.mapswithme.maps.search.Popularity;
import com.mapswithme.maps.ugc.Impress;
import com.mapswithme.maps.ugc.UGC;
import com.mapswithme.maps.widget.RatingView;
import com.mapswithme.util.ConnectionState;
import com.mapswithme.util.NetworkPolicy;
import com.mapswithme.util.UiUtils;
import com.mapswithme.util.Utils;

import java.util.List;

public class Holders
{
  public static class GenericMoreHolder<T extends RegularAdapterStrategy.Item>
      extends BaseViewHolder<T>
  {

    public GenericMoreHolder(@NonNull View itemView, @NonNull List<T> items,
                             @Nullable ItemSelectedListener<T> listener)
    {
      super(itemView, items, listener);
    }

    @Override
    protected void onItemSelected(@NonNull T item, int position)
    {
      ItemSelectedListener<T> listener = getListener();
      if (listener == null || TextUtils.isEmpty(item.getUrl()))
        return;

      listener.onMoreItemSelected(item);
    }
  }

  public static class SearchMoreHolder extends GenericMoreHolder<Items.SearchItem>
  {

    public SearchMoreHolder(@NonNull View itemView, @NonNull List<Items.SearchItem> items,
                            @Nullable ItemSelectedListener<Items.SearchItem> listener)
    {
      super(itemView, items, listener);
    }

    @Override
    protected void onItemSelected(@NonNull Items.SearchItem item, int position)
    {
      ItemSelectedListener<Items.SearchItem> listener = getListener();
      if (listener != null)
        listener.onMoreItemSelected(item);
    }
  }

  public static class LocalExpertViewHolder extends BaseViewHolder<Items.LocalExpertItem>
  {
    @NonNull
    private final ImageView mAvatar;
    @NonNull
    private final RatingView mRating;
    @NonNull
    private final TextView mButton;

    public LocalExpertViewHolder(@NonNull View itemView, @NonNull List<Items.LocalExpertItem> items,
                                 @Nullable ItemSelectedListener<Items.LocalExpertItem> listener)
    {
      super(itemView, items, listener);
      mAvatar = (ImageView) itemView.findViewById(R.id.avatar);
      mRating = (RatingView) itemView.findViewById(R.id.ratingView);
      mButton = (TextView) itemView.findViewById(R.id.button);
    }

    @Override
    public void bind(@NonNull Items.LocalExpertItem item)
    {
      super.bind(item);

      Glide.with(mAvatar.getContext())
           .load(item.getPhotoUrl())
           .asBitmap()
           .centerCrop()
           .placeholder(R.drawable.ic_local_expert_default)
           .into(new BitmapImageViewTarget(mAvatar)
           {
             @Override
             protected void setResource(Bitmap resource)
             {
               RoundedBitmapDrawable circularBitmapDrawable =
                   RoundedBitmapDrawableFactory.create(mAvatar.getContext().getResources(),
                                                       resource);
               circularBitmapDrawable.setCircular(true);
               mAvatar.setImageDrawable(circularBitmapDrawable);
             }
           });

      Context context = mButton.getContext();
      String priceLabel;
      if (item.getPrice() == 0 && TextUtils.isEmpty(item.getCurrency()))
      {
        priceLabel = context.getString(R.string.free);
      }
      else
      {
        String formattedPrice = Utils.formatCurrencyString(String.valueOf(item.getPrice()),
                                                           item.getCurrency());
        priceLabel = context.getString(R.string.price_per_hour, formattedPrice);
      }
      UiUtils.setTextAndHideIfEmpty(mButton, priceLabel);
      float rating = (float) item.getRating();
      Impress impress = Impress.values()[UGC.nativeToImpress(rating)];
      mRating.setRating(impress, UGC.nativeFormatRating(rating));
    }
  }

  public static abstract class ActionButtonViewHolder<T extends RegularAdapterStrategy.Item>
      extends BaseViewHolder<T>
  {
    @NonNull
    private final TextView mButton;

    ActionButtonViewHolder(@NonNull View itemView, @NonNull List<T> items,
                           @Nullable ItemSelectedListener<T> listener)
    {
      super(itemView, items, listener);
      mButton = itemView.findViewById(R.id.button);
      mButton.setOnClickListener(this);
      itemView.findViewById(R.id.infoLayout).setOnClickListener(this);
      mButton.setText(R.string.p2p_to_here);
    }

    @Override
    public void onClick(View v)
    {
      int position = getAdapterPosition();
      if (position == RecyclerView.NO_POSITION || mItems.isEmpty())
        return;

      ItemSelectedListener<T> listener = getListener();
      if (listener == null)
        return;

      T item = mItems.get(position);
      switch (v.getId())
      {
        case R.id.infoLayout:
          listener.onItemSelected(item, position);
          break;
        case R.id.button:
          listener.onActionButtonSelected(item, position);
          break;
      }
    }
  }

  public static final class SearchViewHolder extends ActionButtonViewHolder<Items.SearchItem>
  {
    @NonNull
    private final TextView mSubtitle;
    @NonNull
    private final TextView mDistance;
    @NonNull
    private final RatingView mNumberRating;
    @NonNull
    private final RatingView mPopularTagRating;

    public SearchViewHolder(@NonNull View itemView, @NonNull List<Items.SearchItem> items,
                            @Nullable ItemSelectedListener<Items.SearchItem> adapter)
    {
      super(itemView, items, adapter);
      mSubtitle = itemView.findViewById(R.id.subtitle);
      mDistance = itemView.findViewById(R.id.distance);
      mNumberRating = itemView.findViewById(R.id.counter_rating_view);
      mPopularTagRating = itemView.findViewById(R.id.popular_rating_view);
    }

    @Override
    public void bind(@NonNull Items.SearchItem item)
    {
      super.bind(item);

      String featureType = item.getFeatureType();
      String localizedType = Utils.getLocalizedFeatureType(mSubtitle.getContext(), featureType);
      String title = TextUtils.isEmpty(item.getTitle()) ? localizedType : item.getTitle();

      UiUtils.setTextAndHideIfEmpty(getTitle(), title);
      UiUtils.setTextAndHideIfEmpty(mSubtitle, localizedType);
      UiUtils.setTextAndHideIfEmpty(mDistance, item.getDistance());
      UiUtils.showIf(item.getPopularity().getType() == Popularity.Type.POPULAR, mPopularTagRating);

      float rating = item.getRating();
      Impress impress = Impress.values()[UGC.nativeToImpress(rating)];
      mNumberRating.setRating(impress, UGC.nativeFormatRating(rating));
    }
  }

  public static final class HotelViewHolder extends ActionButtonViewHolder<Items.SearchItem>
  {
    @NonNull
    private final TextView mTitle;
    @NonNull
    private final TextView mSubtitle;
    @NonNull
    private final RatingView mRatingView;
    @NonNull
    private final TextView mDistance;

    public HotelViewHolder(@NonNull View itemView, @NonNull List<Items.SearchItem> items, @Nullable
        ItemSelectedListener<Items.SearchItem> listener)
    {
      super(itemView, items, listener);
      mTitle = itemView.findViewById(R.id.title);
      mSubtitle = itemView.findViewById(R.id.subtitle);
      mRatingView = itemView.findViewById(R.id.ratingView);
      mDistance = itemView.findViewById(R.id.distance);
    }

    @Override
    public void bind(@NonNull Items.SearchItem item)
    {
      String featureType = item.getFeatureType();
      String localizedType = Utils.getLocalizedFeatureType(mSubtitle.getContext(), featureType);
      String title = TextUtils.isEmpty(item.getTitle()) ? localizedType : item.getTitle();

      UiUtils.setTextAndHideIfEmpty(mTitle, title);
      UiUtils.setTextAndHideIfEmpty(mSubtitle, formatDescription(item.getStars(),
                                                                 localizedType,
                                                                 item.getPrice(),
                                                                 mSubtitle.getResources()));

      float rating = item.getRating();
      Impress impress = Impress.values()[UGC.nativeToImpress(rating)];
      mRatingView.setRating(impress, UGC.nativeFormatRating(rating));
      UiUtils.setTextAndHideIfEmpty(mDistance, item.getDistance());
    }

    @NonNull
    private static CharSequence formatDescription(int stars, @Nullable String type,
                                                  @Nullable String priceCategory,
                                                  @NonNull Resources res)
    {
      final SpannableStringBuilder sb = new SpannableStringBuilder();
      if (stars > 0)
        sb.append(HotelUtils.formatStars(stars, res));
      else if (!TextUtils.isEmpty(type))
        sb.append(type);

      if (!TextUtils.isEmpty(priceCategory))
      {
        sb.append(" • ");
        sb.append(priceCategory);
      }

      return sb;
    }
  }

  public static class BaseViewHolder<I extends Items.Item> extends RecyclerView.ViewHolder
      implements View.OnClickListener
  {
    @NonNull
    private final TextView mTitle;
    @Nullable
    private final ItemSelectedListener<I> mListener;
    @NonNull
    protected final List<I> mItems;

    public BaseViewHolder(@NonNull View itemView, @NonNull List<I> items,
                          @Nullable ItemSelectedListener<I> listener)
    {
      super(itemView);
      mTitle = itemView.findViewById(R.id.title);
      mListener = listener;
      itemView.setOnClickListener(this);
      mItems = items;
    }

    public void bind(@NonNull I item)
    {
      mTitle.setText(item.getTitle());
    }

    @Override
    public void onClick(View v)
    {
      int position = getAdapterPosition();
      if (position == RecyclerView.NO_POSITION || mItems.isEmpty())
        return;

      onItemSelected(mItems.get(position), position);
    }

    @NonNull
    protected TextView getTitle()
    {
      return mTitle;
    }

    protected void onItemSelected(@NonNull I item, int position)
    {
      ItemSelectedListener<I> listener = getListener();
      if (listener == null || TextUtils.isEmpty(item.getUrl()))
        return;

      listener.onItemSelected(item, position);
    }

    @Nullable
    protected ItemSelectedListener<I> getListener()
    {
      return mListener;
    }
  }

  public static class LoadingViewHolder extends BaseViewHolder<Items.Item>
      implements View.OnClickListener
  {
    @NonNull
    final ProgressBar mProgressBar;
    @NonNull
    final TextView mSubtitle;

    LoadingViewHolder(@NonNull View itemView, @NonNull List<Items.Item> items,
                      @Nullable ItemSelectedListener<Items.Item> listener)
    {
      super(itemView, items, listener);
      mProgressBar = (ProgressBar) itemView.findViewById(R.id.pb__progress);
      mSubtitle = (TextView) itemView.findViewById(R.id.tv__subtitle);
    }

    @CallSuper
    @Override
    public void bind(@NonNull Items.Item item)
    {
      super.bind(item);
      UiUtils.setTextAndHideIfEmpty(mSubtitle, item.getSubtitle());
    }

    @Override
    public void onClick(View v)
    {
      int position = getAdapterPosition();
      if (position == RecyclerView.NO_POSITION)
        return;

      onItemSelected(mItems.get(position), position);
    }

    @Override
    protected void onItemSelected(@NonNull Items.Item item, int position)
    {
      if (getListener() == null || TextUtils.isEmpty(item.getUrl()))
        return;

      getListener().onActionButtonSelected(item, position);
    }
  }

  public static class SimpleViewHolder extends BaseViewHolder<Items.Item>
  {
    public SimpleViewHolder(@NonNull View itemView, @NonNull List<Items.Item> items,
                            @Nullable ItemSelectedListener<Items.Item> listener)
    {
      super(itemView, items, listener);
    }
  }

  static class ErrorViewHolder extends LoadingViewHolder
  {

    ErrorViewHolder(@NonNull View itemView, @NonNull List<Items.Item> items,
                    @Nullable ItemSelectedListener<Items.Item> listener)
    {
      super(itemView, items, listener);
      UiUtils.hide(mProgressBar);
    }
  }

  public static class OfflineViewHolder extends LoadingViewHolder
  {
    OfflineViewHolder(@NonNull View itemView, @NonNull List<Items.Item> items,
                      @Nullable ItemSelectedListener<Items.Item> listener)
    {
      super(itemView, items, listener);
      UiUtils.hide(mProgressBar);
    }

    @CallSuper
    @Override
    public void bind(@NonNull Items.Item item)
    {
      super.bind(item);
      UiUtils.setTextAndHideIfEmpty(mSubtitle, item.getSubtitle());
    }

    @Override
    protected void onItemSelected(@NonNull Items.Item item, int position)
    {

    }
  }

  public static class CatalogPromoHolder extends BaseViewHolder<PromoEntity>
  {
    @NonNull
    private final ImageView mImage;

    @NonNull
    private final TextView mSubTitle;

    @NonNull
    private final TextView mProLabel;

    public CatalogPromoHolder(@NonNull View itemView,
                              @NonNull List<PromoEntity> items,
                              @Nullable ItemSelectedListener<PromoEntity> listener)
    {
      super(itemView, items, listener);
      mImage = itemView.findViewById(R.id.image);
      mSubTitle = itemView.findViewById(R.id.subtitle);
      mProLabel = itemView.findViewById(R.id.label);
    }

    @Override
    public void bind(@NonNull PromoEntity item)
    {
      super.bind(item);

      bindProLabel(item);
      bindSubTitle(item);
      bindImage(item);
    }

    private void bindSubTitle(@NonNull PromoEntity item)
    {
      mSubTitle.setText(item.getSubtitle());
    }

    private void bindImage(@NonNull PromoEntity item)
    {
      Glide.with(itemView.getContext())
           .load(Uri.parse(item.getImageUrl()))
           .placeholder(R.drawable.img_guides_gallery_placeholder)
           .into(mImage);
    }

    private void bindProLabel(@NonNull PromoEntity item)
    {
      PromoCityGallery.LuxCategory category = item.getCategory();
      UiUtils.showIf(category != null && !TextUtils.isEmpty(category.getName()), mProLabel);
      if (item.getCategory() == null)
        return;

      mProLabel.setText(item.getCategory().getName());
      ShapeDrawable shapeDrawable = new ShapeDrawable(new RectShape());
      shapeDrawable.getPaint().setColor(item.getCategory().getColor());
      mProLabel.setBackgroundDrawable(shapeDrawable);
    }
  }

  public static class CrossPromoLoadingHolder extends SimpleViewHolder
  {
    @NonNull
    private final TextView mSubTitle;

    @NonNull
    private final TextView mButton;

    public CrossPromoLoadingHolder(@NonNull View itemView, @NonNull List<Items.Item> items,
                                   @Nullable ItemSelectedListener<Items.Item> listener)
    {
      super(itemView, items, listener);
      mSubTitle = itemView.findViewById(R.id.subtitle);
      mButton = itemView.findViewById(R.id.button);
    }

    @NonNull
    protected TextView getButton()
    {
      return mButton;
    }

    @Override
    public void bind(@NonNull Items.Item item)
    {
      super.bind(item);
      getTitle().setText(R.string.gallery_pp_download_guides_offline_title);
      mSubTitle.setText(R.string.gallery_pp_download_guides_offline_subtitle);
      UiUtils.invisible(getButton());
    }
  }

  public static class CatalogErrorHolder extends CrossPromoLoadingHolder
  {
    public CatalogErrorHolder(@NonNull View itemView, @NonNull List<Items.Item> items,
                              @Nullable ItemSelectedListener<Items.Item> listener)
    {
      super(itemView, items, listener);
      View progress = itemView.findViewById(R.id.progress);
      UiUtils.invisible(progress);
    }

    public void bind(@NonNull Items.Item item)
    {
      super.bind(item);
      getButton().setText(R.string.gallery_pp_download_guides_offline_cta);
      boolean isBtnInvisible = ConnectionState.isConnected() &&
                               NetworkPolicy.newInstance(NetworkPolicy.getCurrentNetworkUsageStatus()).canUseNetwork();

      if (isBtnInvisible)
        UiUtils.invisible(getButton());
      else
        UiUtils.show(getButton());
    }

    @Override
    protected void onItemSelected(@NonNull Items.Item item, int position)
    {
      ItemSelectedListener<Items.Item> listener = getListener();
      if (listener == null)
        return;

      listener.onItemSelected(item, position);
    }
  }
}
