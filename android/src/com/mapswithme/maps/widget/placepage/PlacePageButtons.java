
package com.mapswithme.maps.widget.placepage;

import android.support.annotation.DrawableRes;
import android.support.annotation.NonNull;
import android.support.annotation.StringRes;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import com.mapswithme.maps.MwmApplication;
import com.mapswithme.maps.R;
import com.mapswithme.maps.routing.RoutingController;
import com.mapswithme.util.BottomSheetHelper;
import com.mapswithme.util.ThemeUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public final class PlacePageButtons
{
  private static final Map<Integer, PartnerItem> PARTNERS_ITEMS = new HashMap<Integer, PartnerItem>()
  {{
    put(PartnerItem.PARTNER1.getIndex(), PartnerItem.PARTNER1);
    put(PartnerItem.PARTNER3.getIndex(), PartnerItem.PARTNER3);
  }};

  private final int mMaxButtons;

  private final PlacePageView mPlacePage;
  private final ViewGroup mFrame;
  private final ItemListener mItemListener;

  private List<PlacePageButton> mPrevItems;

  interface PlacePageButton
  {
    @StringRes
    int getTitle();

    ImageResources getIcon();

    @NonNull
    ButtonType getType();

    class ImageResources
    {

      @DrawableRes
      private final int mEnabledStateResId;
      @DrawableRes
      private final int mDisabledStateResId;

      public ImageResources(int enabledStateResId,
                            int disabledStateResId)
      {
        mEnabledStateResId = enabledStateResId;
        mDisabledStateResId = disabledStateResId;
      }

      public ImageResources(int enabledStateResId)
      {
        this(enabledStateResId, enabledStateResId);
      }

      @DrawableRes
      public int getDisabledStateResId()
      {
        return mDisabledStateResId;
      }

      @DrawableRes
      public int getEnabledStateResId()
      {
        return mEnabledStateResId;
      }

      public static class Stub extends ImageResources
      {
        private static final int STUB_RES_ID = -1;

        public Stub()
        {
          super(STUB_RES_ID);
        }

        @Override
        public int getDisabledStateResId()
        {
          throw new UnsupportedOperationException("not supported here");
        }

        @Override
        public int getEnabledStateResId()
        {
          throw new UnsupportedOperationException("not supported here");
        }
      }
    }
  }

  enum ButtonType
  {
    PARTNER1,
    PARTNER3,
    BOOKING,
    BOOKING_SEARCH,
    OPENTABLE,
    BACK,
    BOOKMARK,
    ROUTE_FROM,
    ROUTE_TO,
    ROUTE_ADD,
    ROUTE_REMOVE,
    SHARE,
    MORE,
    CALL
  }

  private enum PartnerItem implements PlacePageButton
  {
    PARTNER1(1, new ImageResources(R.drawable.ic_24px_logo_partner1))
    {
      @Override
      public int getTitle()
      {
        return R.string.sponsored_partner1_action;
      }

      @Override
      @NonNull
      public ButtonType getType()
      {
        return ButtonType.PARTNER1;
      }
    },

    PARTNER3(3, new ImageResources(R.drawable.ic_24px_logo_partner3))
    {
      @Override
      public int getTitle()
      {
        return R.string.sponsored_partner3_action;
      }

      @Override
      @NonNull
      public ButtonType getType()
      {
        return ButtonType.PARTNER3;
      }
    };

    private final int mIndex;
    @NonNull
    private ImageResources mImageResources;

    PartnerItem(int index, @NonNull ImageResources imageResources)
    {
      mIndex = index;
      mImageResources = imageResources;
    }

    public int getIndex()
    {
      return mIndex;
    }

    @Override
    public ImageResources getIcon()
    {
      return mImageResources;
    }
  }

  enum Item implements PlacePageButton
  {
    BOOKING(new ImageResources(R.drawable.ic_booking))
    {
      @Override
      public int getTitle()
      {
        return R.string.book_button;
      }

      @Override
      @NonNull
      public ButtonType getType()
      {
        return ButtonType.BOOKING;
      }
    },

    BOOKING_SEARCH(new ImageResources(R.drawable.ic_menu_search))
    {
      @Override
      public int getTitle()
      {
        return R.string.booking_search;
      }

      @NonNull
      @Override
      public ButtonType getType()
      {
        return ButtonType.BOOKING_SEARCH;
      }
    },

    OPENTABLE(new ImageResources(R.drawable.ic_opentable))
    {
      @Override
      public int getTitle()
      {
        return R.string.book_button;
      }

      @NonNull
      @Override
      public ButtonType getType()
      {
        return ButtonType.OPENTABLE;
      }
    },

    BACK(new ImageResources.Stub())
    {
      @Override
      public int getTitle()
      {
        return R.string.back;
      }

      @Override
      public ImageResources getIcon()
      {
        return new ImageResources(ThemeUtils.getResource(MwmApplication.get(), android.R.attr.homeAsUpIndicator));
      }

      @NonNull
      @Override
      public ButtonType getType()
      {
        return ButtonType.BACK;
      }
    },

    BOOKMARK(new ImageResources(R.drawable.ic_bookmarks_off, R.drawable.bookmark_disabled))
    {
      @Override
      public int getTitle()
      {
        return R.string.bookmark;
      }

      @NonNull
      @Override
      public ButtonType getType()
      {
        return ButtonType.BOOKMARK;
      }
    },

    ROUTE_FROM(new ImageResources(R.drawable.ic_route_from))
    {
      @Override
      public int getTitle()
      {
        return R.string.p2p_from_here;
      }

      @NonNull
      @Override
      public ButtonType getType()
      {
        return ButtonType.ROUTE_FROM;
      }
    },

    ROUTE_TO(new ImageResources(R.drawable.ic_route_to))
    {
      @Override
      public int getTitle()
      {
        return R.string.p2p_to_here;
      }

      @NonNull
      @Override
      public ButtonType getType()
      {
        return ButtonType.ROUTE_TO;
      }
    },

    ROUTE_ADD(new ImageResources(R.drawable.ic_route_via))
    {
      @Override
      public int getTitle()
      {
        return R.string.placepage_add_stop;
      }

      @NonNull
      @Override
      public ButtonType getType()
      {
        return ButtonType.ROUTE_ADD;
      }
    },

    ROUTE_REMOVE(new ImageResources(R.drawable.ic_route_remove))
    {
      @Override
      public int getTitle()
      {
        return R.string.placepage_remove_stop;
      }

      @NonNull
      @Override
      public ButtonType getType()
      {
        return ButtonType.ROUTE_REMOVE;
      }
    },

    SHARE(new ImageResources(R.drawable.ic_share))
    {
      @Override
      public int getTitle()
      {
        return R.string.share;
      }

      @NonNull
      @Override
      public ButtonType getType()
      {
        return ButtonType.SHARE;
      }
    },

    // Must not be used outside
    MORE(new ImageResources(R.drawable.bs_ic_more))
    {
      @Override
      public int getTitle()
      {
        return R.string.placepage_more_button;
      }

      @NonNull
      @Override
      public ButtonType getType()
      {
        return ButtonType.MORE;
      }
    },

    CALL(new ImageResources(R.drawable.ic_place_page_phone))
    {
      @Override
      public int getTitle()
      {
        return R.string.placepage_call_button;
      }

      @NonNull
      @Override
      public ButtonType getType()
      {
        return ButtonType.CALL;
      }
    };

    private ImageResources mImageResources;

    Item(@NonNull ImageResources imageResources)
    {
      mImageResources = imageResources;
    }

    @StringRes
    public int getTitle()
    {
      throw new UnsupportedOperationException("Not supported!");
    }

    @NonNull
    public ImageResources getIcon()
    {
      return mImageResources;
    }
  }

  interface ItemListener
  {
    void onPrepareVisibleView(PlacePageButton item, View frame, ImageView icon, TextView title);
    void onItemClick(PlacePageButton item);
  }

  PlacePageButtons(PlacePageView placePage, ViewGroup frame, ItemListener itemListener)
  {
    mPlacePage = placePage;
    mFrame = frame;
    mItemListener = itemListener;

    mMaxButtons = mPlacePage.getContext().getResources().getInteger(R.integer.pp_buttons_max);
  }

  @NonNull
  static PlacePageButton getPartnerItem(int partnerIndex)
  {
    PlacePageButton item = PARTNERS_ITEMS.get(partnerIndex);
    if (item == null)
      throw new AssertionError("Wrong partner index: " + partnerIndex);
    return item;
  }

  private @NonNull List<PlacePageButton> collectButtons(List<PlacePageButton> items)
  {
    List<PlacePageButton> res = new ArrayList<>(items);
    if (res.size() > mMaxButtons)
      res.add(mMaxButtons - 1, Item.MORE);

    // Swap ROUTE_FROM and ROUTE_TO if the latter one was pressed out to bottomsheet
    int from = res.indexOf(Item.ROUTE_FROM);
    if (from > -1)
    {
      int addStop = res.indexOf(Item.ROUTE_ADD);
      int to = res.indexOf(Item.ROUTE_TO);
      if ((to > from && to >= mMaxButtons) || (to > from && addStop >= mMaxButtons))
        Collections.swap(res, from, to);

      if (addStop >= mMaxButtons)
      {
        from = res.indexOf(Item.ROUTE_FROM);
        if (addStop > from)
          Collections.swap(res, from, addStop);
      }

      preserveRoutingButtons(res, Item.CALL);
      preserveRoutingButtons(res, Item.BOOKING);
      preserveRoutingButtons(res, Item.BOOKING_SEARCH);
      from = res.indexOf(Item.ROUTE_FROM);
      to = res.indexOf(Item.ROUTE_TO);
      if (from < mMaxButtons && from > to)
        Collections.swap(res, to, from);
    }

    return res;
  }

  private void preserveRoutingButtons(@NonNull List<PlacePageButton> items, @NonNull Item itemToShift)
  {
    if (!RoutingController.get().isNavigating() && !RoutingController.get().isPlanning())
      return;

    int pos = items.indexOf(itemToShift);
    if (pos > -1)
    {
      items.remove(pos);
      items.add(mMaxButtons, itemToShift);
      int to = items.indexOf(Item.ROUTE_TO);
      if (items.indexOf(Item.ROUTE_ADD) > -1)
      {
        items.remove(Item.ROUTE_ADD);
        items.remove(Item.ROUTE_FROM);
        items.add(to + 1, Item.ROUTE_ADD);
        items.add(mMaxButtons, Item.ROUTE_FROM);
      }
      else
      {
        items.remove(Item.ROUTE_FROM);
        items.add(to + 1, Item.ROUTE_FROM);
      }
    }
  }

  private void showPopup(final List<PlacePageButton> buttons)
  {
    BottomSheetHelper.Builder bs = new BottomSheetHelper.Builder(mPlacePage.getActivity());
    for (int i = mMaxButtons; i < buttons.size(); i++)
    {
      PlacePageButton bsItem = buttons.get(i);
      int iconRes = bsItem.getIcon().getEnabledStateResId();
      bs.sheet(i, iconRes, bsItem.getTitle());
    }

    bs.listener(new MenuItem.OnMenuItemClickListener()
    {
      @Override
      public boolean onMenuItemClick(MenuItem item)
      {
        mItemListener.onItemClick(buttons.get(item.getItemId()));
        return true;
      }
    });

    bs.tint().show();
  }

  private View createButton(final List<PlacePageButton> items, final PlacePageButton current)
  {
    LayoutInflater inflater = LayoutInflater.from(mPlacePage.getContext());
    View parent = inflater.inflate(R.layout.place_page_button, mFrame, false);

    ImageView icon = (ImageView) parent.findViewById(R.id.icon);
    TextView title = (TextView) parent.findViewById(R.id.title);

    title.setText(current.getTitle());
    icon.setImageResource(current.getIcon().getEnabledStateResId());
    mItemListener.onPrepareVisibleView(current, parent, icon, title);
    parent.setOnClickListener(new ShowPopupClickListener(current, items));
    return parent;
  }

  void setItems(List<PlacePageButton> items)
  {
    final List<PlacePageButton> buttons = collectButtons(items);
    if (buttons.equals(mPrevItems))
      return;

    mFrame.removeAllViews();
    int count = Math.min(buttons.size(), mMaxButtons);
    for (int i = 0; i < count; i++)
      mFrame.addView(createButton(buttons, buttons.get(i)));

    mPrevItems = buttons;
  }

  private class ShowPopupClickListener implements View.OnClickListener
  {
    private final PlacePageButton mCurrent;
    private final List<PlacePageButton> mItems;

    public ShowPopupClickListener(PlacePageButton current, List<PlacePageButton> items)
    {
      mCurrent = current;
      mItems = items;
    }

    @Override
    public void onClick(View v)
    {
      if (mCurrent == Item.MORE)
        showPopup(mItems);
      else
        mItemListener.onItemClick(mCurrent);
    }
  }
}
