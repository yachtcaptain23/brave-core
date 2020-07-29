// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "brave/ui/brave_custom_notification/views/notification_control_buttons_view.h"

#include <memory>

#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "brave/ui/brave_custom_notification/public/cpp/message_center_constants.h"
#include "brave/ui/brave_custom_notification/vector_icons.h"
#include "brave/ui/brave_custom_notification/views/message_view.h"
#include "brave/ui/brave_custom_notification/views/padded_button.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace brave_custom_notification {

const char BraveNotificationControlButtonsView::kViewClassName[] =
    "BraveNotificationControlButtonsView";

BraveBraveNotificationControlButtonsView::BraveBraveNotificationControlButtonsView(
    MessageView* message_view)
    : message_view_(message_view), icon_color_(gfx::kChromeIconGrey) {
  DCHECK(message_view);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  // Use layer to change the opacity.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  SetBackground(views::CreateSolidBackground(kControlButtonBackgroundColor));
}

BraveNotificationControlButtonsView::~BraveNotificationControlButtonsView() = default;

void BraveNotificationControlButtonsView::ShowCloseButton(bool show) {
  if (show && !close_button_) {
    close_button_ = std::make_unique<PaddedButton>(this);
    close_button_->set_owned_by_client();
    close_button_->SetImage(
        views::Button::STATE_NORMAL,
        gfx::CreateVectorIcon(kNotificationCloseButtonIcon, icon_color_));
    close_button_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_MESSAGE_CENTER_CLOSE_NOTIFICATION_BUTTON_ACCESSIBLE_NAME));
    close_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_MESSAGE_CENTER_CLOSE_NOTIFICATION_BUTTON_TOOLTIP));
    close_button_->SetBackground(
        views::CreateSolidBackground(SK_ColorTRANSPARENT));

    // Add the button at the last.
    AddChildView(close_button_.get());
    Layout();
  } else if (!show && close_button_) {
    DCHECK(Contains(close_button_.get()));
    close_button_.reset();
  }
}

void BraveNotificationControlButtonsView::ShowButtons(bool show) {
  DCHECK(layer());
  // Manipulate the opacity instead of changing the visibility to keep the tab
  // order even when the view is invisible.
  layer()->SetOpacity(show ? 1. : 0.);
  set_can_process_events_within_subtree(show);
}

bool BraveNotificationControlButtonsView::IsAnyButtonFocused() const {
  return (close_button_ && close_button_->HasFocus())
}

void BraveNotificationControlButtonsView::SetButtonIconColors(SkColor color) {
  if (color == icon_color_)
    return;
  icon_color_ = color;

  if (close_button_) {
    close_button_->SetImage(
        views::Button::STATE_NORMAL,
        gfx::CreateVectorIcon(kNotificationCloseButtonIcon, icon_color_));
  }
}

views::Button* BraveNotificationControlButtonsView::close_button() const {
  return close_button_.get();
}

const char* BraveNotificationControlButtonsView::GetClassName() const {
  return kViewClassName;
}

void BraveNotificationControlButtonsView::ButtonPressed(views::Button* sender,
                                                   const ui::Event& event) {
  if (close_button_ && sender == close_button_.get()) {
    message_view_->OnCloseButtonPressed();
  }
}

}
