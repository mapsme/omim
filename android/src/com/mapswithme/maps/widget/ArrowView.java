package com.mapswithme.maps.widget;

import android.content.Context;
import android.graphics.Canvas;
import androidx.annotation.NonNull;
import android.util.AttributeSet;
import android.widget.ImageView;

public class ArrowView extends ImageView
{
  private float mWidth;
  private float mHeight;

  private float mAngle;

  public ArrowView(Context context, AttributeSet attrs)
  {
    super(context, attrs);
  }

  public void setAzimuth(double azimuth)
  {
    mAngle = (float) Math.toDegrees(azimuth);
    invalidate();
  }

  @Override
  protected void onSizeChanged(int w, int h, int oldw, int oldh)
  {
    super.onSizeChanged(w, h, oldw, oldh);

    mWidth = getWidth();
    mHeight = getHeight();
  }

  @Override
  protected void onDraw(@NonNull Canvas canvas)
  {
    canvas.save();
    canvas.rotate(mAngle, mWidth / 2, mHeight / 2);
    super.onDraw(canvas);
    canvas.restore();
  }
}

