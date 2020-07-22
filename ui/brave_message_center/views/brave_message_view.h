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
class BraveNotificationControlButtonsView;
class MessageView;

// An base class for a notification entry. Contains background and other
// elements shared by derived notification views.
class MESSAGE_CENTER_EXPORT BraveMessageView : public MessageView {};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_MESSAGE_VIEW_H_
