#pragma once

#include <FUi.h>

class CopyRightForm: public Tizen::Ui::Controls::Form
, public Tizen::Ui::Controls::IFormBackEventListener
, public Tizen::Ui::IActionEventListener
{
public:
  CopyRightForm();
  virtual ~CopyRightForm(void);

  bool Initialize(void);
  virtual result OnInitializing(void);
  virtual void OnFormBackRequested(Tizen::Ui::Controls::Form & source);
  virtual void OnActionPerformed(Tizen::Ui::Control const & source, int actionId);

private:
  static const int ID_CLOSE = 101;
};
