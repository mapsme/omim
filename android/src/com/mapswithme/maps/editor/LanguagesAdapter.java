package com.mapswithme.maps.editor;

import android.support.annotation.NonNull;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import com.mapswithme.maps.R;
import com.mapswithme.maps.editor.data.Language;

public class LanguagesAdapter extends RecyclerView.Adapter<LanguagesAdapter.Holder>
{
  private final Language[] mLanguages;
  private final LanguagesFragment mFragment;

  public LanguagesAdapter(@NonNull LanguagesFragment host, @NonNull Language[] languages)
  {
    mFragment = host;
    mLanguages = languages;
  }

  @Override
  public Holder onCreateViewHolder(ViewGroup parent, int viewType)
  {
    return new Holder(LayoutInflater.from(parent.getContext()).inflate(R.layout.item_language, parent, false));
  }

  @Override
  public void onBindViewHolder(Holder holder, int position)
  {
    holder.bind(position);
  }

  @Override
  public int getItemCount()
  {
    return mLanguages.length;
  }

  protected class Holder extends RecyclerView.ViewHolder
  {
    TextView name;

    public Holder(View itemView)
    {
      super(itemView);
      name = (TextView) itemView;
      itemView.setOnClickListener(new View.OnClickListener()
      {
        @Override
        public void onClick(View v)
        {
          mFragment.onLanguageSelected(mLanguages[getAdapterPosition()]);
        }
      });
    }

    public void bind(int position)
    {
      name.setText(mLanguages[position].name);
    }
  }
}
