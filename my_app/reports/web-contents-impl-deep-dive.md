# WebContentsImpl — Deep Dive

## 1. What It Is

`WebContentsImpl` is the **central hub** of Chromium's `//content` layer. It represents a single web page (a "tab" in browser terms) and acts as the glue connecting frames, navigation, rendering, input, accessibility, and the embedder.

It is the concrete implementation of the public `content::WebContents` interface.

### Code Statistics

| File | Lines |
|------|-------|
| `web_contents_impl.h` | 2,820 |
| `web_contents_impl.cc` | 12,655 |
| `web_contents.h` (public API) | 1,863 |
| `web_contents_delegate.h` | 974 |
| `web_contents_observer.h` | 1,034 |
| **Total** | **19,346** |

- Implements **393+** methods (counted `void WebContentsImpl::` alone)
- 45 files in `content/browser/web_contents/` directory
- One of the largest classes in all of Chromium

---

## 2. Class Hierarchy

`WebContentsImpl` inherits from **14 interfaces**, acting as the delegate/hub for nearly every subsystem:

```cpp
class WebContentsImpl
    : public WebContents,                                    // Public API
      public FrameTree::Delegate,                            // Frame tree lifecycle
      public RenderFrameHostDelegate,                        // Frame events (close, dialogs, fullscreen)
      public RenderViewHostDelegate,                         // View-level events
      public RenderWidgetHostDelegate,                       // Widget/input events
      public RenderFrameHostManager::Delegate,               // Process swaps, speculative RFH
      public PageDelegate,                                   // Page-level events
      public blink::mojom::ColorChooserFactory,              // Color picker creation
      public NavigationControllerDelegate,                   // History/navigation state changes
      public NavigatorDelegate,                              // Navigation decisions
      public ui::NativeThemeObserver,                        // Dark mode / forced colors
      public ui::ColorProviderSourceObserver,                // Color provider changes
      public SlowWebPreferenceCacheObserver,                 // Preference cache invalidation
      public input::RenderWidgetHostInputEventRouter::Delegate  // Input event routing
```

And the public interface:

```cpp
class WebContents
    : public PageNavigator,            // OpenURL capability
      public base::SupportsUserData    // Attach arbitrary data (WebContentsUserData)
```

### Why So Many Interfaces?

Each subsystem in `//content` (frames, navigation, rendering, input) defines its own delegate interface. `WebContentsImpl` implements all of them because it's the single object that has enough context to coordinate between subsystems. The delegate pattern keeps subsystems decoupled — `RenderFrameHostImpl` doesn't know about `WebContents`, it only knows about `RenderFrameHostDelegate`.

```
                    WebContentsImpl
                    (implements all delegates)
                          │
            ┌─────────────┼─────────────────┐
            ▼             ▼                  ▼
    FrameTree      NavigationController   RenderWidgetHost
    (calls FrameTree::  (calls Navigation    (calls RenderWidget
     Delegate)           ControllerDelegate)  HostDelegate)
            │             │                  │
            ▼             ▼                  ▼
    RenderFrameHostImpl  NavigationRequest  InputEventRouter
    (calls RenderFrame   (calls Navigator    (calls Input
     HostDelegate)        Delegate)           Delegate)
```

---

## 3. Creation & Lifecycle

### Factory Methods

```cpp
// Public API — creates a new WebContents
static std::unique_ptr<WebContents> WebContents::Create(
    const WebContents::CreateParams& params);

// Internal implementation
static std::unique_ptr<WebContentsImpl> WebContentsImpl::CreateWithOpener(
    const WebContents::CreateParams& params,
    RenderFrameHostImpl* opener_rfh);
```

### CreateParams

```cpp
struct CreateParams {
    BrowserContext* browser_context;              // Required — profile/storage
    scoped_refptr<SiteInstance> site_instance;     // Optional — pre-selected process
    bool initially_hidden = false;                // Start hidden (for prerender)
    gfx::NativeView context = gfx::NativeView();  // Parent native view
    // ... opener, renderer preferences, etc.
};
```

### Construction Flow

```
WebContents::Create(params)
  └─ WebContentsImpl::CreateWithOpener(params, nullptr)
       ├─ new WebContentsImpl(browser_context)
       │   ├─ Initialize primary_frame_tree_
       │   ├─ Create AudioStreamMonitor
       │   ├─ Create PrerenderHostRegistry
       │   ├─ Set renderer preferences (accept_languages, etc.)
       │   └─ Register with global WebContents tracking
       │
       ├─ Init(params)
       │   ├─ Create WebContentsView (platform-specific)
       │   │   ├─ WebContentsViewAura (Linux/Windows/ChromeOS)
       │   │   ├─ WebContentsViewMac (macOS)
       │   │   ├─ WebContentsViewAndroid (Android)
       │   │   └─ WebContentsViewIOS (iOS)
       │   ├─ Create RenderWidgetHostView
       │   └─ Initialize the FrameTree
       │       └─ Creates initial RenderFrameHostImpl (main frame)
       │
       └─ Return unique_ptr<WebContentsImpl>
```

### Destruction

```
~WebContentsImpl()
  ├─ Notify observers: WebContentsDestroyed()
  ├─ Clear all observers
  ├─ Destroy frame tree (kills all RenderFrameHosts)
  ├─ Destroy the view
  └─ Unregister from global tracking
```

---

## 4. Key Member Variables

### Core State

| Variable | Type | Purpose |
|----------|------|---------|
| `primary_frame_tree_` | `FrameTree` | The tree of frames (main frame + iframes) |
| `delegate_` | `raw_ptr<WebContentsDelegate>` | Embedder callbacks (1:1, set by embedder) |
| `view_` | `unique_ptr<WebContentsView>` | Platform-specific native view |
| `node_` | `unique_ptr<WebContentsTreeNode>` | Inner/outer WebContents relationships (portals, GuestViews) |
| `observers_` | `ObserverList<WebContentsObserver>` | All registered observers |

### Navigation & History

| Variable | Type | Purpose |
|----------|------|---------|
| `controller_` | (via FrameTree) | Navigation history (back/forward/reload) |
| `should_focus_location_bar_by_default_` | `bool` | Whether initial nav focuses the URL bar |

### Rendering & Input

| Variable | Type | Purpose |
|----------|------|---------|
| `rwh_input_event_router_` | `unique_ptr<InputEventRouter>` | Routes input to correct frame (OOPIF support) |
| `text_input_manager_` | `unique_ptr<TextInputManager>` | IME/text input coordination |
| `web_preferences_` | `optional<blink::web_pref::WebPreferences>` | Cached renderer preferences |
| `page_base_background_color_` | `optional<SkColor>` | Background color set by embedder |

### Media & Audio

| Variable | Type | Purpose |
|----------|------|---------|
| `audio_stream_monitor_` | `AudioStreamMonitor` | Tracks audio playback state |
| `is_currently_audible_` | `bool` | Whether the page is producing audio |
| `was_ever_audible_` | `bool` | Whether the page ever produced audio |
| `cached_video_sizes_` | `map<MediaPlayerId, gfx::Size>` | Video element sizes |
| `has_persistent_video_` | `bool` | Persistent video (PiP candidate) |

### Fullscreen

| Variable | Type | Purpose |
|----------|------|---------|
| `fullscreen_frames_` | `set<RenderFrameHostImpl*>` | Frames in fullscreen mode |
| `current_fullscreen_frame_id_` | `GlobalRenderFrameHostId` | Currently fullscreen frame |

### Visibility & Display

| Variable | Type | Purpose |
|----------|------|---------|
| `visibility_` | `Visibility` | VISIBLE / OCCLUDED / HIDDEN |
| `is_overlay_content_` | `bool` | Whether this is overlay content |
| `is_popup_` | `bool` | Whether this is a popup window |

### Prerendering

| Variable | Type | Purpose |
|----------|------|---------|
| `prerender_host_registry_` | `unique_ptr<PrerenderHostRegistry>` | Manages prerendered pages |

### Weak Pointers

| Variable | Purpose |
|----------|---------|
| `loading_weak_factory_` | Weak ptrs invalidated when loading state changes |
| `weak_factory_` | General weak pointer factory (invalidated on destruction) |

---

## 5. The Delegate Pattern

`WebContentsDelegate` is the **embedder's primary interface** to `WebContents`. There is exactly **one delegate per WebContents** (set by the embedder).

### How my_app Uses It

```cpp
class AppWindow : public content::WebContentsDelegate {
  void CloseContents(WebContents* source) override;
  void TitleWasSet(NavigationEntry* entry) override;
};

// In AppWindow::Create():
web_contents_->SetDelegate(this);
```

### Key Delegate Methods (974 lines, ~80 virtual methods)

**Window Management:**

| Method | When Called |
|--------|------------|
| `AddNewContents(source, new_contents, disposition, rect, user_gesture)` | `window.open()` or target=_blank link creates new tab/window |
| `CloseContents(source)` | `window.close()` or renderer requests close |
| `ActivateContents(source)` | Page requests focus (e.g., `window.focus()`) |
| `SetContentsBounds(source, bounds)` | `window.moveTo()` / `window.resizeTo()` |
| `IsContentsActive(source)` | Query if tab is active |

**Navigation & Loading:**

| Method | When Called |
|--------|------------|
| `OpenURLFromTab(source, params)` | Navigation that needs a new tab (target=_blank, etc.) |
| `NavigationStateChanged(source, flags)` | URL, title, loading state, or security state changed |
| `LoadingStateChanged(source, should_show_loading_ui)` | Loading spinner state changed |
| `ShouldFocusLocationBarByDefault(source)` | Whether to focus URL bar after navigation |

**Dialogs & UI:**

| Method | When Called |
|--------|------------|
| `DidAddMessageToConsole(source, level, message, line, source_id)` | JS console.log/warn/error |
| `ShouldSuppressDialogs(source)` | Whether to block alert/confirm/prompt |
| `BeforeUnloadFired(source, proceed, did_cancel)` | beforeunload dialog result |
| `RunFileChooser(rfh, listener, params)` | `<input type=file>` clicked |

**Fullscreen:**

| Method | When Called |
|--------|------------|
| `EnterFullscreenModeForTab(rfh, options)` | Element enters fullscreen |
| `ExitFullscreenModeForTab(source)` | Exit fullscreen |
| `IsFullscreenForTabOrPending(source)` | Query fullscreen state |

**Security:**

| Method | When Called |
|--------|------------|
| `VisibleSecurityStateChanged(source)` | HTTPS/certificate state changed |

**Drag & Drop:**

| Method | When Called |
|--------|------------|
| `StartDragging(rfh, drag_data, ops, image, cursor_offset)` | Drag initiated from web content |

---

## 6. The Observer Pattern

`WebContentsObserver` provides **read-only notifications** about WebContents events. Multiple observers can be registered (many:1 relationship).

### How my_app Uses It

```cpp
class AppWindow : public content::WebContentsObserver {
  void TitleWasSet(NavigationEntry* entry) override;
};

// In constructor:
Observe(web_contents_.get());
```

### Key Observer Methods (1,034 lines, ~100+ virtual methods)

**Navigation Lifecycle:**

| Method | When |
|--------|------|
| `DidStartNavigation(NavigationHandle*)` | Navigation begins |
| `DidRedirectNavigation(NavigationHandle*)` | HTTP redirect |
| `ReadyToCommitNavigation(NavigationHandle*)` | Response received, about to commit |
| `DidFinishNavigation(NavigationHandle*)` | Committed or failed |

**Loading:**

| Method | When |
|--------|------|
| `DidStartLoading()` | First frame begins loading |
| `DOMContentLoaded(RenderFrameHost*)` | DOMContentLoaded event |
| `DidFinishLoad(RenderFrameHost*, GURL)` | Frame's load event fired |
| `DidStopLoading()` | All frames done loading |
| `DidFailLoad(RenderFrameHost*, GURL, int error)` | Load failed |

**Frame Lifecycle:**

| Method | When |
|--------|------|
| `RenderFrameCreated(RenderFrameHost*)` | New frame created |
| `RenderFrameDeleted(RenderFrameHost*)` | Frame destroyed |
| `RenderFrameHostChanged(old_rfh, new_rfh)` | RFH swapped (cross-process navigation) |
| `FrameDeleted(FrameTreeNodeId)` | Frame tree node removed |

**Page Lifecycle:**

| Method | When |
|--------|------|
| `PrimaryPageChanged(Page&)` | Primary page changed (commit, BFCache restore, prerender activation) |
| `TitleWasSet(NavigationEntry*)` | document.title changed |

**Process Events:**

| Method | When |
|--------|------|
| `PrimaryMainFrameRenderProcessGone(TerminationStatus)` | Renderer crashed or killed |
| `OnRendererUnresponsive(RenderProcessHost*)` | Renderer hung |
| `OnRendererResponsive(RenderProcessHost*)` | Renderer recovered |

**Visibility:**

| Method | When |
|--------|------|
| `OnVisibilityChanged(Visibility)` | Tab visible/occluded/hidden |

**Media:**

| Method | When |
|--------|------|
| `MediaStartedPlaying(MediaPlayerInfo, MediaPlayerId)` | Media element started |
| `MediaStoppedPlaying(MediaPlayerInfo, MediaPlayerId, reason)` | Media element stopped |
| `OnAudioStateChanged(bool audible)` | Audio state changed |

**Destruction:**

| Method | When |
|--------|------|
| `WebContentsDestroyed()` | WebContents being destroyed (last notification) |

### Rule: Observer Methods Must Return Void

All observer methods return `void`. This ensures observer registration order doesn't matter — no observer can "consume" an event or affect the outcome.

---

## 7. Frame Tree Management

`WebContentsImpl` owns the `primary_frame_tree_`, which is the tree of all frames in the page.

```
WebContentsImpl
  └─ primary_frame_tree_ (FrameTree)
       └─ root_ (FrameTreeNode) — main frame
            ├─ current_frame_host_ (RenderFrameHostImpl)
            │   └─ render_frame_host_manager_ (RenderFrameHostManager)
            │       └─ speculative_rfh_ (during cross-process nav)
            ├─ child_nodes_[0] (FrameTreeNode) — iframe 1
            │   └─ current_frame_host_
            └─ child_nodes_[1] (FrameTreeNode) — iframe 2
                └─ current_frame_host_ (possibly different process)
```

### Key FrameTree::Delegate Methods (implemented by WebContentsImpl)

| Method | Purpose |
|--------|---------|
| `CanAccessInitialDocument()` | Whether scripts can access initial empty document |
| `NotifySwappedFromRenderManager()` | RenderFrameHost was swapped (cross-process navigation) |
| `GetFrameTree()` | Return the frame tree |
| `CreateNewWindow()` | `window.open()` — create a new WebContents |
| `ShowCreatedWindow()` | Show the newly created window |

### WebContents ↔ RenderFrameHost Relationship

```
WebContentsImpl
  │ GetPrimaryMainFrame() → the main frame's RenderFrameHost
  │ GetAllFrames() → all RenderFrameHosts in the tree
  │ ForEachRenderFrameHost(callback) → iterate all frames
  │
  │ implements RenderFrameHostDelegate:
  │   OnDidFinishLoad()        ← called by RFH
  │   RenderFrameCreated()     ← called when new frame appears
  │   RenderFrameDeleted()     ← called when frame disappears
  │   ShowContextMenu()        ← right-click in a frame
  │   RunJavaScriptDialog()    ← alert/confirm/prompt from a frame
  │   Close()                  ← window.close() from main frame
  │   EnterFullscreenMode()    ← requestFullscreen() from a frame
  │   CreateNewWindow()        ← window.open() from a frame
  │   UpdateTitle()            ← document.title changed in a frame
  │   DidChangeName()          ← frame name attribute changed
  │   RunBeforeUnloadConfirm() ← beforeunload dialog needed
```

---

## 8. Navigation Integration

WebContentsImpl participates in navigation through multiple delegate interfaces:

### NavigationControllerDelegate

Called by `NavigationController` (session history):

| Method | Purpose |
|--------|---------|
| `NotifyNavigationEntryCommitted()` | History entry committed |
| `NotifyNavigationListPruned()` | Old entries removed |
| `NotifyNavigationEntryChanged()` | Entry updated |

### NavigatorDelegate

Called during the navigation lifecycle:

| Method | Purpose |
|--------|---------|
| `DidStartNavigation()` | Fire observer notifications |
| `DidRedirectNavigation()` | Fire observer notifications |
| `ReadyToCommitNavigation()` | Fire observer notifications |
| `DidFinishNavigation()` | Fire observer notifications, update loading state |
| `DidCommitAndDrawCompositorFrame()` | First paint after navigation |

### The OpenURL Flow

```
Embedder calls:
  WebContents::OpenURL(OpenURLParams)
    │
    └─ WebContentsImpl::OpenURL()
         ├─ If same WebContents: NavigationController::LoadURLWithParams()
         └─ If new tab needed: delegate_->OpenURLFromTab()
              └─ Embedder creates new WebContents + navigates
```

---

## 9. Input Event Routing

For pages with out-of-process iframes (OOPIF), input events must be routed to the correct frame's renderer process.

```
OS Input Event (mouse click at x,y)
  │
  ├─ WebContentsViewAura receives it
  │
  ├─ RenderWidgetHostInputEventRouter::RouteMouseEvent()
  │   ├─ Hit-test the compositor frame (viz surface layer tree)
  │   ├─ Determine which RenderWidgetHostView is at (x,y)
  │   └─ Forward event to correct RWHV (possibly cross-process iframe)
  │
  └─ RenderWidgetHostImpl dispatches to renderer via Mojo
```

WebContentsImpl owns the `rwh_input_event_router_` and implements `RenderWidgetHostInputEventRouter::Delegate` to coordinate this.

---

## 10. WebContentsUserData Pattern

Embedders and features attach per-WebContents data using the CRTP `WebContentsUserData` pattern:

```cpp
// Define a tab helper:
class FooTabHelper : public content::WebContentsUserData<FooTabHelper> {
 public:
  ~FooTabHelper() override;

  void DoSomething();

 private:
  friend WebContentsUserData;
  explicit FooTabHelper(content::WebContents* web_contents);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

// Attach to a WebContents:
FooTabHelper::CreateForWebContents(web_contents);

// Retrieve:
FooTabHelper* helper = FooTabHelper::FromWebContents(web_contents);
```

This uses `base::SupportsUserData` (inherited by `WebContents`) to store arbitrary typed data keyed by a static `void*`. The data is automatically destroyed when the WebContents is destroyed.

### Scoping Alternatives

| Class | Lifetime Scope |
|-------|---------------|
| `WebContentsUserData` | Whole WebContents lifetime |
| `DocumentUserData` | One document in one frame (destroyed on navigation) |
| `NavigationHandleUserData` | One navigation attempt |
| `PageUserData` | One Page (multiple frames sharing same page) |

Prefer the most specific scope to avoid data leaking across navigations.

---

## 11. Platform Views

`WebContentsImpl` delegates platform-specific rendering to `WebContentsView`:

| Class | Platform | Responsibility |
|-------|----------|----------------|
| `WebContentsViewAura` | Linux, Windows, ChromeOS | Aura window hosting, drag-and-drop |
| `WebContentsViewMac` | macOS | NSView hosting |
| `WebContentsViewAndroid` | Android | Android View hosting |
| `WebContentsViewIOS` | iOS | UIView hosting |
| `WebContentsViewChildFrame` | All | OOPIF inner frames |

The view provides:
- `GetNativeView()` — the platform window/view handle
- `CreateView()` — create the platform rendering surface
- `SetPageTitle()` — update native window title
- `StoreFocus()` / `RestoreFocus()` — focus management
- `StartDragging()` — initiate OS drag-and-drop
- `GetContainerBounds()` — get container geometry

---

## 12. How my_app Interacts with WebContentsImpl

```
AppWindow
  │
  ├─ Creates: WebContents::Create(CreateParams{browser_context})
  │   └─ Returns a WebContentsImpl (behind WebContents interface)
  │
  ├─ Sets delegate: web_contents_->SetDelegate(this)
  │   └─ AppWindow receives CloseContents, DidAddMessageToConsole
  │
  ├─ Observes: Observe(web_contents_.get())
  │   └─ AppWindow receives TitleWasSet
  │
  ├─ Navigates: web_contents_->GetController().LoadURLWithParams(params)
  │   └─ Triggers full navigation pipeline
  │
  ├─ Displays: views::WebView::SetWebContents(web_contents_.get())
  │   └─ WebContentsView provides the native view for embedding
  │
  └─ Destroys: web_contents_.reset() in ~AppWindow
       └─ Fires WebContentsDestroyed(), cleans up everything
```

### What my_app Could Use But Doesn't Yet

| API | Purpose |
|-----|---------|
| `web_contents->GetPrimaryMainFrame()` | Access the main frame's RFH |
| `web_contents->ForEachRenderFrameHost(cb)` | Iterate all frames |
| `web_contents->GetVisibleURL()` | Current URL in the address bar |
| `web_contents->GetLastCommittedURL()` | Last committed URL |
| `web_contents->IsLoading()` | Loading spinner state |
| `web_contents->Stop()` | Stop loading |
| `web_contents->GetController().GoBack()` | Navigate back |
| `web_contents->GetController().GoForward()` | Navigate forward |
| `web_contents->GetController().Reload()` | Reload |
| `WebContentsObserver::DidStartNavigation()` | Track navigation progress |
| `WebContentsObserver::DidFinishNavigation()` | Navigation completed |
| `WebContentsObserver::PrimaryMainFrameRenderProcessGone()` | Handle renderer crash |
| `WebContentsDelegate::AddNewContents()` | Handle window.open() |
| `WebContentsDelegate::RunFileChooser()` | Handle `<input type=file>` |

---

## 13. Architectural Diagram

```
                         ┌───────────────────────────────────┐
                         │          EMBEDDER                 │
                         │  (Chrome / my_app / WebView)      │
                         │                                   │
                         │  WebContentsDelegate (1:1)        │
                         │  WebContentsObserver (many:1)     │
                         │  WebContentsUserData (many:1)     │
                         └─────────────┬─────────────────────┘
                                       │ SetDelegate / Observe / CreateForWebContents
                                       ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                        WebContentsImpl                                   │
│                                                                          │
│  ┌─────────────┐  ┌──────────────────┐  ┌────────────────────────────┐  │
│  │ FrameTree   │  │ NavigationCtrl   │  │ InputEventRouter          │  │
│  │  └─ FTNode  │  │  └─ NavEntries   │  │  └─ hit-test → route     │  │
│  │     └─ RFH  │  │                  │  │     to correct RWHV       │  │
│  │     └─ RFPH │  │                  │  │                           │  │
│  └──────┬──────┘  └────────┬─────────┘  └──────────┬────────────────┘  │
│         │                  │                       │                    │
│  implements:               │                       │                    │
│  FrameTree::Delegate       │                       │                    │
│  RenderFrameHostDelegate   │ NavigationController   │ RenderWidgetHost   │
│  RenderViewHostDelegate    │ Delegate               │ Delegate           │
│  RFHManager::Delegate      │ NavigatorDelegate      │ InputRouter::Delegate│
│  PageDelegate              │                       │                    │
│                            │                       │                    │
│  ┌─────────────┐  ┌───────┴──────┐  ┌─────────────┴──────┐            │
│  │ WebContents │  │ AudioStream  │  │ PrerenderHost      │            │
│  │ View        │  │ Monitor      │  │ Registry           │            │
│  │ (platform)  │  │              │  │                    │            │
│  └─────────────┘  └──────────────┘  └────────────────────┘            │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
         │                    │                    │
         ▼                    ▼                    ▼
   RenderFrameHostImpl  NavigationRequest    RenderWidgetHostImpl
   (browser-side frame) (one navigation)    (browser-side widget)
         │                    │                    │
         │              Mojo IPC              Mojo IPC
         ▼                    ▼                    ▼
   RenderFrameImpl     Network Service     RenderWidgetImpl
   (renderer process)  (fetches URLs)      (renderer process)
```

---

## 14. Source References

| File | Purpose |
|------|---------|
| `content/public/browser/web_contents.h` | Public API (1,863 lines) |
| `content/public/browser/web_contents_delegate.h` | Embedder delegate interface (974 lines) |
| `content/public/browser/web_contents_observer.h` | Observer interface (1,034 lines) |
| `content/public/browser/web_contents_user_data.h` | UserData CRTP pattern |
| `content/browser/web_contents/web_contents_impl.h` | Implementation header (2,820 lines) |
| `content/browser/web_contents/web_contents_impl.cc` | Implementation (12,655 lines) |
| `content/browser/web_contents/web_contents_view_aura.cc` | Linux/Windows view |
| `content/browser/web_contents/web_contents_view_mac.mm` | macOS view |
