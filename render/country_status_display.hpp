#pragma once

#include "render/active_maps_bridge.hpp"

#include "storage/index.hpp"
#include "storage/storage_defines.hpp"

#include "gui/element.hpp"
#include "platform/country_defines.hpp"

#include "std/unique_ptr.hpp"
#include "std/function.hpp"

namespace gui
{

class Button;
class TextView;

} // namespace gui

namespace storage { struct TIndex; }

namespace rg
{

/// This class is a composite GUI element to display
/// an on-screen GUI for the country, which is not downloaded yet.
class CountryStatusDisplay : public gui::Element
{
  using TBase = gui::Element;
public:
  struct Params : public gui::Element::Params
  {
    ActiveMapsBridge * m_activeMaps;
  };

  CountryStatusDisplay(Params const & p);
  ~CountryStatusDisplay();

  /// set current country name
  void SetCountryIndex(storage::TIndex const & idx);

  /// @name Override from graphics::OverlayElement and gui::Element.
  //@{
  virtual void setIsVisible(bool isVisible) const;
  virtual void setIsDirtyLayout(bool isDirty) const;
  virtual m2::RectD GetBoundRect() const;

  void draw(graphics::OverlayRenderer * r, math::Matrix<double, 3, 3> const & m) const;

  void cache();
  void purge();
  void layout();

  void setController(gui::Controller * controller);

  bool onTapStarted(m2::PointD const & pt);
  bool onTapMoved(m2::PointD const & pt);
  bool onTapEnded(m2::PointD const & pt);
  bool onTapCancelled(m2::PointD const & pt);
  //@}

private:
  virtual void CountryStatusChanged(storage::TIndex const & index, storage::TStatus const & newStatus);
  virtual void DownloadingProgressUpdate(storage::TIndex const & index, storage::LocalAndRemoteSizeT const & progress);

  template <class T1, class T2>
  string FormatStatusMessage(string const & msgID, T1 const * t1 = 0, T2 const * t2 = 0);

  void FormatDisplayName(string const & mapName, string const & groupName);

  void SetVisibilityForState() const;
  void SetContentForState();
  void SetContentForDownloadPropose();
  void SetContentForProgress();
  void SetContentForInQueue();
  void SetContentForError();

  void ComposeElementsForState();

  typedef function<bool (unique_ptr<gui::Button> const &, m2::PointD const &)> TTapActionFn;
  bool OnTapAction(TTapActionFn const & action, m2::PointD const & pt);
  void OnButtonClicked(Element const * button);

  void Repaint() const;

  bool IsStatusFailed() const;

private:
  ActiveMapsBridge * m_activeMaps;

  unique_ptr<gui::TextView> m_label;
  unique_ptr<gui::Button> m_primaryButton;
  unique_ptr<gui::Button> m_secondaryButton;

  string m_displayMapName;
  mutable storage::TStatus m_countryStatus = storage::TStatus::EUnknown;
  storage::TIndex m_countryIdx;
  storage::LocalAndRemoteSizeT m_progressSize;
};

} // namespace rg
