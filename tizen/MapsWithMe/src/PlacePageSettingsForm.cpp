#include "PlacePageSettingsForm.hpp"
#include "SceneRegister.hpp"
#include "AppResourceId.h"
#include "BookMarkUtils.hpp"
#include "Utils.hpp"
#include "Constants.hpp"
#include "../../../base/logging.hpp"
#include "../../../platform/settings.hpp"
#include "../../../platform/tizen_utils.hpp"

using namespace Tizen::Base;
using namespace Tizen::Ui;
using namespace Tizen::Ui::Controls;
using namespace Tizen::Ui::Scenes;
using namespace bookmark;
using namespace consts;

PlacePageSettingsForm::PlacePageSettingsForm()
{
}

PlacePageSettingsForm::~PlacePageSettingsForm(void)
{
}

bool PlacePageSettingsForm::Initialize(void)
{
  Construct(IDF_PLACE_PAGE_SETTIGS_FORM);
  return true;
}

result PlacePageSettingsForm::OnInitializing(void)
{
  Button * pCurrentSetButton = static_cast<Button*>(GetControl(IDC_BUTTON_SET, true));
  pCurrentSetButton->SetActionId(ID_SELECT_SET);
  pCurrentSetButton->AddActionEventListener(*this);

  EditArea * pMessage = static_cast<EditArea*>(GetControl(IDC_EDITAREA_NOTES, true));
  pMessage->AddTextEventListener(*this);

  EditField * pName = static_cast<EditField*>(GetControl(IDC_EDITFIELD_NAME, true));
  pName->AddTextEventListener(*this);

  Header* pHeader = GetHeader();
  ButtonItem buttonItem;
  buttonItem.Construct(BUTTON_ITEM_STYLE_TEXT, ID_DONE_BUTTON);
  buttonItem.SetText(GetString(IDS_DONE));
  pHeader->SetButtonColor(BUTTON_ITEM_STATUS_NORMAL, green);
  pHeader->SetButtonColor(BUTTON_ITEM_STATUS_PRESSED, green);
  pHeader->SetButton(BUTTON_POSITION_LEFT, buttonItem);
  pHeader->AddActionEventListener(*this);

  UpdateState();

  SetFormBackEventListener(this);
  return E_SUCCESS;
}

void PlacePageSettingsForm::OnTextValueChanged (Tizen::Ui::Control const & source)
{
  BookMarkManager & mngr = GetBMManager();
  EditArea * pMessage = static_cast<EditArea*>(GetControl(IDC_EDITAREA_NOTES, true));
  mngr.SetBookMarkMessage(pMessage->GetText());
  EditField * pName = static_cast<EditField*>(GetControl(IDC_EDITFIELD_NAME, true));
  mngr.SetCurBookMarkName(pName->GetText());
}

void PlacePageSettingsForm::OnActionPerformed(Tizen::Ui::Control const & source, int actionId)
{
  switch(actionId)
  {
    case ID_SELECT_SET:
    {
      SceneManager * pSceneManager = SceneManager::GetInstance();
      pSceneManager->GoForward(ForwardSceneTransition(SCENE_SELECT_BM_CATEGORY,
          SCENE_TRANSITION_ANIMATION_TYPE_LEFT, SCENE_HISTORY_OPTION_ADD_HISTORY, SCENE_DESTROY_OPTION_KEEP));
      break;
    }
    case ID_DONE_BUTTON:
    {
      SceneManager * pSceneManager = SceneManager::GetInstance();
      pSceneManager->GoBackward(BackwardSceneTransition(SCENE_TRANSITION_ANIMATION_TYPE_RIGHT));
      break;
    }
  }
  Invalidate(true);
}

void PlacePageSettingsForm::OnFormBackRequested(Tizen::Ui::Controls::Form & source)
{
  SceneManager * pSceneManager = SceneManager::GetInstance();
  pSceneManager->GoBackward(BackwardSceneTransition(SCENE_TRANSITION_ANIMATION_TYPE_RIGHT));
}

void PlacePageSettingsForm::OnSceneActivatedN(const Tizen::Ui::Scenes::SceneId& previousSceneId,
    const Tizen::Ui::Scenes::SceneId& currentSceneId, Tizen::Base::Collection::IList* pArgs)
{
  UpdateState();
}

void PlacePageSettingsForm::UpdateState()
{
  BookMarkManager & mngr = GetBMManager();
  Button * pCurrentSetButton = static_cast<Button*>(GetControl(IDC_BUTTON_SET, true));
  pCurrentSetButton->SetText(mngr.GetCurrentCategoryName());

  EditArea * pMessage = static_cast<EditArea*>(GetControl(IDC_EDITAREA_NOTES, true));
  if (!mngr.GetBookMarkMessage().IsEmpty())
    pMessage->SetText(mngr.GetBookMarkMessage());
  else
    pMessage->SetText(GetString(IDS_BOOKMARKS));

  EditField * pName = static_cast<EditField*>(GetControl(IDC_EDITFIELD_NAME, true));
  pName->SetText(GetMarkName(mngr.GetCurMark()));
}
