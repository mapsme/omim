package com.mapswithme.maps.search;

/**
 * Class instances are created from native code.
 */
@SuppressWarnings("unused")
public class SearchResult
{
  public static final int TYPE_SUGGEST = 0;
  public static final int TYPE_RESULT = 1;

  // Values should match osm::YesNoUnknown enum.
  public static final int OPEN_NOW_UNKNOWN = 0;
  public static final int OPEN_NOW_YES = 1;
  public static final int OPEN_NOW_NO = 2;

  public static class Description
  {
    public final String subtitle;
    public final String region;
    public final String distance;
    public final int openNow;

    public Description(String subtitle, String region, String distance, int openNow)
    {
      this.subtitle = subtitle;
      this.region = region;
      this.distance = distance;
      this.openNow = openNow;
    }
  }

  public final String name;
  public final String suggestion;
  public final double lat;
  public final double lon;

  public final int type;
  public final Description description;

  // Consecutive pairs of indexes (each pair contains : start index, length), specifying highlighted matches of original query in result
  public final int[] highlightRanges;

  public SearchResult(String name, String suggestion, double lat, double lon, int[] highlightRanges)
  {
    this.name = name;
    this.suggestion = suggestion;
    this.lat = lat;
    this.lon = lon;
    description = null;
    type = TYPE_SUGGEST;

    this.highlightRanges = highlightRanges;
  }

  public SearchResult(String name, Description description, double lat, double lon, int[] highlightRanges)
  {
    this.name = name;
    suggestion = null;
    this.lat = lat;
    this.lon = lon;
    type = TYPE_RESULT;
    this.description = description;
    this.highlightRanges = highlightRanges;
  }
}