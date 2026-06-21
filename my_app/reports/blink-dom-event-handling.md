# Blink DOM Event Handling

How a mouse click on an `<a>` link becomes a navigation request — the full pipeline from OS event to `BeginNavigation` IPC.

---

## The Full Pipeline

```
OS mouse click
  → Browser delivers WebInputEvent via Mojo IPC
    → Blink input routing (hit-test, pointer/mouse managers)
      → Synthesize "click" DOM event
        → DOM event dispatch (capture → target → bubble)
          → Default event handler (HTMLAnchorElement)
            → Frame::Navigate()
              → RenderFrameImpl::BeginNavigation() IPC to browser
```

---

## Stage 1: Browser → Renderer Input Delivery

The OS input event (mouse click) arrives at the **browser process** first, not the renderer. The browser:

1. Receives the OS event (via Aura/platform window)
2. Hit-tests the compositor frame to find which renderer/frame to target
3. Routes the event via Mojo IPC to the correct renderer process

```
Browser Process                          Renderer Process
  │                                        │
  ├─ OS mouse click event                  │
  ├─ InputEventRouter::RouteMouseEvent()   │
  ├─ Hit-test compositor surface           │
  ├─ Find target RenderWidgetHostView      │
  │                                        │
  ├─ Send WebMouseEvent via Mojo ─────────►│
  │                                        ├─ WebFrameWidgetImpl::
  │                                        │    HandleInputEvent()
  │                                        │
```

### `WebFrameWidgetImpl::HandleInputEvent()`

The entry point for all input in Blink. It:
- Checks for provisional frames, bfcache, DevTools interception
- Handles pointer lock and mouse capture as special cases
- Delegates to `WidgetEventHandler::HandleInputEvent()`
- Which routes to `EventHandler` based on event type

---

## Stage 2: EventHandler — Input Routing

`EventHandler` (`third_party/blink/renderer/core/input/event_handler.h`) is the top-level input router in Blink. It owns specialized sub-managers:

```
EventHandler
  ├─ MouseEventManager        ← mouse press/release/move/click
  ├─ PointerEventManager       ← pointer events (unified mouse/touch/pen)
  ├─ GestureManager            ← tap, scroll, pinch gestures
  ├─ KeyboardEventManager      ← keydown/keypress/keyup
  └─ ScrollManager             ← scroll handling
```

### Mouse Press

```cpp
// event_handler.cc:844
WebInputEventResult EventHandler::HandleMousePressEvent(
    const WebMouseEvent& mouse_event) {
  // 1. Hit-test at mouse position → find target Node
  HitTestResult result = HitTestResultAtLocation(location);

  // 2. Record the press node (for matching with release)
  mouse_event_manager_->SetMousePressNode(result.InnerNode());

  // 3. Notify user activation (anti-popup-spam)
  LocalFrame::NotifyUserActivation(frame);

  // 4. Dispatch pointerdown + mousedown events
  mouse_event_manager_->HandleMousePressEvent(result);
}
```

### Mouse Release → Click Synthesis

```cpp
// event_handler.cc:1234
WebInputEventResult EventHandler::HandleMouseReleaseEvent(
    const WebMouseEvent& mouse_event) {
  // 1. Hit-test at release position
  HitTestResult result = HitTestResultAtLocation(location);

  // 2. Dispatch pointerup + mouseup events
  DispatchMousePointerEvent(WebInputEvent::Type::kPointerUp, ...);

  // 3. Delegate to MouseEventManager
  mouse_event_manager_->HandleMouseReleaseEvent(result);
  //   └─ If press node == release node:
  //      └─ SYNTHESIZE "click" MouseEvent
  //         └─ target_node->DispatchEvent(click_event)
}
```

**Key insight:** The `click` event is **synthesized by Blink**, not sent by the OS. The OS sends `mousedown` + `mouseup`. Blink creates `click` when press and release target the same node.

---

## Stage 3: DOM Event Dispatch

When the synthesized `click` event is dispatched on the target node, Blink's event dispatch system takes over.

### Entry Point

```cpp
// node.cc:3496
DispatchEventResult Node::DispatchEventInternal(Event& event) {
  return EventDispatcher::DispatchEvent(*this, event);
}
```

### EventDispatcher — The Core Loop

```cpp
// event_dispatcher.cc:72
DispatchEventResult EventDispatcher::DispatchEvent(Node& node, Event& event) {
  EventDispatcher dispatcher(node, event);  // builds EventPath
  return event.DispatchEvent(dispatcher);   // runs the dispatch loop
}
```

### The Dispatch Sequence

```
EventDispatcher::Dispatch()                    [event_dispatcher.cc:181]
  │
  ├─ 1. BUILD EVENT PATH
  │     EventPath walks from target node up through:
  │       target → parent → ... → Document → Window
  │     Handles shadow DOM boundaries (re-targets across shadow roots)
  │
  │     Example for <a> inside <div> inside <body>:
  │       EventPath = [<a>, <div>, <body>, <html>, Document, Window]
  │
  ├─ 2. FIND ACTIVATION TARGET
  │     For "click" events only:
  │     Walk the path to find first node with HasActivationBehavior()
  │       └─ HTMLAnchorElement has it → it becomes activation_target
  │
  ├─ 3. PRE-ACTIVATION BEHAVIOR
  │     LegacyPreActivationBehavior() on activation target
  │       └─ e.g., checkbox saves its state before toggling
  │
  ├─ 4. CAPTURE PHASE (Window → target)
  │     Traverse EventPath in REVERSE order:
  │       Window    [phase: kCapturingPhase]  → run capture listeners
  │       Document  [phase: kCapturingPhase]  → run capture listeners
  │       <html>    [phase: kCapturingPhase]  → run capture listeners
  │       <body>    [phase: kCapturingPhase]  → run capture listeners
  │       <div>     [phase: kCapturingPhase]  → run capture listeners
  │       <a>       [phase: kAtTarget]        → run capture listeners
  │
  │     If any listener calls event.stopPropagation() → stop here
  │     If any listener calls event.preventDefault() → mark canceled
  │
  ├─ 5. BUBBLE PHASE (target → Window)
  │     Traverse EventPath in FORWARD order:
  │       <a>       [phase: kAtTarget]        → run bubble listeners
  │       <div>     [phase: kBubblingPhase]   → run bubble listeners
  │       <body>    [phase: kBubblingPhase]   → run bubble listeners
  │       <html>    [phase: kBubblingPhase]   → run bubble listeners
  │       Document  [phase: kBubblingPhase]   → run bubble listeners
  │       Window    [phase: kBubblingPhase]   → run bubble listeners
  │
  │     If any listener calls event.stopPropagation() → stop here
  │
  └─ 6. POST-DISPATCH (DispatchEventPostProcess)
        │
        ├─ If event NOT canceled (preventDefault not called):
        │   ├─ Fire accessibility click notification
        │   ├─ Call activation_target->RunActivationBehavior()
        │   └─ Call DefaultEventHandler on target, then bubble up
        │       └─ THIS IS WHERE NAVIGATION HAPPENS
        │
        └─ If event WAS canceled:
            └─ Call LegacyDidCancelActivationBehavior()
                └─ e.g., checkbox reverts its state
```

### DispatchEventResult

```cpp
enum class DispatchEventResult {
  kNotCanceled,                   // normal — default action runs
  kCanceledByEventHandler,        // script called preventDefault()
  kCanceledByDefaultEventHandler, // browser default handler canceled
  kCanceledBeforeDispatch,        // suppressed before dispatch
};
```

---

## Stage 4: Default Event Handler Chain

After the capture and bubble phases, if `preventDefault()` was NOT called, Blink runs the **default event handler** chain. This is where browser-native behaviors live (not JS listeners).

The chain bubbles up the DOM, giving each element a chance to handle the event:

```
DispatchEventPostProcess()
  │
  ├─ target->DefaultEventHandler(event)
  │   └─ HTMLAnchorElement::DefaultEventHandler()    ← NAVIGATION TRIGGER
  │
  ├─ target->parent->DefaultEventHandler(event)
  │   └─ HTMLDivElement inherits Node::DefaultEventHandler()
  │
  ├─ ... up to Document
  │
  └─ Document::DefaultEventHandler()
```

### HTMLAnchorElement::DefaultEventHandler

```cpp
// html_anchor_element.cc:191
void HTMLAnchorElementBase::DefaultEventHandler(Event& event) {
  if (IsLink()) {
    // Enter key on focused link → simulate click
    if (IsFocused() && IsEnterKeyKeydownEvent(event) && IsLiveLink()) {
      event.SetDefaultHandled();
      DispatchSimulatedClick(&event);
      return;
    }

    // Actual mouse click on link → NAVIGATE
    if (IsLinkClick(event) && IsLiveLink()) {
      HandleClick(To<MouseEvent>(event));     // ← THIS TRIGGERS NAVIGATION
      return;
    }
  }

  HTMLElement::DefaultEventHandler(event);     // pass to parent handler
}
```

`IsLinkClick()` checks:
- Event type is `click` or `auxclick`
- Button is left-click (or middle-click for auxclick)
- The element has an `href` attribute

### HTMLAnchorElement::HandleClick — The Navigation Trigger

```cpp
// html_anchor_element.cc:504
void HTMLAnchorElementBase::HandleClick(MouseEvent& event) {
  // 1. Mark event as handled
  event.SetDefaultHandled();

  // 2. Get the href URL
  KURL url = GetDocument().CompleteURL(StripLeadingAndTrailingHTMLSpaces(
      FastGetAttribute(html_names::kHrefAttr)));

  // 3. Send <a ping> tracking pings (if ping attribute set)
  SendPings(url);

  // 4. Build ResourceRequest
  ResourceRequest request(url);
  request.SetHasUserGesture(HasTransientUserActivation(frame));
  request.SetRequestorOrigin(GetDocument().GetSecurityOrigin());

  // 5. Determine navigation policy from event
  //    (same tab, new tab, new window, download)
  NavigationPolicy policy = NavigationPolicyFromEvent(event);
  //    Left-click → same tab
  //    Ctrl+click → new tab (background)
  //    Shift+click → new window
  //    Alt+click → download

  // 6. Handle download attribute
  if (HasAttribute(html_names::kDownloadAttr)) {
    request.SetSuggestedFilename(
        FastGetAttribute(html_names::kDownloadAttr));
  }

  // 7. Set target frame (from target="_blank" etc.)
  AtomicString target = GetEffectiveTarget();

  // 8. NAVIGATE
  frame->Navigate(FrameLoadRequest(GetDocument(), request),
                  WebFrameLoadType::kStandard);
  //    └─ FrameLoader::StartNavigation()
  //       └─ RenderFrameImpl::BeginNavigation()
  //          └─ Mojo IPC to browser process
}
```

---

## Stage 5: Frame::Navigate → BeginNavigation IPC

```
HTMLAnchorElement::HandleClick()
  └─ Frame::Navigate(request)
       └─ FrameLoader::StartNavigation(request)
            └─ LocalFrameClient::BeginNavigation(request, ...)
                 └─ RenderFrameImpl::BeginNavigation()
                      │
                      │  Builds:
                      │    CommonNavigationParams (URL, referrer, transition)
                      │    BeginNavigationParams (headers, load flags)
                      │
                      │  Mojo IPC: mojom::FrameHost::BeginNavigation(
                      │      common_params,
                      │      begin_params,
                      │      blob_url_token,
                      │      navigation_client)
                      │
                      └──────────────────────────► Browser Process
                                                    │
                                                    └─ NavigationRequest created
                                                       (see navigation-deep-dive.md)
```

---

## Complete Flow: Click to Navigation

```
Time ──────────────────────────────────────────────────────────────►

OS: mouse button down                    OS: mouse button up
  │                                        │
  ▼                                        ▼
Browser: route WebMouseEvent            Browser: route WebMouseEvent
  │ (press)                                │ (release)
  ▼                                        ▼
Renderer:                               Renderer:
  EventHandler::                           EventHandler::
    HandleMousePressEvent()                  HandleMouseReleaseEvent()
      │                                        │
      ├─ Hit-test → find <a>                   ├─ Hit-test → same <a>
      ├─ Save press node                       ├─ Dispatch pointerup
      ├─ NotifyUserActivation                  ├─ Dispatch mouseup
      └─ Dispatch mousedown                    │
         │                                     ├─ Press node == release node?
         ├─ capture: Window→<a>                │   YES → synthesize click
         └─ bubble:  <a>→Window                │
                                               ▼
                                          Dispatch "click" event
                                               │
                                          ┌────▼──── EventDispatcher ────────┐
                                          │                                  │
                                          │  Build EventPath: <a>→...→Window │
                                          │  Find activation target: <a>     │
                                          │                                  │
                                          │  CAPTURE PHASE (Window → <a>):   │
                                          │    JS listeners run              │
                                          │    (can preventDefault!)         │
                                          │                                  │
                                          │  BUBBLE PHASE (<a> → Window):    │
                                          │    JS listeners run              │
                                          │    (can preventDefault!)         │
                                          │                                  │
                                          │  POST-DISPATCH:                  │
                                          │    NOT canceled?                 │
                                          │    └─ DefaultEventHandler chain  │
                                          │       └─ HTMLAnchorElement::     │
                                          │          DefaultEventHandler()   │
                                          │          └─ HandleClick()        │
                                          │             └─ Frame::Navigate() │
                                          │                └─ BeginNav IPC   │
                                          └──────────────────────────────────┘
                                                              │
                                                              ▼
                                                       Browser Process
                                                       (NavigationRequest)
```

---

## Key Design Decisions

### Why click is synthesized, not from OS

The OS sends `mousedown` + `mouseup`. Blink synthesizes `click` because:
- Click requires press and release on the **same element** (drag away = no click)
- `click` event has DOM-level semantics (bubbles, cancelable, default action)
- Allows Blink to handle complex cases (text selection during press, drag detection)

### Why default handlers run AFTER JS listeners

```
JS listener (capture)  → can preventDefault() to CANCEL navigation
JS listener (bubble)   → can preventDefault() to CANCEL navigation
Default handler        → only runs if NOT canceled → triggers navigation
```

This is how `event.preventDefault()` on a link prevents navigation — it cancels the default action before the default handler chain runs.

### Why default handlers bubble up the DOM

```cpp
// Node::DefaultEventHandler is called on target, then parent, then grandparent...
// This allows a <div> to provide default behavior for events on its children
```

Example: a `<form>` element's default handler for `submit` events fires even when the submit button is the target — because the default handler bubbles up to `<form>`.

---

## Key Classes

| Class | File | Role |
|-------|------|------|
| `EventHandler` | `core/input/event_handler.h` | Top-level input router |
| `MouseEventManager` | `core/input/mouse_event_manager.h` | Mouse-specific logic, click synthesis |
| `PointerEventManager` | `core/input/pointer_event_manager.h` | Unified pointer events |
| `EventDispatcher` | `core/dom/events/event_dispatcher.h` | Runs capture→target→bubble dispatch |
| `EventPath` | `core/dom/events/event_path.h` | Target → Window path (shadow DOM aware) |
| `Event` | `core/dom/events/event.h` | Base event class (phases, cancelable, composed) |
| `EventTarget` | `core/dom/events/event_target.h` | Base for all event targets (Node, Window) |
| `HTMLAnchorElement` | `core/html/html_anchor_element.h` | Link click → navigation trigger |
| `FrameLoader` | `core/loader/frame_loader.h` | Frame-level navigation management |

---

## Source References

| File | Role |
|------|------|
| `third_party/blink/renderer/core/input/event_handler.cc` | Input routing, hit-testing |
| `third_party/blink/renderer/core/input/mouse_event_manager.cc` | Click synthesis |
| `third_party/blink/renderer/core/dom/events/event_dispatcher.cc` | Capture/bubble dispatch loop |
| `third_party/blink/renderer/core/dom/events/event_path.cc` | Event path construction |
| `third_party/blink/renderer/core/dom/node.cc` | DispatchEventInternal, DefaultEventHandler |
| `third_party/blink/renderer/core/html/html_anchor_element.cc` | Link click handling, HandleClick |
| `third_party/blink/renderer/core/loader/frame_loader.cc` | StartNavigation → BeginNavigation IPC |
