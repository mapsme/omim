#pragma once

#include <FUi.h>

class PlacePageSettingsForm: public Tizen::Ui::Controls::Form
, public Tizen::Ui::Controls::IFormBackEventListener
, public Tizen::Ui::IActionEventListener
, public Tizen::Ui::ITextEventListener
, public Tizen::Ui::Scenes::ISceneEventListener
{
public:
  PlacePageSettingsForm();
  virtual ~PlacePageSettingsForm(void);

  bool Initialize(void);
  virtual result OnInitializing(void);
  virtual void OnFormBackRequested(Tizen::Ui::Controls::Form & source);
  virtual void OnActionPerformed(Tizen::Ui::Control const & source, int actionId);
  // Tizen::Ui::ITextEventListener
  virtual void  OnTextValueChangeCanceled (Tizen::Ui::Control const & source){}
  virtual void  OnTextValueChanged (Tizen::Ui::Control const & source);

  // ISceneEventListener
  virtual void OnSceneActivatedN(const Tizen::Ui::Scenes::SceneId & previousSceneId,
                   const Tizen::Ui::Scenes::SceneId & currentSceneId, Tizen::Base::Collection::IList * pArgs);
  virtual void OnSceneDeactivated(const Tizen::Ui::Scenes::SceneId & currentSceneId,
                  const Tizen::Ui::Scenes::SceneId & nextSceneId){}

  void UpdateState();
private:
  enum EActions
    {
      ID_SELECT_SET = 0,
      ID_SHARE_BUTTON,
      ID_DONE_BUTTON
    };
};
