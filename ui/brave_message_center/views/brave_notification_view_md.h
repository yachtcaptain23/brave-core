// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_BRAVE_NOTIFICATION_VIEW_MD_H_
#define UI_MESSAGE_CENTER_VIEWS_BRAVE_NOTIFICATION_VIEW_MD_H_

#include <vector>

#include "chrome/browser/ui/browser_list_observer.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "ui/message_center/message_center_export.h"
#include "brave/ui/brave_message_center/views/brave_message_view.h"
#include "ui/views/animation/ink_drop_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/textfield/textfield_controller.h"

namespace views {
class ImageButton;
class Label;
class LabelButton;
class ProgressBar;
class RadioButton;
class Textfield;
}

namespace message_center {

class BraveNotificationHeaderView;
class ProportionalImageView;

// CompactTitleBraveMessageView shows notification title and message in a single
// line. This view is used for NOTIFICATION_TYPE_PROGRESS.
class CompactTitleBraveMessageView : public views::View {
 public:
  explicit CompactTitleBraveMessageView();
  ~CompactTitleBraveMessageView() override;

  const char* GetClassName() const override;

  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;

  void set_title(const base::string16& title);
  void set_message(const base::string16& message);

 private:
  DISALLOW_COPY_AND_ASSIGN(CompactTitleBraveMessageView);

  views::Label* title_ = nullptr;
  views::Label* message_ = nullptr;
};

class BraveLargeImageView : public views::View {
 public:
  BraveLargeImageView();
  ~BraveLargeImageView() override;

  void SetImage(const gfx::ImageSkia& image);

  void OnPaint(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;

 private:
  gfx::Size GetResizedImageSize();

  gfx::ImageSkia image_;

  DISALLOW_COPY_AND_ASSIGN(BraveLargeImageView);
};

// This class is needed in addition to LabelButton mainly becuase we want to set
// visible_opacity of InkDropHighlight.
// This button capitalizes the given label string.
class BraveNotificationButtonMD : public views::LabelButton {
 public:
  // |is_inline_reply| is true when the notification action takes text as the
  // return value i.e. the notification action is inline reply.
  // The input field would be shown when the button is clicked.
  // |placeholder| is placeholder text shown on the input field. Only used when
  // |is_inline_reply| is true.
  BraveNotificationButtonMD(views::ButtonListener* listener,
                       const base::string16& label,
                       const base::Optional<base::string16>& placeholder);
  ~BraveNotificationButtonMD() override;

  void SetText(const base::string16& text) override;
  const char* GetClassName() const override;

  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;

  SkColor enabled_color_for_testing() { return label()->GetEnabledColor(); }

  const base::Optional<base::string16>& placeholder() const {
    return placeholder_;
  }
  void set_placeholder(const base::Optional<base::string16>& placeholder) {
    placeholder_ = placeholder;
  }

 private:
  base::Optional<base::string16> placeholder_;

  DISALLOW_COPY_AND_ASSIGN(BraveNotificationButtonMD);
};

class BraveNotificationInputDelegate {
 public:
  virtual void OnNotificationInputSubmit(size_t index,
                                         const base::string16& text) = 0;
  virtual ~BraveNotificationInputDelegate() = default;
};

class BraveNotificationInputContainerMD : public views::InkDropHostView,
                                     public views::ButtonListener,
                                     public views::TextfieldController {
 public:
  BraveNotificationInputContainerMD(BraveNotificationInputDelegate* delegate);
  ~BraveNotificationInputContainerMD() override;

  void AnimateBackground(const ui::Event& event);

  // views::InkDropHostView:
  void AddLayerBeneathView(ui::Layer* layer) override;
  void RemoveLayerBeneathView(ui::Layer* layer) override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  SkColor GetInkDropBaseColor() const override;

  // Overridden from views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  void OnAfterUserAction(views::Textfield* sender) override;

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  views::Textfield* textfield() const { return textfield_; }
  views::ImageButton* button() const { return button_; }

 private:
  BraveNotificationInputDelegate* const delegate_;

  views::InkDropContainerView* const ink_drop_container_;

  views::Textfield* const textfield_;
  views::ImageButton* const button_;

  DISALLOW_COPY_AND_ASSIGN(BraveNotificationInputContainerMD);
};

// View that displays all current types of notification (web, basic, image, and
// list) except the custom notification. Future notification types may be
// handled by other classes, in which case instances of those classes would be
// returned by the Create() factory method below.
class MESSAGE_CENTER_EXPORT BraveNotificationViewMD
    : public BraveMessageView,
      public views::InkDropObserver,
      public BraveNotificationInputDelegate,
      public views::ButtonListener {
 public:
  explicit BraveNotificationViewMD(const Notification& notification);
  ~BraveNotificationViewMD() override;

  void Activate();

  void AddBackgroundAnimation(const ui::Event& event);
  void RemoveBackgroundAnimation();

  // BraveMessageView:
  void AddLayerBeneathView(ui::Layer* layer) override;
  void RemoveLayerBeneathView(ui::Layer* layer) override;
  void Layout() override;
  void OnFocus() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void PreferredSizeChanged() override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;
  SkColor GetInkDropBaseColor() const override;
  void UpdateWithNotification(const message_center::Notification& notification) override;
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;
  void UpdateCornerRadius(int top_radius, int bottom_radius) override;
  NotificationControlButtonsView* GetControlButtonsView() const override;
  bool IsExpanded() const override;
  void SetExpanded(bool expanded) override;
  bool IsManuallyExpandedOrCollapsed() const override;
  void SetManuallyExpandedOrCollapsed(bool value) override;
  void OnSettingsButtonPressed(const ui::Event& event) override;

  // views::InkDropObserver:
  void InkDropAnimationStarted() override;
  void InkDropRippleAnimationEnded(views::InkDropState ink_drop_state) override;

  // Overridden from BraveNotificationInputDelegate:
  void OnNotificationInputSubmit(size_t index,
                                 const base::string16& text) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest, AppNameExtension);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest, AppNameSystemNotification);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest, AppNameWebNotification);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest, CreateOrUpdateTest);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest, ExpandLongMessage);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest, InlineSettings);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest,
                           InlineSettingsInkDropAnimation);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest, NotificationWithoutIcon);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest, ShowProgress);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest, ShowTimestamp);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest, TestAccentColor);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest, TestActionButtonClick);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest, TestClick);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest, TestClickExpanded);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest,
                           TestDeleteOnDisableNotification);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest, TestDeleteOnToggleExpanded);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest, TestIconSizing);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest, TestInlineReply);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest,
                           TestInlineReplyActivateWithKeyPress);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest,
                           TestInlineReplyRemovedByUpdate);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest, TestLongTitleAndMessage);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest, UpdateAddingIcon);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest, UpdateButtonCountTest);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest, UpdateButtonsStateTest);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest, UpdateInSettings);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest, UpdateType);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest, UpdateViewsOrderingTest);
  FRIEND_TEST_ALL_PREFIXES(BraveNotificationViewMDTest, UseImageAsIcon);

  friend class BraveNotificationViewMDTest;

  class BraveNotificationViewMDPathGenerator;

  void UpdateControlButtonsVisibilityWithNotification(
      const message_center::Notification& notification);

  void CreateOrUpdateViews(const message_center::Notification& notification);

  void CreateOrUpdateContextTitleView(const message_center::Notification& notification);
  void CreateOrUpdateTitleView(const message_center::Notification& notification);
  void CreateOrUpdateBraveMessageView(const message_center::Notification& notification);
  void CreateOrUpdateCompactTitleBraveMessageView(const message_center::Notification& notification);
  void CreateOrUpdateProgressBarView(const message_center::Notification& notification);
  void CreateOrUpdateProgressStatusView(const message_center::Notification& notification);
  void CreateOrUpdateListItemViews(const message_center::Notification& notification);
  void CreateOrUpdateIconView(const message_center::Notification& notification);
  void CreateOrUpdateSmallIconView(const message_center::Notification& notification);
  void CreateOrUpdateImageView(const message_center::Notification& notification);
  void CreateOrUpdateActionButtonViews(const message_center::Notification& notification);
  void CreateOrUpdateInlineSettingsViews(const message_center::Notification& notification);

  bool IsExpandable();
  void ToggleExpanded();
  void UpdateViewForExpandedState(bool expanded);
  void ToggleInlineSettings(const ui::Event& event);

  // Returns the list of children which need to have their layers created or
  // destroyed when the ink drop is visible.
  std::vector<views::View*> GetChildrenForLayerAdjustment() const;

  views::InkDropContainerView* const ink_drop_container_;

  // View containing close and settings buttons
  std::unique_ptr<NotificationControlButtonsView> control_buttons_view_;

  // Whether this notification is expanded or not.
  bool expanded_ = true;

  // True if the notification is expanded/collapsed by user interaction.
  // If true, MessagePopupCollection will not auto-collapse the notification.
  bool manually_expanded_or_collapsed_ = true;

  // Whether hiding icon on the right side when expanded.
  bool hide_icon_on_expanded_ = false;

  // Number of total list items in the given Notification class.
  int list_items_count_ = 0;

  // Describes whether the view should display a hand pointer or not.
  bool clickable_;

  // Container views directly attached to this view.
  BraveNotificationHeaderView* header_row_ = nullptr;
  views::View* content_row_ = nullptr;
  views::View* actions_row_ = nullptr;
  views::View* settings_row_ = nullptr;

  // Containers for left and right side on |content_row_|
  views::View* left_content_ = nullptr;
  views::View* right_content_ = nullptr;

  // Views which are dynamically created inside view hierarchy.
  views::Label* title_view_ = nullptr;
  views::Label* message_view_ = nullptr;
  views::Label* status_view_ = nullptr;
  ProportionalImageView* icon_view_ = nullptr;
  views::View* image_container_view_ = nullptr;
  std::vector<BraveNotificationButtonMD*> action_buttons_;
  std::vector<views::View*> item_views_;
  views::ProgressBar* progress_bar_view_ = nullptr;
  CompactTitleBraveMessageView* compact_title_message_view_ = nullptr;
  views::View* action_buttons_row_ = nullptr;
  BraveNotificationInputContainerMD* inline_reply_ = nullptr;

  // Counter for view layouting, which is used during the CreateOrUpdate*
  // phases to keep track of the view ordering. See crbug.com/901045
  int left_content_count_;

  // Views for inline settings.
  views::RadioButton* block_all_button_ = nullptr;
  views::RadioButton* dont_block_button_ = nullptr;
  views::LabelButton* settings_done_button_ = nullptr;

  // Owned by views properties. Guaranteed to be not null for the lifetime of
  // |this| because views properties are the last thing cleaned up.
  BraveNotificationViewMDPathGenerator* highlight_path_generator_ = nullptr;

  std::unique_ptr<ui::EventHandler> click_activator_;

  base::TimeTicks last_mouse_pressed_timestamp_;

  base::WeakPtrFactory<BraveNotificationViewMD> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BraveNotificationViewMD);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_MD_H_
