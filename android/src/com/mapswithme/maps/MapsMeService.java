package com.mapswithme.maps;

import android.graphics.Bitmap;

/*
 * MapsMe service
 * It is the entry point to the MapsMe services
 */
public class MapsMeService
{
    /*
     * Render service
     * Required to render an extent of the map to Bitmap
     */
    public static class RenderService
    {
        public interface Listener
        {
            void onRenderComplete(Bitmap bitmap);
        }

        /*
         * Add/Remove event listeners
         */
        public native static void addListener(Listener listener);
        public native static void removeListener(Listener listener);

        /*
         * Render a map extent
         */
        public native static void renderMap(double originLatitude,
                                            double originLongitude,
                                            int scale,
                                            int imageWidth,
                                            int imageHeight,
                                            double rotationAngleDegrees);

        /*
         * Workarounds for render engine
         */
        public native static void allowRenderingInBackground(boolean allow);
    }

    /*
     * Lookup service
     * Required to search items on the map
     */
    public static class LookupService
    {
        public class LookupItem
        {
            public String mName;
            public String mSuggestion;
            public String mRegion;
            public String mAmenity;
            public double mLatitude;
            public double mLongitude;
            public int mType;

            public static final int TYPE_SUGGESTION = 0; // keep it synchronous with MapsMeService.cpp
            public static final int TYPE_FEATURE = 1; // keep it synchronous with MapsMeService.cpp
        }

        public interface Listener
        {
            void onLookupComplete(LookupItem[] items);
        }

        /*
         * Add/Remove event listeners
         */
        public native static void addListener(Listener listener);
        public native static void removeListener(Listener listener);

        /*
         * Searches items on the map
         */
        public native static void lookup(double originLatitude,
                                         double originLongitude,
                                         int radiusMeters,
                                         String query,
                                         String language,
                                         int count);
    }
}
