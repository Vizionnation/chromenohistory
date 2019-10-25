// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_WEB_UI_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_WEB_UI_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/assistant/assistant_controller_observer.h"
#include "base/macros.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

class AssistantController;
class AssistantWebContainerView;

// The class to manage Assistant web container view.
class ASH_EXPORT AssistantWebUiController : public views::WidgetObserver,
                                            public AssistantControllerObserver {
 public:
  explicit AssistantWebUiController(AssistantController* assistant_controller);
  ~AssistantWebUiController() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // AssistantControllerObserver:
  void OnAssistantControllerDestroying() override;
  void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) override;

  AssistantWebContainerView* GetViewForTest();

 private:
  void ShowUi();

  // Constructs/resets |web_container_view_|.
  void CreateWebContainerView();
  void ResetWebContainerView();

  AssistantController* const assistant_controller_;  // Owned by Shell.

  // Owned by view hierarchy.
  AssistantWebContainerView* web_container_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AssistantWebUiController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_WEB_UI_CONTROLLER_H_
