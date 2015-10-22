package com.mapswithme.maps;

import android.os.Bundle;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.View;

import com.mapswithme.maps.base.MWMFragmentActivity;

public abstract class RenderActivity extends MWMFragmentActivity
                                     implements View.OnTouchListener,
                                                SurfaceHolder.Callback
{
  private int mLastPointerId = 0;
  private SurfaceHolder mSurfaceHolder = null;
  private int m_displayDensity = 0;

  @Override
  public void surfaceCreated(SurfaceHolder surfaceHolder)
  {
    Log.i("UVR", "Surface created");
    mSurfaceHolder = surfaceHolder;
    InitEngine();
  }

  @Override
  public void surfaceChanged(SurfaceHolder surfaceHolder, int i, int w, int h)
  {
    SurfaceResized(w, h);
  }

  @Override
  public void surfaceDestroyed(SurfaceHolder surfaceHolder)
  {
    mSurfaceHolder = null;
    DestroyEngine();
  }

  @Override
  protected void onCreate(Bundle b)
  {
    Log.i("UVR", "onCreate");
    final DisplayMetrics metrics = new DisplayMetrics();
    getWindowManager().getDefaultDisplay().getMetrics(metrics);
    m_displayDensity = metrics.densityDpi;
    super.onCreate(b);
  }

  @Override
  protected void onResume()
  {
    Log.i("UVR", "onResume");
    InitEngine();
    super.onResume();
  }

  @Override
  protected void onStop()
  {
    super.onStop();
    DestroyEngine();
  }

  @Override
  public boolean onTouch(View view, MotionEvent event)
  {
    final int count = event.getPointerCount();

    if (count == 0)
      return super.onTouchEvent(event);

    switch (count)
    {
    case 1:
    {
      mLastPointerId = event.getPointerId(0);

      final float x0 = event.getX();
      final float y0 = event.getY();

      return OnTouch(event.getAction(), true, false, x0, y0, 0, 0);
    }
    default:
    {
      final float x0 = event.getX(0);
      final float y0 = event.getY(0);

      final float x1 = event.getX(1);
      final float y1 = event.getY(1);

      if (event.getPointerId(0) == mLastPointerId)
        return OnTouch(event.getAction(), true, true, x0, y0, x1, y1);
      else
        return OnTouch(event.getAction(), true, true, x1, y1, x0, y0);
    }
    }
  }

  private void InitEngine()
  {
    Log.i("UVR", "Init engine");
    if (mSurfaceHolder != null)
    {
      Log.i("UVR", "CreateEngine");
      if (CreateEngine(mSurfaceHolder.getSurface(), m_displayDensity))
        OnRenderingInitialized();
      else
        ReportUnsupported();
    }
  }

  abstract public void OnRenderingInitialized();
  abstract public void ReportUnsupported();

  private native boolean CreateEngine(Surface surface, int density);
  private native void SurfaceResized(int w, int h);
  private native void DestroyEngine();
  private native boolean OnTouch(int actionType, boolean hasFirst, boolean hasSecond, float x1, float y1, float x2, float y2);
}
