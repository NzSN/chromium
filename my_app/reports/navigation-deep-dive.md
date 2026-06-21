# How Navigation Works in Chromium

## Overview

Navigation is the process of loading a new document in a frame. It is **browser-controlled** — the renderer only requests navigations and renders committed results. The browser process makes all security and process model decisions.

```
Renderer                          Browser                         Network
(untrusted)                       (trusted)                       (sandboxed)
┌──────────┐                     ┌──────────────────┐            ┌─────────┐
│ Blink    │  BeginNavigation    │ NavigationRequest │  URL fetch │ Network │
│ FrameLoad├────────────────────►│ (state machine)   ├───────────►│ Service │
│ er       │       IPC           │                   │◄───────────┤         │
│          │◄────────────────────┤                   │  response  │         │
│          │  CommitNavigation   │                   │            │         │
│          │       IPC           │                   │            │         │
│          ├────────────────────►│                   │            │         │
│          │  DidCommitNavigation│                   │            │         │
└──────────┘       IPC           └──────────────────┘            └─────────┘
```

---

## Stage 1: Navigation Trigger

A navigation can be triggered from two places:

### Renderer-Initiated (most common)

User action or JavaScript in the web page:

```
User clicks <a href="https://example.com">
  │
  └─ Blink DOM event handling
       └─ HTMLAnchorElement::HandleClick()
            └─ FrameLoader::StartNavigation()
                 └─ LocalFrameClient::BeginNavigation()
                      └─ RenderFrameImpl::BeginNavigation()     [renderer/]
                           │
                           │ Mojo IPC: mojom::FrameHost::BeginNavigation(
                           │     CommonNavigationParams,
                           │     BeginNavigationParams)
                           │
                           └─────────────────────────────► Browser Process
```

Other renderer triggers:
- `window.location.href = "..."` (JS assignment)
- `window.location.replace("...")` (JS replace)
- `<form>` submission
- `<meta http-equiv="refresh">`
- `window.open("...")` (new window)

### Browser-Initiated

The browser itself starts the navigation (no renderer involvement initially):

```
User types URL in address bar
  │
  └─ NavigationControllerImpl::LoadURLWithParams()
       └─ NavigationControllerImpl::NavigateToPendingEntry()
            └─ Navigator::Navigate()
                 └─ NavigationRequest::BeginNavigation()
```

Other browser triggers:
- Clicking a bookmark
- Restoring a tab from session history
- `WebContents::OpenURL()` (embedder API)
- Back/Forward button
- Reload

---

## Stage 2: BeforeUnload

Before leaving the current page, the browser asks the renderer to run the `beforeunload` event handler (if one exists):

```
Browser                                    Renderer
  │                                          │
  ├─ "Run beforeunload handler please" ────► │
  │                                          ├─ Execute JS beforeunload handler
  │                                          │   └─ event.preventDefault() ?
  │  ◄──── "proceed=true/false" ─────────── ┤
  │                                          │
  ├─ If proceed=false:                       │
  │   └─ Navigation CANCELLED                │
  │                                          │
  ├─ If proceed=true:                        │
  │   └─ Continue to Stage 3                 │
```

The browser can also skip `beforeunload`:
- `ContentBrowserClient::ShouldSkipBeforeUnloadDialog()` returns true
- Navigation is same-document (hash change, pushState)
- Page has no `beforeunload` listener registered

---

## Stage 3: NavigationRequest State Machine

The browser creates a `NavigationRequest` — the central object that tracks the navigation from start to commit. It implements a state machine:

```
                    ┌──────────────┐
                    │  NOT_STARTED │
                    └──────┬───────┘
                           │
              ┌────────────▼─────────────┐
              │ WAITING_FOR_RENDERER_    │  (renderer-initiated only)
              │ RESPONSE                 │  wait for renderer's throttles
              └────────────┬─────────────┘
                           │
              ┌────────────▼─────────────┐
              │ WILL_START_NAVIGATION    │  navigation becomes visible
              │                          │  to observers
              └────────────┬─────────────┘
                           │
                           │ WebContentsObserver::DidStartNavigation()
                           │
              ┌────────────▼─────────────┐
              │ WILL_START_REQUEST       │  NavigationThrottles run
              │                          │  WillStartRequest()
              └────────────┬─────────────┘
                           │
                           │ [throttles: PROCEED / CANCEL / DEFER]
                           │
              ┌────────────▼─────────────┐
              │    Network Request        │  browser fetches URL via
              │    (via network service)  │  network service
              └────────────┬─────────────┘
                           │
                    ┌──────▼──────┐
                    │  Redirect?  │
                    └──┬──────┬───┘
                  yes  │      │ no
              ┌────────▼───┐  │
              │WILL_REDIRECT│  │   NavigationThrottles run
              │_REQUEST     │  │   WillRedirectRequest()
              └────────┬───┘  │
                       │      │   WebContentsObserver::DidRedirectNavigation()
                       └──────┤
                              │
              ┌───────────────▼──────────┐
              │ WILL_PROCESS_RESPONSE   │  response headers arrived
              │                          │  NavigationThrottles run
              │                          │  WillProcessResponse()
              └───────────┬──────────────┘
                          │
                          │ [throttles: PROCEED / CANCEL / BLOCK]
                          │
                          │ WebContentsObserver::ReadyToCommitNavigation()
                          │
              ┌───────────▼──────────────┐
              │    READY_TO_COMMIT       │  browser selected renderer
              │                          │  process, sending commit IPC
              └───────────┬──────────────┘
                          │
                          │ CommitNavigation IPC → renderer
                          │ Renderer renders, sends DidCommitNavigation back
                          │
              ┌───────────▼──────────────┐
              │      DID_COMMIT          │  navigation complete
              └──────────────────────────┘
                          │
                          │ WebContentsObserver::DidFinishNavigation()
```

### Error Path

```
              ┌───────────────────────────┐
              │    WILL_FAIL_REQUEST      │  network error, SSL error, etc.
              └───────────┬───────────────┘
                          │
              ┌───────────▼───────────────┐
              │    CANCELING              │  throttle blocked, or error
              └───────────┬───────────────┘
                          │
              ┌───────────▼───────────────┐
              │  DID_COMMIT_ERROR_PAGE    │  error page shown
              └───────────────────────────┘
```

---

## Stage 4: NavigationThrottles

At three key points, the browser runs **NavigationThrottles** — embedder-registered interceptors that can block, redirect, or defer the navigation.

### When Throttles Run

```
BeginNavigation
  │
  ├─► WillStartRequest()          — before network request
  │   └─ Check: should we even send this request?
  │      Example: Safe Browsing blocks known malware URLs
  │
  ├─► WillRedirectRequest()       — on each HTTP redirect (3xx)
  │   └─ Check: is the redirect target allowed?
  │      Example: enterprise policy blocks redirect to blocked site
  │   (can run multiple times for redirect chains)
  │
  ├─► WillProcessResponse()       — response headers arrived
  │   └─ Check: should we show this response?
  │      Example: check response against Safe Browsing database
  │
  └─► WillCommitWithoutUrlLoader() — for non-network navigations
      └─ same-document nav, about:blank, data: URLs
```

### Throttle Return Values

```cpp
enum ThrottleAction {
  PROCEED,                   // allow — continue to next throttle or next stage
  DEFER,                     // pause — call Resume() later to continue
  CANCEL,                    // cancel — abort navigation silently
  CANCEL_AND_IGNORE,         // cancel — abort and don't show error
  BLOCK_REQUEST,             // block — show net::ERR_BLOCKED_BY_CLIENT
  BLOCK_REQUEST_AND_COLLAPSE, // block — also collapse the frame (for iframes)
  BLOCK_RESPONSE,            // block — response specifically blocked
};
```

### Throttle Registration

```cpp
// ContentBrowserClient — embedder registers throttles:
std::vector<std::unique_ptr<NavigationThrottle>>
ContentBrowserClient::CreateThrottlesForNavigation(NavigationHandle* handle) {
  std::vector<std::unique_ptr<NavigationThrottle>> throttles;
  // [Chrome] adds:
  throttles.push_back(std::make_unique<SafeBrowsingThrottle>(handle));
  throttles.push_back(std::make_unique<PolicyBlacklistThrottle>(handle));
  throttles.push_back(std::make_unique<SSLErrorThrottle>(handle));
  // [my_app] adds nothing (uses defaults)
  return throttles;
}
```

### Throttle Execution Order

Throttles run in registration order. If ANY throttle returns `CANCEL` or `BLOCK`, the navigation stops immediately — later throttles don't run.

```
WillStartRequest():
  Throttle 1 (SafeBrowsing)    → PROCEED
  Throttle 2 (PolicyBlacklist) → PROCEED
  Throttle 3 (SSLError)        → PROCEED
  All passed → make network request

WillStartRequest() (blocked example):
  Throttle 1 (SafeBrowsing)    → BLOCK_REQUEST  ← malware detected!
  Throttle 2 (PolicyBlacklist) → never called
  Throttle 3 (SSLError)        → never called
  Navigation aborted, error page shown
```

---

## Stage 5: Network Request

After throttles approve `WillStartRequest()`, the browser fetches the URL:

```
Browser Process                              Network Service
  │                                          (utility process)
  ├─ Create URLLoader via URLLoaderFactory    │
  │                                           │
  ├─ For custom schemes (myapp://):           │
  │   ContentBrowserClient::                  │
  │     CreateNonNetworkNavigationURL         │
  │     LoaderFactory("myapp")                │
  │   └─ Returns AppURLLoaderFactory          │
  │      (serves from pak file)               │
  │                                           │
  ├─ For network URLs (http/https):           │
  │   └─ URLLoaderFactory::CreateLoader() ───►│
  │                                           ├─ DNS resolve
  │                                           ├─ TCP connect
  │                                           ├─ TLS handshake
  │                                           ├─ HTTP request
  │                                           ├─ CORS check
  │                                           ├─ CORB/ORB check
  │   ◄── Response headers ──────────────────┤
  │                                           │
  │   [Run WillRedirectRequest throttles      │
  │    if 3xx redirect]                       │
  │                                           │
  │   [Run WillProcessResponse throttles]     │
  │                                           │
  │   ◄── Response body (data pipe) ─────────┤
  │                                           │
```

### Special Response Handling

| Response | Action |
|----------|--------|
| 200 OK | Continue to commit |
| 3xx Redirect | Follow redirect, run `WillRedirectRequest` throttles |
| 204 No Content | No new document — cancel navigation |
| 205 Reset Content | No new document — cancel navigation |
| `Content-Disposition: attachment` | Trigger download, no navigation |
| Network error | Show error page (`net::ERR_*`) |
| SSL certificate error | Show interstitial or error page |

---

## Stage 6: Process Selection

Before committing, the browser decides **which renderer process** should handle the response. This is where Site Isolation happens:

```
Browser receives response for https://example.com
  │
  ├─ Determine SiteInfo for the URL
  │   └─ SiteInfo = {scheme=https, host=example.com}
  │
  ├─ ContentBrowserClient::DoesSiteRequireDedicatedProcess()
  │   └─ [Chrome desktop] yes — strict site isolation
  │   └─ [Android] depends — only for sensitive sites
  │
  ├─ Find or create SiteInstance for this SiteInfo
  │   ├─ Same site as current page? → reuse current process
  │   └─ Different site? → need a new process
  │
  ├─ ContentBrowserClient::ShouldTryToUseExistingProcessHost()
  │   └─ Can we reuse any existing process?
  │
  ├─ ContentBrowserClient::ShouldLockProcessToSite()
  │   └─ Lock the renderer to example.com only?
  │
  ├─ ContentBrowserClient::IsSuitableHost()
  │   └─ Is this specific process suitable?
  │
  ├─ Select process:
  │   ├─ Reuse current (same-site navigation)
  │   ├─ Reuse existing (process sharing allowed)
  │   ├─ Use spare process (pre-spawned)
  │   └─ Spawn new process (cross-site navigation)
  │
  └─ Create speculative RenderFrameHost if cross-process
```

### Cross-Process Navigation

When navigating from site A to site B (different processes):

```
Before navigation:
  FrameTreeNode
    └─ current RFH (process A, locked to site-a.com)

During navigation:
  FrameTreeNode
    ├─ current RFH (process A) ← still showing old page
    └─ speculative RFH (process B) ← waiting for commit

After commit:
  FrameTreeNode
    └─ current RFH (process B, locked to site-b.com) ← new page
       (old RFH in process A destroyed or moved to BFCache)
```

---

## Stage 7: Commit

The browser tells the renderer to load the document:

```
Browser Process                              Renderer Process
  │                                            │
  ├─ Prepare CommitNavigationParams:           │
  │   ├─ URL, origin, referrer                 │
  │   ├─ Response head (headers, MIME type)     │
  │   ├─ Response body (Mojo data pipe)        │
  │   ├─ URL loader client endpoints           │
  │   ├─ Subresource loader factories          │
  │   ├─ Service worker info                   │
  │   ├─ Document token                        │
  │   ├─ Content security policy               │
  │   └─ Navigation timing data                │
  │                                            │
  ├─ NavigationClient::CommitNavigation() ────►│
  │       (Mojo IPC)                           │
  │                                            ├─ Create new Document
  │                                            ├─ Set document URL, origin
  │                                            ├─ Begin parsing response body
  │                                            ├─ ──────────────────────────
  │                                            │  THIS IS THE COMMIT POINT
  │                                            │  Security state changes here
  │                                            ├─ ──────────────────────────
  │                                            │
  │  ◄──── DidCommitNavigation callback ───────┤
  │        (DidCommitProvisionalLoadParams)     │
  │        └─ committed URL, origin,           │
  │           transition type, page state,     │
  │           HTTP status code, timing         │
  │                                            │
  ├─ Update browser state:                     │
  │   ├─ Security state (origin, HTTPS)        │
  │   ├─ Session history (NavigationEntry)     │
  │   ├─ Process lock (if needed)              │
  │   └─ Fire observer notifications           │
  │                                            │
  │   WebContentsObserver::                    │
  │     DidFinishNavigation(handle)            │
  │                                            │
  └─ Navigation complete                       ├─ Continue loading:
                                               │   Parse HTML → build DOM
                                               │   Fetch subresources
                                               │   Execute JavaScript
                                               │   DOMContentLoaded
                                               │   load event
                                               └─ Page fully loaded
```

### The Commit Point

The commit is the **critical security transition**:

| Before Commit | After Commit |
|---------------|-------------|
| Old document visible | New document visible |
| Old origin in address bar | New origin in address bar |
| Old security state | New security state |
| Old session history entry | New session history entry created |
| Old process lock | New process lock (may change) |

The commit point is **atomic** — there is no in-between state where the user sees the old URL but the new document, or vice versa.

---

## Stage 8: Loading (Post-Commit)

After commit, the renderer loads the page. Errors at this stage show **partial content** (not error pages):

```
Renderer (after commit)              Browser (receives notifications)
  │                                    │
  ├─ Parse HTML                        │
  │   └─ Build DOM tree                │
  │                                    │
  ├─ Discover subresources             │
  │   ├─ <link rel="stylesheet">       │
  │   ├─ <script src="...">            │
  │   ├─ <img src="...">               │
  │   └─ Fetch via URLLoaderFactory    │
  │       (given during commit)        │
  │                                    │
  ├─ DOMContentLoaded event ──────────►├─ WebContentsObserver::DOMContentLoaded()
  │                                    │
  ├─ All subresources loaded           │
  ├─ load event ──────────────────────►├─ WebContentsObserver::DidFinishLoad()
  │                                    │
  ├─ All frames done ────────────────► ├─ WebContentsObserver::DidStopLoading()
  │                                    │
  └─ Page interactive                  └─ Loading spinner stops
```

### Post-Commit Errors

If a subresource fails to load after commit, the browser does NOT show an error page — the page is already committed. Instead:
- Broken images show placeholder
- Failed scripts don't execute
- Failed CSS means unstyled content
- `WebContentsObserver::DidFailLoad()` fires for the failed frame

---

## Observer Notifications Timeline

```
Time ────────────────────────────────────────────────────────────────────►

  DidStart     DidRedirect   ReadyTo      DidFinish      DidStop
  Navigation   Navigation    Commit       Navigation     Loading
     │            │          Navigation       │             │
     ▼            ▼              ▼            ▼             ▼
     ┊            ┊              ┊            ┊             ┊
     ┊ WillStart  ┊ WillRedirect ┊ WillProcess┊             ┊
     ┊ Request    ┊ Request      ┊ Response   ┊             ┊
     ┊ (throttle) ┊ (throttle)   ┊ (throttle) ┊             ┊
     ┊            ┊              ┊            ┊             ┊
─────┼────────────┼──────────────┼────────────┼─────────────┼──────
     │            │              │            │             │
     │  network   │   redirect   │   commit   │  subresource│
     │  request   │              │   IPC      │  loading    │
     │            │              │            │             │
     ├────────────┼──── NAVIGATION PHASE ─────┤             │
                                              ├─ LOADING ──┤
                                              │  PHASE      │
                                              │             │
                                              │ DOMContent  │
                                              │ Loaded      │
                                              │     │       │
                                              │ DidFinish   │
                                              │ Load        │
```

---

## Same-Document Navigations

Not all navigations create a new document. **Same-document navigations** skip the network entirely:

| Type | Example | New Document? |
|------|---------|---------------|
| Hash change | `#section2` | No |
| `history.pushState()` | JS history API | No |
| `history.replaceState()` | JS history API | No |

```
Same-document navigation:
  Renderer handles entirely (no network, no process selection)
    └─ Updates URL in address bar via IPC
    └─ Creates session history entry
    └─ Fires popstate event
    └─ WebContentsObserver::DidFinishNavigation(handle)
       └─ handle->IsSameDocument() == true
```

---

## NavigationHandle — The Public API

`NavigationHandle` is the public interface observers and throttles use to query navigation state:

```cpp
class NavigationHandle {
  // Identity
  const GURL& GetURL();                    // current URL (after redirects)
  const GURL& GetPreviousPrimaryMainFrameURL();
  bool IsInMainFrame();
  bool IsInPrimaryMainFrame();
  bool IsSameDocument();
  bool IsRendererInitiated();

  // State
  bool HasCommitted();
  bool IsErrorPage();
  bool IsDownload();
  net::Error GetNetError();
  int GetResponseHeaders();               // HTTP response headers
  const net::SSLInfo& GetSSLInfo();        // certificate info

  // Frame info
  RenderFrameHost* GetRenderFrameHost();   // the committed RFH (after commit)
  FrameTreeNodeId GetFrameTreeNodeId();
  SiteInstance* GetSiteInstance();

  // Timing
  const NavigationHandleTiming& GetNavigationHandleTiming();
};
```

---

## Key Mojo Interfaces

### Browser → Renderer

```mojom
// content/common/navigation_client.mojom

interface NavigationClient {
  // Browser tells renderer to commit the navigation
  CommitNavigation(
      CommonNavigationParams common_params,
      CommitNavigationParams commit_params,
      network.mojom.URLResponseHead response_head,
      mojo.ScopedDataPipeConsumerHandle response_body,
      SubresourceLoaderParams subresource_loader_params,
      ...)
    => (DidCommitProvisionalLoadParams params, ...);

  // Browser tells renderer to commit a failed navigation (error page)
  CommitFailedNavigation(
      CommonNavigationParams common_params,
      CommitNavigationParams commit_params,
      ...)
    => (DidCommitProvisionalLoadParams params, ...);
};
```

### Renderer → Browser

```mojom
// content/common/frame.mojom

interface FrameHost {
  // Renderer tells browser: "user clicked link, please navigate"
  BeginNavigation(
      CommonNavigationParams common_params,
      BeginNavigationParams begin_params,
      pending_receiver<BlobURLToken>? blob_url_token,
      pending_associated_remote<NavigationClient> navigation_client,
      ...);

  // Renderer tells browser: "I committed the navigation"
  // (via CommitNavigation callback, not a separate method)
};
```

---

## Complete Flow Diagram

```
USER CLICKS LINK
       │
       ▼
 ┌─ RENDERER ──────────────────────────┐
 │  Blink: FrameLoader::StartNavigation│
 │    └─ RenderFrameImpl::             │
 │       BeginNavigation()             │
 └──────────┬──────────────────────────┘
            │ Mojo IPC: BeginNavigation
            ▼
 ┌─ BROWSER ───────────────────────────────────────────────────┐
 │                                                              │
 │  NavigationRequest created (state: NOT_STARTED)             │
 │    │                                                         │
 │    ├─ Run beforeunload on old document                      │
 │    │                                                         │
 │    ├─ State: WILL_START_NAVIGATION                          │
 │    │   └─ Observer: DidStartNavigation()                    │
 │    │                                                         │
 │    ├─ State: WILL_START_REQUEST                             │
 │    │   └─ Throttles: WillStartRequest()                     │
 │    │       ├─ PROCEED → continue                            │
 │    │       └─ BLOCK → abort, show error                     │
 │    │                                                         │
 │    ├─ Fetch URL via network service ─────────────────────┐  │
 │    │                                                      │  │
 │    │  ┌─ NETWORK SERVICE ─────────────────────────────┐  │  │
 │    │  │  DNS → TCP → TLS → HTTP GET                   │  │  │
 │    │  │  CORS check, CORB/ORB check                   │  │  │
 │    │  │  Response headers → response body              │  │  │
 │    │  └────────────────────────────────────────────────┘  │  │
 │    │                                                      │  │
 │    │  ◄── response ─────────────────────────────────────┘  │
 │    │                                                         │
 │    ├─ [if redirect] State: WILL_REDIRECT_REQUEST            │
 │    │   ├─ Throttles: WillRedirectRequest()                  │
 │    │   ├─ Observer: DidRedirectNavigation()                 │
 │    │   └─ loop back to fetch redirect URL                   │
 │    │                                                         │
 │    ├─ State: WILL_PROCESS_RESPONSE                          │
 │    │   └─ Throttles: WillProcessResponse()                  │
 │    │                                                         │
 │    ├─ SELECT PROCESS                                        │
 │    │   ├─ DoesSiteRequireDedicatedProcess()                 │
 │    │   ├─ ShouldLockProcessToSite()                         │
 │    │   ├─ ShouldTryToUseExistingProcessHost()               │
 │    │   └─ Create speculative RFH if cross-process           │
 │    │                                                         │
 │    ├─ Observer: ReadyToCommitNavigation()                    │
 │    │                                                         │
 │    ├─ State: READY_TO_COMMIT                                │
 │    │                                                         │
 └────┼────────────────────────────────────────────────────────┘
      │ Mojo IPC: CommitNavigation(response_head, body, ...)
      ▼
 ┌─ RENDERER (target process) ─────────────────────────────────┐
 │                                                              │
 │  Create Document, set URL/origin                            │
 │  Parse response body                                        │
 │  ═══ COMMIT POINT (security state changes) ═══             │
 │                                                              │
 │  Return DidCommitProvisionalLoadParams                      │
 │                                                              │
 └──────────┬──────────────────────────────────────────────────┘
            │ Mojo callback: DidCommitNavigation
            ▼
 ┌─ BROWSER ───────────────────────────────────────────────────┐
 │                                                              │
 │  State: DID_COMMIT                                          │
 │  Update session history (NavigationEntry)                   │
 │  Update security state                                      │
 │  Observer: DidFinishNavigation()                            │
 │                                                              │
 └─────────────────────────────────────────────────────────────┘
            │
            │ (renderer continues loading subresources)
            ▼
 ┌─ RENDERER ──────────────────────────────────────────────────┐
 │  Fetch CSS, JS, images via SubresourceLoaderFactories       │
 │  Execute JavaScript                                         │
 │  DOMContentLoaded → DidFinishLoad → DidStopLoading         │
 └─────────────────────────────────────────────────────────────┘
```

---

## Source References

| File | Role |
|------|------|
| `docs/navigation.md` | Official navigation documentation |
| `content/browser/renderer_host/navigation_request.h` | NavigationRequest state machine (primary impl) |
| `content/public/browser/navigation_handle.h` | Public API for observers/throttles |
| `content/public/browser/navigation_throttle.h` | Throttle interface |
| `content/browser/renderer_host/navigator.h` | Performs navigations in frame tree nodes |
| `content/browser/renderer_host/render_frame_host_impl.cc` | CommitNavigation/DidCommitNavigation |
| `content/common/navigation_client.mojom` | Mojo interface for commit IPC |
| `third_party/blink/renderer/core/loader/frame_loader.cc` | Blink-side navigation trigger |
