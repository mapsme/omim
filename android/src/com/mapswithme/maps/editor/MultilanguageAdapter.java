package com.mapswithme.maps.editor;

import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;

import com.mapswithme.maps.R;
import com.mapswithme.maps.editor.data.LocalizedName;
import com.mapswithme.util.UiUtils;

import java.util.ArrayList;

public class MultilanguageAdapter extends RecyclerView.Adapter<MultilanguageAdapter.Holder>
{
  private ArrayList<LocalizedName> mNames;
  private EditorHostFragment mHostFragment;

  // TODO(mgsergio): Refactor: don't pass the whole EditorHostFragment.
  MultilanguageAdapter(EditorHostFragment hostFragment)
  {
    mHostFragment = hostFragment;
    mNames = new ArrayList<>();
    ArrayList<LocalizedName> names = mHostFragment.getLocalizedNames();

    // Skip default name.
    for (int i = 1; i < names.size(); i++)
      mNames.add(names.get(i));
  }

  public void setNames(ArrayList<LocalizedName> names)
  {
    mNames = names;
  }

  public ArrayList<LocalizedName> getNames()
  {
    return mNames;
  }

  @Override
  public Holder onCreateViewHolder(ViewGroup parent, int viewType)
  {
    final View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.item_localized_name, parent, false);
    // TODO(mgsergio): Deletion is not implemented.
    UiUtils.hide(view.findViewById(R.id.delete));
    return new Holder(view);
  }

  @Override
  public void onBindViewHolder(Holder holder, int position)
  {
    LocalizedName name = mNames.get(position);
    holder.input.setText(name.name);
    holder.input.setHint(name.lang);
  }

  @Override
  public int getItemCount() { return mNames.size(); }

  public class Holder extends RecyclerView.ViewHolder
  {
    EditText input;

    public Holder(View itemView)
    {
      super(itemView);
      input = (EditText) itemView.findViewById(R.id.input);
      itemView.findViewById(R.id.delete).setOnClickListener(new View.OnClickListener()
                                                            {
                                                              @Override
                                                              public void onClick(View v)
                                                              {
                                                                // TODO(mgsergio): Implement item deletion.
                                                                // int position = getAdapterPosition();
                                                                // mHostFragment.removeLocalizedName(position + 1);
                                                                // mNames.remove(position);
                                                                // notifyItemRemoved(position);
                                                              }
                                                            });
    }
  }
}
