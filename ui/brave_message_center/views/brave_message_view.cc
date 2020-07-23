// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include <base/debug/stack_trace.h>
#include "brave/ui/brave_message_center/views/brave_message_view.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/shadow_util.h"
#include "ui/gfx/shadow_value.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/notification_background_painter.h"
#include "brave/ui/brave_message_center/views/brave_notification_control_buttons_view.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#endif

namespace message_center {

namespace {

// Creates a text for spoken feedback from the data contained in the
// notification.
base::string16 CreateAccessibleName(const Notification& notification) {
  if (!notification.accessible_name().empty())
    return notification.accessible_name();

  // Fall back to a text constructed from the notification.
  std::vector<base::string16> accessible_lines = {
      notification.title(), notification.message(),
      notification.context_message()};
  std::vector<NotificationItem> items = notification.items();
  for (size_t i = 0; i < items.size() && i < kNotificationMaximumItems; ++i) {
    accessible_lines.push_back(items[i].title + base::ASCIIToUTF16(" ") +
                               items[i].message);
  }
  return base::JoinString(accessible_lines, base::ASCIIToUTF16("\n"));
}

bool ShouldShowAeroShadowBorder() {
#if defined(OS_WIN)
  return ui::win::IsAeroGlassEnabled();
#else
  return false;
#endif
}

}  // namespace

// static
const char kViewClassName[] = "BraveMessageView";

class BraveMessageView::HighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  HighlightPathGenerator() = default;

  // views::HighlightPathGenerator:
  SkPath GetHighlightPath(const views::View* view) override {
    return static_cast<const BraveMessageView*>(view)->GetHighlightPath();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HighlightPathGenerator);
};

BraveMessageView::BraveMessageView(const Notification& notification) : MessageView(notification),
  notification_id_(notification.id()), slide_out_controller_(this, this) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  // no idea wtf this is albert
  // focus_ring_ = views::FocusRing::Install(this);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<HighlightPathGenerator>());

  // TODO(amehfooz): Remove explicit color setting after native theme changes.
  // albert lol wut
  // focus_ring_->SetColor(SK_ColorTRANSPARENT);

  // Paint to a dedicated layer to make the layer non-opaque.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  UpdateWithNotification(notification);

  // albert: this might be interesting
  // base::debug::StackTrace st;
  // st.Print();
  UpdateCornerRadius(100, 100);

  // If Aero is enabled, set shadow border.
  if (ShouldShowAeroShadowBorder()) {
    const auto& shadow = gfx::ShadowDetails::Get(2, 0);
    gfx::Insets ninebox_insets = gfx::ShadowValue::GetBlurRegion(shadow.values);
    SetBorder(views::CreateBorderPainter(
        views::Painter::CreateImagePainter(shadow.ninebox_image,
                                           ninebox_insets),
        -gfx::ShadowValue::GetMargin(shadow.values)));
  }
}

void BraveMessageView::UpdateWithNotification(const Notification& notification) {
  pinned_ = notification.pinned();
  base::string16 new_accessible_name = CreateAccessibleName(notification);
  if (new_accessible_name != accessible_name_) {
    accessible_name_ = new_accessible_name;
    NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);
  }
  slide_out_controller_.set_slide_mode(CalculateSlideMode());
}

void BraveMessageView::CloseSwipeControl() {
  slide_out_controller_.CloseSwipeControl();
}

void BraveMessageView::SlideOutAndClose(int direction) {
  // Do not process events once the message view is animating out.
  // crbug.com/940719
  SetEnabled(false);

  slide_out_controller_.SlideOutAndClose(direction);
}

void BraveMessageView::SetExpanded(bool expanded) {
  // Not implemented by default.
}

bool BraveMessageView::IsExpanded() const {
  // Not implemented by default.
  return false;
}

bool BraveMessageView::IsAutoExpandingAllowed() const {
  // Allowed by default.
  return true;
}

bool BraveMessageView::IsManuallyExpandedOrCollapsed() const {
  // Not implemented by default.
  return false;
}

void BraveMessageView::SetManuallyExpandedOrCollapsed(bool value) {
  // Not implemented by default.
}

void BraveMessageView::UpdateCornerRadius(int top_radius, int bottom_radius) {
  SetCornerRadius(top_radius, bottom_radius);
  SetBackground(views::CreateBackgroundFromPainter(
      std::make_unique<NotificationBackgroundPainter>(top_radius,
                                                      bottom_radius)));
  SchedulePaint();
}

SkPath BraveMessageView::GetHighlightPath() const {
  gfx::Rect rect(GetBoundsInScreen().size());
  // Shrink focus ring size by -kFocusHaloInset on each side to draw
  // them on top of the notifications. We need to do this because TrayBubbleView
  // has a layer that masks to bounds due to which the focus ring can not extend
  // outside the view.
  int inset = -views::PlatformStyle::kFocusHaloInset;
  rect.Inset(gfx::Insets(inset));

  int top_radius = std::max(0, top_radius_ - inset);
  //int top_radius = 0;
  int bottom_radius = std::max(0, bottom_radius_ - inset);
  // int bottom_radius = 60;
  LOG(INFO) << "albert *** calling GetHighlightPath()" << top_radius << " " << bottom_radius;
  SkScalar radii[8] = {top_radius,    top_radius,      // top-left
                       top_radius,    top_radius,      // top-right
                       bottom_radius, bottom_radius,   // bottom-right
                       bottom_radius, bottom_radius};  // bottom-left

  return SkPath().addRoundRect(gfx::RectToSkRect(rect), radii);
}

void BraveMessageView::OnContainerAnimationStarted() {
  // Not implemented by default.
}

void BraveMessageView::OnContainerAnimationEnded() {
  // Not implemented by default.
}

void BraveMessageView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kGenericContainer;
  node_data->AddStringAttribute(
      ax::mojom::StringAttribute::kRoleDescription,
      l10n_util::GetStringUTF8(IDS_MESSAGE_NOTIFICATION_ACCESSIBLE_NAME));
  node_data->SetName(accessible_name_);
}

void BraveMessageView::OnPaint(gfx::Canvas* canvas) {
  if (ShouldShowAeroShadowBorder()) {
    // If the border is shadow, paint border first.
    OnPaintBorder(canvas);
    // Clip at the border so we don't paint over it.
    canvas->ClipRect(GetContentsBounds());
    OnPaintBackground(canvas);
  } else {
    views::View::OnPaint(canvas);
  }
}

void BraveMessageView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN: {
      SetDrawBackgroundAsActive(true);
      break;
    }
    case ui::ET_GESTURE_TAP_CANCEL:
    case ui::ET_GESTURE_END: {
      SetDrawBackgroundAsActive(false);
      break;
    }
    case ui::ET_GESTURE_TAP: {
      SetDrawBackgroundAsActive(false);
      MessageCenter::Get()->ClickOnNotification(notification_id_);
      event->SetHandled();
      return;
    }
    default: {
      // Do nothing
    }
  }

  if (!event->IsScrollGestureEvent() && !event->IsFlingScrollEvent())
    return;

  if (scroller_)
    scroller_->OnGestureEvent(event);
  event->SetHandled();
}

void BraveMessageView::RemovedFromWidget() {
  if (!focus_manager_)
    return;
  focus_manager_->RemoveFocusChangeListener(this);
  focus_manager_ = nullptr;
}

void BraveMessageView::AddedToWidget() {
  focus_manager_ = GetFocusManager();
  if (focus_manager_)
    focus_manager_->AddFocusChangeListener(this);
}

void BraveMessageView::OnThemeChanged() {
  InkDropHostView::OnThemeChanged();
  SetNestedBorderIfNecessary();
}

ui::Layer* BraveMessageView::GetSlideOutLayer() {
  return is_nested_ ? layer() : GetWidget()->GetLayer();
}

void BraveMessageView::OnSlideStarted() {
  for (auto& observer : observers_) {
    observer.OnSlideStarted(notification_id_);
  }
}

void BraveMessageView::OnSlideChanged(bool in_progress) {
  for (auto& observer : observers_) {
    observer.OnSlideChanged(notification_id_);
  }
}

void BraveMessageView::AddObserver(BraveMessageView::Observer* observer) {
  observers_.AddObserver(observer);
}

void BraveMessageView::RemoveObserver(BraveMessageView::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void BraveMessageView::OnSlideOut() {
  // The notification will be deleted after slide out, so give observers a
  // chance to handle the notification before fulling sliding out.
  for (auto& observer : observers_)
    observer.OnPreSlideOut(notification_id_);

  MessageCenter::Get()->RemoveNotification(notification_id_,
                                           true /* by_user */);
  for (auto& observer : observers_)
    observer.OnSlideOut(notification_id_);
}

views::SlideOutController::SlideMode BraveMessageView::CalculateSlideMode() const {
  if (disable_slide_)
    return views::SlideOutController::SlideMode::kNone;

  switch (GetMode()) {
    case Mode::SETTING:
      return views::SlideOutController::SlideMode::kNone;
    case Mode::PINNED:
      return views::SlideOutController::SlideMode::kPartial;
    case Mode::NORMAL:
      return views::SlideOutController::SlideMode::kFull;
  }

  NOTREACHED();
  return views::SlideOutController::SlideMode::kFull;
}

BraveMessageView::Mode BraveMessageView::GetMode() const {
  if (setting_mode_)
    return Mode::SETTING;

  // Only nested notifications can be pinned. Standalones (i.e. popups) can't
  // be.
  if (pinned_ && is_nested_)
    return Mode::PINNED;

  return Mode::NORMAL;
}

float BraveMessageView::GetSlideAmount() const {
  return slide_out_controller_.gesture_amount();
}

void BraveMessageView::SetSettingMode(bool setting_mode) {
  setting_mode_ = setting_mode;
  slide_out_controller_.set_slide_mode(CalculateSlideMode());
  UpdateControlButtonsVisibility();
}

void BraveMessageView::DisableSlideForcibly(bool disable) {
  disable_slide_ = disable;
  slide_out_controller_.set_slide_mode(CalculateSlideMode());
}

void BraveMessageView::SetSlideButtonWidth(int control_button_width) {
  slide_out_controller_.SetSwipeControlWidth(control_button_width);
}

void BraveMessageView::SetCornerRadius(int top_radius, int bottom_radius) {
  LOG(INFO) << "albert *** UpdateCornerRadius" << top_radius << " " << bottom_radius;
  top_radius_ = top_radius;
  bottom_radius_ = bottom_radius;
}

void BraveMessageView::OnCloseButtonPressed() {
  for (auto& observer : observers_)
    observer.OnCloseButtonPressed(notification_id_);
  MessageCenter::Get()->RemoveNotification(notification_id_,
                                           true /* by_user */);
}

void BraveMessageView::OnSettingsButtonPressed(const ui::Event& event) {
  for (auto& observer : observers_)
    observer.OnSettingsButtonPressed(notification_id_);

  MessageCenter::Get()->ClickOnSettingsButton(notification_id_);
}

void BraveMessageView::OnSnoozeButtonPressed(const ui::Event& event) {
  for (auto& observer : observers_)
    observer.OnSnoozeButtonPressed(notification_id_);
}

void BraveMessageView::SetNestedBorderIfNecessary() {
  if (is_nested_) {
    /*
    SkColor border_color = GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_UnfocusedBorderColor);
        */
    SkColor border_color = SK_ColorTRANSPARENT;
    SetBorder(views::CreateRoundedRectBorder(
        kNotificationBorderThickness, kNotificationCornerRadius, border_color));
  }
}

}  // namespace message_center
