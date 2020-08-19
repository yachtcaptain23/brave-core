/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "ui/message_center/views/message_popup_view.h"

#import <Cocoa/Cocoa.h>

#include "ui/views/widget/widget.h"

namespace message_center {

float MessagePopupView::GetOpacity() const {
  if (!IsWidgetValid())
    return 0.f;
  return [GetWidget()->GetNativeWindow().GetNativeNSWindow() alphaValue];
}

}  // namespace message_center
