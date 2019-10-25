// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_MAC_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_MAC_H_

#import <Cocoa/Cocoa.h>

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "content/browser/frame_host/popup_menu_helper_mac.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/content_export.h"
#include "content/common/drag_event_source_info.h"
#include "content/common/web_contents_ns_view_bridge.mojom.h"
#include "content/public/browser/visibility.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#import "ui/base/cocoa/views_hostable.h"
#include "ui/gfx/geometry/size.h"

@class WebContentsViewCocoa;
@class WebDragDest;

namespace content {
class RenderWidgetHostViewMac;
class WebContentsImpl;
class WebContentsViewDelegate;
class WebContentsViewMac;
}

namespace gfx {
class Vector2d;
}

namespace remote_cocoa {
class WebContentsNSViewBridge;
}  // remote_cocoa

namespace content {

// Mac-specific implementation of the WebContentsView. It owns an NSView that
// contains all of the contents of the tab and associated child views.
class WebContentsViewMac : public WebContentsView,
                           public RenderViewHostDelegateView,
                           public PopupMenuHelper::Delegate,
                           public remote_cocoa::mojom::WebContentsNSViewHost,
                           public ui::ViewsHostableView {
 public:
  // The corresponding WebContentsImpl is passed in the constructor, and manages
  // our lifetime. This doesn't need to be the case, but is this way currently
  // because that's what was easiest when they were split.
  WebContentsViewMac(WebContentsImpl* web_contents,
                     WebContentsViewDelegate* delegate);
  ~WebContentsViewMac() override;

  // WebContentsView implementation --------------------------------------------
  gfx::NativeView GetNativeView() const override;
  gfx::NativeView GetContentNativeView() const override;
  gfx::NativeWindow GetTopLevelNativeWindow() const override;
  void GetContainerBounds(gfx::Rect* out) const override;
  void SizeContents(const gfx::Size& size) override;
  void Focus() override;
  void SetInitialFocus() override;
  void StoreFocus() override;
  void RestoreFocus() override;
  void FocusThroughTabTraversal(bool reverse) override;
  DropData* GetDropData() const override;
  gfx::Rect GetViewBounds() const override;
  void CreateView(gfx::NativeView context) override;
  RenderWidgetHostViewBase* CreateViewForWidget(
      RenderWidgetHost* render_widget_host,
      bool is_guest_view_hack) override;
  RenderWidgetHostViewBase* CreateViewForChildWidget(
      RenderWidgetHost* render_widget_host) override;
  void SetPageTitle(const base::string16& title) override;
  void RenderViewCreated(RenderViewHost* host) override;
  void RenderViewReady() override;
  void RenderViewHostChanged(RenderViewHost* old_host,
                             RenderViewHost* new_host) override;
  void SetOverscrollControllerEnabled(bool enabled) override;
  bool CloseTabAfterEventTrackingIfNeeded() override;

  // RenderViewHostDelegateView:
  void StartDragging(const DropData& drop_data,
                     blink::WebDragOperationsMask allowed_operations,
                     const gfx::ImageSkia& image,
                     const gfx::Vector2d& image_offset,
                     const DragEventSourceInfo& event_info,
                     RenderWidgetHostImpl* source_rwh) override;
  void UpdateDragCursor(blink::WebDragOperation operation) override;
  void GotFocus(RenderWidgetHostImpl* render_widget_host) override;
  void LostFocus(RenderWidgetHostImpl* render_widget_host) override;
  void TakeFocus(bool reverse) override;
  void ShowContextMenu(RenderFrameHost* render_frame_host,
                       const ContextMenuParams& params) override;
  void ShowPopupMenu(RenderFrameHost* render_frame_host,
                     const gfx::Rect& bounds,
                     int item_height,
                     double item_font_size,
                     int selected_item,
                     const std::vector<MenuItem>& items,
                     bool right_aligned,
                     bool allow_multiple_selection) override;
  void HidePopupMenu() override;

  // PopupMenuHelper::Delegate:
  void OnMenuClosed() override;

  // ViewsHostableView:
  void ViewsHostableAttach(ViewsHostableView::Host* host) override;
  void ViewsHostableDetach() override;
  void ViewsHostableSetBounds(const gfx::Rect& bounds_in_window) override;
  void ViewsHostableSetVisible(bool visible) override;
  void ViewsHostableMakeFirstResponder() override;
  void ViewsHostableSetParentAccessible(
      gfx::NativeViewAccessible parent_accessibility_element) override;
  gfx::NativeViewAccessible ViewsHostableGetAccessibilityElement() override;

  // A helper method for closing the tab in the
  // CloseTabAfterEventTracking() implementation.
  void CloseTab();

  WebContentsImpl* web_contents() { return web_contents_; }
  WebContentsViewDelegate* delegate() { return delegate_.get(); }
  WebDragDest* drag_dest() const { return drag_dest_.get(); }

  using RenderWidgetHostViewCreateFunction =
      RenderWidgetHostViewMac* (*)(RenderWidgetHost*, bool);

  // Used to override the creation of RenderWidgetHostViews in tests.
  CONTENT_EXPORT static void InstallCreateHookForTests(
      RenderWidgetHostViewCreateFunction create_render_widget_host_view);

 private:
  WebContentsViewCocoa* GetInProcessNSView() const;

  // remote_cocoa::mojom::WebContentsNSViewHost:
  void OnMouseEvent(bool motion, bool exited) override;
  void OnBecameFirstResponder(
      remote_cocoa::mojom::SelectionDirection direction) override;
  void OnWindowVisibilityChanged(
      remote_cocoa::mojom::Visibility visibility) override;
  void SetDropData(const DropData& drop_data) override;
  bool DraggingEntered(remote_cocoa::mojom::DraggingInfoPtr dragging_info,
                       uint32_t* out_result) override;
  void DraggingExited() override;
  bool DraggingUpdated(remote_cocoa::mojom::DraggingInfoPtr dragging_info,
                       uint32_t* out_result) override;
  bool PerformDragOperation(remote_cocoa::mojom::DraggingInfoPtr dragging_info,
                            bool* out_result) override;
  bool DragPromisedFileTo(const base::FilePath& file_path,
                          const DropData& drop_data,
                          const GURL& download_url,
                          base::FilePath* out_file_path) override;
  void EndDrag(uint32_t drag_opeation,
               const gfx::PointF& local_point,
               const gfx::PointF& screen_point) override;

  // remote_cocoa::mojom::WebContentsNSViewHost, synchronous methods:
  void DraggingEntered(remote_cocoa::mojom::DraggingInfoPtr dragging_info,
                       DraggingEnteredCallback callback) override;
  void DraggingUpdated(remote_cocoa::mojom::DraggingInfoPtr dragging_info,
                       DraggingUpdatedCallback callback) override;
  void PerformDragOperation(remote_cocoa::mojom::DraggingInfoPtr dragging_info,
                            PerformDragOperationCallback callback) override;
  void DragPromisedFileTo(const base::FilePath& file_path,
                          const DropData& drop_data,
                          const GURL& download_url,
                          DragPromisedFileToCallback callback) override;

  // Return the list of child RenderWidgetHostViewMacs. This will remove any
  // destroyed instances before returning.
  std::list<RenderWidgetHostViewMac*> GetChildViews();

  // The WebContentsImpl whose contents we display.
  WebContentsImpl* web_contents_;

  // Destination for drag-drop.
  base::scoped_nsobject<WebDragDest> drag_dest_;

  // Tracks the RenderWidgetHost where the current drag started.
  base::WeakPtr<content::RenderWidgetHostImpl> drag_source_start_rwh_;

  // Our optional delegate.
  std::unique_ptr<WebContentsViewDelegate> delegate_;

  // This contains all RenderWidgetHostViewMacs that have been added as child
  // NSViews to this NSView. Note that this list may contain RWHVMacs besides
  // just |web_contents_->GetRenderWidgetHostView()|. The only time that the
  // RWHVMac's NSView is removed from the WCVMac's NSView is when it is
  // destroyed.
  std::list<base::WeakPtr<RenderWidgetHostViewBase>> child_views_;

  // Interface to the views::View host of this view.
  ViewsHostableView::Host* views_host_ = nullptr;

  // The accessibility element specified via ViewsHostableSetParentAccessible.
  gfx::NativeViewAccessible views_host_accessibility_element_ = nil;

  std::unique_ptr<PopupMenuHelper> popup_menu_helper_;

  // The id that may be used to look up this NSView.
  const uint64_t ns_view_id_;

  // The WebContentsViewCocoa that lives in the NSView hierarchy in this
  // process. This is always non-null, even when the view is being displayed
  // in another process.
  std::unique_ptr<remote_cocoa::WebContentsNSViewBridge>
      in_process_ns_view_bridge_;

  // Mojo bindings for an out of process instance of this NSView.
  remote_cocoa::mojom::WebContentsNSViewAssociatedPtr remote_ns_view_;
  mojo::AssociatedBinding<remote_cocoa::mojom::WebContentsNSViewHost>
      remote_ns_view_host_binding_;

  // Used by CloseTabAfterEventTrackingIfNeeded.
  base::WeakPtrFactory<WebContentsViewMac> deferred_close_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsViewMac);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_MAC_H_
