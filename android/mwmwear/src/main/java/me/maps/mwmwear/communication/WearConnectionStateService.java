package me.maps.mwmwear.communication;

import android.util.Log;

import com.google.android.gms.wearable.Node;
import com.google.android.gms.wearable.WearableListenerService;

public class WearConnectionStateService extends WearableListenerService
{
  private static final String TAG = WearConnectionStateService.class.getSimpleName();
  public static boolean isConnected;

  @Override
  public void onPeerConnected(Node peer)
  {
    Log.d(TAG, "Peer connected!");
    isConnected = true;
  }

  @Override
  public void onPeerDisconnected(Node peer)
  {
    Log.d(TAG, "Peer disconnected!");
    isConnected = false;
  }
}
