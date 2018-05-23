package com.mapswithme.maps.routing;

import android.annotation.SuppressLint;
import android.app.Dialog;
import android.content.DialogInterface;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v7.app.AlertDialog;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.widget.ExpandableListAdapter;
import android.widget.ExpandableListView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import com.mapswithme.maps.R;
import com.mapswithme.maps.adapter.DisabledChildSimpleExpandableListAdapter;
import com.mapswithme.maps.base.BaseMwmDialogFragment;
import com.mapswithme.maps.downloader.CountryItem;
import com.mapswithme.maps.downloader.MapManager;
import com.mapswithme.util.StringUtils;
import com.mapswithme.util.UiUtils;

abstract class BaseRoutingErrorDialogFragment extends BaseMwmDialogFragment
{
  static final String EXTRA_MISSING_MAPS = "MissingMaps";

  private static final String GROUP_NAME = "GroupName";
  private static final String GROUP_SIZE = "GroupSize";
  private static final String COUNTRY_NAME = "CountryName";

  final List<CountryItem> mMissingMaps = new ArrayList<>();
  String[] mMapsArray;

  private boolean mCancelRoute = true;
  boolean mCancelled;

  void beforeDialogCreated(AlertDialog.Builder builder) {}
  void bindGroup(View view) {}

  private Dialog createDialog(AlertDialog.Builder builder)
  {
    View view = (mMissingMaps.size() == 1 ? buildSingleMapView(mMissingMaps.get(0))
                                          : buildMultipleMapView());
    builder.setView(view);
    return builder.create();
  }

  @NonNull
  @Override
  public Dialog onCreateDialog(Bundle savedInstanceState)
  {
    parseArguments();
    AlertDialog.Builder builder = new AlertDialog.Builder(getActivity())
                                                 .setCancelable(true)
                                                 .setNegativeButton(android.R.string.cancel, null);
    beforeDialogCreated(builder);
    return createDialog(builder);
  }

  @Override
  public void onStart()
  {
    super.onStart();

    AlertDialog dlg = (AlertDialog) getDialog();
    dlg.getButton(DialogInterface.BUTTON_NEGATIVE).setOnClickListener(new View.OnClickListener()
    {
      @Override
      public void onClick(View v)
      {
        mCancelled = true;
        dismiss();
      }
    });
  }

  @Override
  public void onDismiss(DialogInterface dialog)
  {
    if (mCancelled && mCancelRoute)
      RoutingController.get().cancel();

    super.onDismiss(dialog);
  }

  void parseArguments()
  {
    Bundle args = getArguments();
    mMapsArray = args.getStringArray(EXTRA_MISSING_MAPS);
    for (String map : mMapsArray)
      mMissingMaps.add(CountryItem.fill(map));
  }

  View buildSingleMapView(CountryItem map)
  {
    @SuppressLint("InflateParams")
    final View countryView = View.inflate(getActivity(), R.layout.dialog_missed_map, null);
    ((TextView) countryView.findViewById(R.id.tv__title)).setText(map.name);

    final TextView szView = (TextView) countryView.findViewById(R.id.tv__size);
    szView.setText(MapManager.nativeIsLegacyMode() ? "" : StringUtils.getFileSizeString(map.totalSize));
    ViewGroup.MarginLayoutParams lp = (ViewGroup.MarginLayoutParams) szView.getLayoutParams();
    lp.rightMargin = 0;
    szView.setLayoutParams(lp);

    return countryView;
  }

  View buildMultipleMapView()
  {
    @SuppressLint("InflateParams")
    final View countriesView = View.inflate(getActivity(), R.layout.dialog_missed_maps, null);

    final ExpandableListView listView = (ExpandableListView) countriesView.findViewById(R.id.items_frame);
    if (mMissingMaps.isEmpty())
    {
      mCancelRoute = false;

      UiUtils.hide(listView);
      UiUtils.hide(countriesView.findViewById(R.id.divider_top));
      UiUtils.hide(countriesView.findViewById(R.id.divider_bottom));
      return countriesView;
    }

    listView.setAdapter(buildAdapter());
    listView.setChildDivider(new ColorDrawable(getResources().getColor(android.R.color.transparent)));

    UiUtils.waitLayout(listView, new ViewTreeObserver.OnGlobalLayoutListener()
    {
      @Override
      public void onGlobalLayout()
      {
        final int width = listView.getWidth();
        final int indicatorWidth = UiUtils.dimen(R.dimen.margin_quadruple);
        listView.setIndicatorBounds(width - indicatorWidth, width);
        if (Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.JELLY_BEAN_MR2)
          listView.setIndicatorBounds(width - indicatorWidth, width);
        else
          listView.setIndicatorBoundsRelative(width - indicatorWidth, width);
      }
    });

    return countriesView;
  }

  private ExpandableListAdapter buildAdapter()
  {
    List<Map<String, String>> countries = new ArrayList<>();
    long size = 0;
    boolean legacy = MapManager.nativeIsLegacyMode();

    for (CountryItem item: mMissingMaps)
    {
      Map<String, String> data = new HashMap<>();
      data.put(COUNTRY_NAME, item.name);
      countries.add(data);

      if (!legacy)
        size += item.totalSize;
    }

    Map<String, String> group = new HashMap<>();
    group.put(GROUP_NAME, getString(R.string.maps) + " (" + mMissingMaps.size() + ") ");
    group.put(GROUP_SIZE, (legacy ? "" : StringUtils.getFileSizeString(size)));

    List<Map<String, String>> groups = new ArrayList<>();
    groups.add(group);

    List<List<Map<String, String>>> children = new ArrayList<>();
    children.add(countries);

    return new DisabledChildSimpleExpandableListAdapter(getActivity(),
                                                        groups,
                                                        R.layout.item_missed_map_group,
                                                        R.layout.item_missed_map,
                                                        new String[] { GROUP_NAME, GROUP_SIZE },
                                                        new int[] { R.id.tv__title, R.id.tv__size },
                                                        children,
                                                        R.layout.item_missed_map,
                                                        new String[] { COUNTRY_NAME },
                                                        new int[] { R.id.tv__title })
    {
      @Override
      public View getGroupView(int groupPosition, boolean isExpanded, View convertView, ViewGroup parent)
      {
        View res = super.getGroupView(groupPosition, isExpanded, convertView, parent);
        bindGroup(res);
        return res;
      }
    };
  }
}
