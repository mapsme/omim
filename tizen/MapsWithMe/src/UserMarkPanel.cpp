#include "UserMarkPanel.hpp"
#include "SceneRegister.hpp"
#include "MapsWithMeForm.hpp"
#include "AppResourceId.h"
#include "Constants.hpp"
#include "BookMarkUtils.hpp"
#include "Utils.hpp"

#include "../../../map/framework.hpp"
#include "../../../platform/settings.hpp"
#include "../../../platform/tizen_utils.hpp"
#include "../../../base/logging.hpp"

using namespace Tizen::Base;
using namespace Tizen::Ui::Controls;
using namespace Tizen::Ui::Scenes;
using namespace Tizen::Graphics;
using namespace consts;
using namespace bookmark;

UserMarkPanel::UserMarkPanel()
:m_pMainForm(0)
{

}

UserMarkPanel::~UserMarkPanel(void)
{
}

bool UserMarkPanel::Construct(const Tizen::Graphics::FloatRectangle& rect)
{
  Panel::Construct(rect);
  SetBackgroundColor(white_bkg);
  m_pButton = new Button;
  m_pButton->Construct(FloatRectangle(rect.width - btnSz - btwWdth, btwWdth, btnSz, btnSz));

  m_pButton->SetActionId(ID_STAR);
  m_pButton->AddActionEventListener(*this);
  AddControl(m_pButton);

  m_pEditButton = new Button;
  m_pEditButton->Construct(FloatRectangle(rect.width - btnSz - btwWdth - editBtnSz, 0, editBtnSz, editBtnSz));
  m_pEditButton->SetActionId(ID_EDIT);
  m_pEditButton->AddActionEventListener(*this);
  AddControl(m_pEditButton);

  m_pLabel = new Label();
  m_pLabel->Construct(Rectangle(btwWdth, btwWdth, rect.width - btnSz - btwWdth - editBtnSz, markPanelHeight - 2 * btwWdth), "");
  m_pLabel->SetTextColor(txt_main_black);
  m_pLabel->AddTouchEventListener(*this);
  m_pLabel->SetTextHorizontalAlignment(ALIGNMENT_LEFT);
  m_pLabel->SetTextVerticalAlignment(ALIGNMENT_TOP);
  m_pLabel->SetTextConfig(mainFontSz ,LABEL_TEXT_STYLE_NORMAL);
  AddControl(m_pLabel);

  UpdateState();
  return true;
}

void UserMarkPanel::SetMainForm(MapsWithMeForm * pMainForm)
{
  m_pMainForm = pMainForm;
}

void  UserMarkPanel::OnTouchPressed (Tizen::Ui::Control const & source,
    Tizen::Graphics::Point const & currentPosition,
    Tizen::Ui::TouchEventInfo const & touchInfo)
{
  if (m_pMainForm)
    m_pMainForm->ShowBookMarkSplitPanel();
}

void UserMarkPanel::Enable()
{
  SetShowState(true);
  UpdateState();
  Invalidate(true);
}

void UserMarkPanel::Disable()
{
  SetShowState(false);
}

void UserMarkPanel::OnActionPerformed(Tizen::Ui::Control const & source, int actionId)
{
  switch(actionId)
  {
    case ID_STAR:
    {
      if (IsBookMark(GetCurMark()))
      {
        BookMarkManager::GetInstance().RemoveCurBookMark();
      }
      else
      {
        BookMarkManager::GetInstance().AddCurMarkToBookMarks();
      }
      UpdateState();
      break;
    }
    case ID_EDIT:
    {
      SceneManager * pSceneManager = SceneManager::GetInstance();
      pSceneManager->GoForward(ForwardSceneTransition(SCENE_PLACE_PAGE_SETTINGS,
          SCENE_TRANSITION_ANIMATION_TYPE_LEFT, SCENE_HISTORY_OPTION_ADD_HISTORY, SCENE_DESTROY_OPTION_KEEP));
      break;
    }
  }
}

UserMark const * UserMarkPanel::GetCurMark()
{
  return BookMarkManager::GetInstance().GetCurMark();
}

void UserMarkPanel::UpdateState()
{
  if (IsBookMark(GetCurMark()))
  {
    m_pButton->SetNormalBackgroundBitmap(*GetBitmap(IDB_PLACE_PAGE_BUTTON_SELECTED));
    m_pButton->SetPressedBackgroundBitmap(*GetBitmap(IDB_PLACE_PAGE_BUTTON_SELECTED));
    m_pEditButton->SetShowState(true);
    m_pEditButton->SetNormalBackgroundBitmap(*GetBitmap(IDB_PLACE_PAGE_EDIT_BUTTON));
    m_pEditButton->SetPressedBackgroundBitmap(*GetBitmap(IDB_PLACE_PAGE_EDIT_BUTTON));
  }
  else
  {
    m_pButton->SetNormalBackgroundBitmap(*GetBitmap(IDB_PLACE_PAGE_BUTTON));
    m_pButton->SetPressedBackgroundBitmap(*GetBitmap(IDB_PLACE_PAGE_BUTTON));
    m_pEditButton->SetShowState(false);
  }

  BookMarkManager & mngr = GetBMManager();
  String res = GetMarkName(GetCurMark());
  res.Append("\n");
  if (IsBookMark(GetCurMark()))
    res.Append(mngr.GetCurrentCategoryName());
  else
    res.Append(GetMarkType(GetCurMark()));
  m_pLabel->SetText(res);

  Invalidate(true);
}
