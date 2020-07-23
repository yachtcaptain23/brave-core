// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_BRAVE_MESSAGE_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_BRAVE_MESSAGE_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/animation/slide_out_controller.h"
#include "ui/views/animation/slide_out_controller_delegate.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace views {
class ScrollView;
}  // namespace views

namespace message_center {

namespace test {
class MessagePopupCollectionTest;
}

class Notification;
class NotificationControlButtonsView;
class MessageView;

// An base class for a notification entry. Contains background and other
// elements shared by derived notification views.
class MESSAGE_CENTER_EXPORT BraveMessageView : public MessageView {
 public:
  BraveMessageView(const Notification& notification);
  ~BraveMessageView() override;
  void UpdateWithNotification(const Notification& notification) override;
  void SetExpanded(bool expanded) override;
  bool IsExpanded() const override;
  bool IsAutoExpandingAllowed() const override;
  bool IsManuallyExpandedOrCollapsed() const override;
  void SetManuallyExpandedOrCollapsed(bool value) override;
  void CloseSwipeControl() override;
  void SlideOutAndClose(int direction) override;

  void UpdateCornerRadius(int top_radius, int bottom_radius) override;

  // Invoked when the container view of MessageView (e.g. MessageCenterView in
  // ash) is starting the animation that possibly hides some part of
  // the MessageView.
  // During the animation, MessageView should comply with the Z order in views.
  void OnContainerAnimationStarted() override;
  void OnContainerAnimationEnded() override;
  void OnCloseButtonPressed();
  void OnSettingsButtonPressed(const ui::Event& event) override;
  void OnSnoozeButtonPressed(const ui::Event& event) override;

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void RemovedFromWidget() override;
  void AddedToWidget() override;
  void OnThemeChanged() override;

  // views::SlideOutControllerDelegate:
  ui::Layer* GetSlideOutLayer() override;
  void OnSlideStarted() override;
  void OnSlideChanged(bool in_progress) override;
  void OnSlideOut() override;

  // views::FocusChangeListener:

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  Mode GetMode() const;

  // Gets the current horizontal scroll offset of the view by slide gesture.
  float GetSlideAmount() const;

  // Set "setting" mode. This overrides "pinned" mode. See the comment of
  // MessageView::Mode enum for detail.
  void SetSettingMode(bool setting_mode);

  // Disables slide by vertical swipe regardless of the current notification
  // mode.
  void DisableSlideForcibly(bool disable);

  // Updates the width of the buttons which are hidden and avail by swipe.
  void SetSlideButtonWidth(int coutrol_button_width);

  void set_scroller(views::ScrollView* scroller) { scroller_ = scroller; }
  std::string notification_id() const { return notification_id_; }

 protected:
  // Changes the background color and schedules a paint.
  void SetCornerRadius(int top_radius, int bottom_radius);

  views::ScrollView* scroller() { return scroller_; }

  bool is_nested() const { return is_nested_; }

  base::ObserverList<Observer>::Unchecked* observers() { return &observers_; }

 private:
  class HighlightPathGenerator;

  // Gets the highlight path for the notification based on bounds and corner
  // radii.
  SkPath GetHighlightPath() const;

  // Returns the ideal slide mode by calculating the current status.
  views::SlideOutController::SlideMode CalculateSlideMode() const;

  // Sets the border if |is_nested_| is true.
  void SetNestedBorderIfNecessary();

  std::string notification_id_;
  views::ScrollView* scroller_ = nullptr;

  base::string16 accessible_name_;

  // Flag if the notification is set to pinned or not. See the comment in
  // MessageView::Mode for detail.
  bool pinned_ = false;

  // "fixed" mode flag. See the comment in MessageView::Mode for detail.
  bool setting_mode_ = false;

  views::SlideOutController slide_out_controller_;
  base::ObserverList<Observer>::Unchecked observers_;

  // True if |this| is embedded in another view. Equivalent to |!top_level| in
  // MessageViewFactory parlance.
  bool is_nested_ = false;

  // True if the slide is disabled forcibly.
  bool disable_slide_ = false;

  views::FocusManager* focus_manager_ = nullptr;
  std::unique_ptr<views::FocusRing> focus_ring_;

  // Radius values used to determine the rounding for the rounded rectangular
  // shape of the notification.
  int top_radius_ = 0;
  int bottom_radius_ = 0;

  DISALLOW_COPY_AND_ASSIGN(BraveMessageView);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_MESSAGE_VIEW_H_
