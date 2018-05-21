package com.mapswithme.maps.bookmarks;

import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.design.widget.TabLayout;
import android.support.v4.app.FragmentManager;
import android.support.v4.view.ViewPager;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import com.mapswithme.maps.R;
import com.mapswithme.maps.base.BaseMwmFragment;

import java.util.Arrays;
import java.util.List;

public class BookmarkCategoriesPagerFragment extends BaseMwmFragment
{

  @NonNull
  private List<BookmarksPageFactory> mFactories;

  @Override
  public void onCreate(@Nullable Bundle savedInstanceState)
  {
    super.onCreate(savedInstanceState);
    mFactories = onPrepareAdapterDataSet();
  }

  private List<BookmarksPageFactory> onPrepareAdapterDataSet()
  {
    return Arrays.asList(BookmarksPageFactory.OWNED, BookmarksPageFactory.CATALOG);
  }

  @Nullable
  @Override
  public View onCreateView(LayoutInflater inflater,
                           @Nullable ViewGroup container,
                           @Nullable Bundle savedInstanceState)
  {
    View root = inflater.inflate(R.layout.bookmark_categories_pager_frag, null);
    ViewPager viewPager = root.findViewById(R.id.viewpager);
    TabLayout tabLayout = root.findViewById(R.id.sliding_tabs_layout);

    FragmentManager fm = getActivity().getSupportFragmentManager();
    BookmarksPagerAdapter adapter = new BookmarksPagerAdapter(getContext(),
                                                              fm,
                                                              mFactories);
    viewPager.setAdapter(adapter);
    tabLayout.setupWithViewPager(viewPager);
    return root;
  }
}
