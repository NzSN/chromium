# Browser Process Lifecycle

## Overview

The browser process is the **privileged, unsandboxed** process that manages everything: windows, navigation, child processes, storage, networking. Its lifecycle is a carefully ordered sequence of initialization phases, a main message loop, and a shutdown sequence.

The embedder participates via two interfaces:
- **`ContentBrowserClient`** — creates `BrowserMainParts` and provides lifecycle queries
- **`BrowserMainParts`** — the primary lifecycle hook interface (12 phases)

---

## Complete Lifecycle Timeline

```
main()
  │
  ╔═══════════════════════════════════════════════════════════════╗
  ║                     INITIALIZATION                            ║
  ╚═══════════════════════════════════════════════════════════════╝
  │
  ├─ ContentMain() → Initialize() → RunBrowser()
  │   ├─ ContentBrowserClient::CreateBrowserMainParts()
  │   │   └─ [my_app] returns AppBrowserMainParts
  │   │
  │   └─ BrowserMainRunnerImpl::Initialize()
  │       │
  │       ├─── Phase 1: PreEarlyInitialization ─────────────────
  │       │    "As soon as possible on program start"
  │       │    • base::FeatureList is functional
  │       │    • base::ThreadPool exists but won't run tasks yet
  │       │    • [Chrome] signal handlers, crash setup
  │       │    • [my_app] returns RESULT_CODE_NORMAL_EXIT
  │       │
  │       ├─── (BrowserMainLoop::EarlyInitialization) ──────────
  │       │    • Content's own early init (internal)
  │       │
  │       ├─── Phase 2: PostEarlyInitialization ────────────────
  │       │    "After content's early init"
  │       │    • [Chrome] field trials, metrics
  │       │    • [my_app] (not overridden)
  │       │
  │       ├─── Phase 3: ToolkitInitialized ─────────────────────
  │       │    "UI toolkit is ready"
  │       │    • GTK/Aura/Cocoa initialized
  │       │    • [Chrome] extra toolkit setup
  │       │    • [my_app] (not overridden)
  │       │
  │       ├─── Phase 4: PreCreateMainMessageLoop ───────────────
  │       │    "Before the message loop exists"
  │       │    • [Chrome] wire up platform-specific systems
  │       │    • [my_app] (not overridden)
  │       │
  │       ├─── (BrowserMainLoop::CreateMainMessageLoop) ────────
  │       │    • Creates the UI thread message loop
  │       │    • BrowserThread::UI is now alive
  │       │
  │       ├─── Phase 5: PostCreateMainMessageLoop ──────────────
  │       │    "Message loop exists, BrowserThread::UI is up"
  │       │    • Can now PostTask to UI thread
  │       │    • [Chrome] start system monitor, power monitor
  │       │    • [my_app] (not overridden)
  │       │
  │       └─── BrowserMainLoop::CreateStartupTasks() ───────────
  │            │
  │            ├── Phase 6: PreCreateThreads ═══════════════════
  │            │    "Single-threaded init — no IO thread yet"
  │            │    • Initialize singletons, thread-compatible objects
  │            │    • ThreadPool exists but tasks don't run yet
  │            │    • [Chrome] create BrowserContext/Profile, prefs
  │            │    • [my_app] create AppBrowserContext,
  │            │              NotificationManager::Initialize()
  │            │
  │            ├── (CreateThreads — internal) ──────────────────
  │            │    • Release ThreadPool execution fence
  │            │      (posted tasks start running)
  │            │    • Create and register IO thread
  │            │    • BrowserThread::IO is now alive
  │            │
  │            ├── Phase 7: PostCreateThreads ══════════════════
  │            │    "IO thread up, ThreadPool running"
  │            │    • Can PostTask to IO thread
  │            │    • Can run async work on ThreadPool
  │            │    • [Chrome] start services that need IO thread
  │            │    • [my_app] (not overridden)
  │            │
  │            └── Phase 8: PreMainMessageLoopRun ══════════════
  │                 "IN DOUBT, PUT THINGS HERE"
  │                 • All core APIs initialized
  │                 • All threads running
  │                 • Best place for embedder initialization
  │                 • [Chrome] create browser window, toolbar,
  │                           restore tabs, start extensions
  │                 • [my_app] create WMState, Screen,
  │                           ViewsDelegate, AppWindow,
  │                           load initial URL
  │
  ╔═══════════════════════════════════════════════════════════════╗
  ║                    RUNNING (message loop)                     ║
  ╚═══════════════════════════════════════════════════════════════╝
  │
  ├─── Phase 9: WillRunMainMessageLoop ─────────────────────────
  │    "RunLoop::Run() is about to be called"
  │    • Last chance to modify or replace the RunLoop
  │    • NOT called in browser tests (test body runs instead)
  │    • [my_app] captures quit_closure from RunLoop
  │
  ├─── RunLoop::Run() ◄══════ BLOCKS HERE ══════════════════════
  │    • App is alive, processing events
  │    • UI events dispatched (mouse, keyboard, paint)
  │    • Mojo messages dispatched (IPC from child processes)
  │    • Timers, posted tasks executed
  │    │
  │    ├── Phase 10: OnFirstIdle ───────────────────────────────
  │    │    "First time main thread has nothing to do"
  │    │    • All startup tasks completed
  │    │    • [Chrome] start responsiveness watcher,
  │    │              deferred background work
  │    │    • [my_app] (not overridden)
  │    │
  │    └── [quit_closure called — last window closed]
  │         └─ RunLoop::Run() returns
  │
  ╔═══════════════════════════════════════════════════════════════╗
  ║                       SHUTDOWN                                ║
  ╚═══════════════════════════════════════════════════════════════╝
  │
  ├─── Phase 11: PostMainMessageLoopRun ════════════════════════
  │    "Cleanup while threads are still alive"
  │    • ThreadPool and IO thread still running
  │    • Can still PostTask, run async cleanup
  │    • [Chrome] close windows, stop services, flush storage
  │    • [my_app] CloseAllWindows(),
  │              NotificationManager::Shutdown(),
  │              browser_context_.reset(),
  │              views_delegate_.reset(),
  │              screen_.reset(), wm_state_.reset()
  │
  ├─── (BrowserMainLoop::ShutdownThreadsAndCleanUp) ────────────
  │    • Shutdown IO thread
  │    • Shutdown ThreadPool
  │    • No more async work possible after this
  │
  ├─── Phase 12: PostDestroyThreads ════════════════════════════
  │    "Single-threaded teardown"
  │    • Matching pair of PreCreateThreads
  │    • Destroy things created in PreCreateThreads
  │    • [Chrome] final cleanup
  │    • [my_app] (not overridden)
  │
  └─── ContentMainRunnerImpl::Shutdown()
       • Tear down MojoIpcSupport
       • BrowserTaskExecutor shutdown
       • AtExitManager runs exit callbacks
       • Process exits
```

---

## Runtime: ContentBrowserClient During the Message Loop

`BrowserMainParts` handles **startup and shutdown** (the 12 phases above). But the browser's life is mostly spent in `RunLoop::Run()` — processing events, navigations, and IPC. During this phase, `//content` calls `ContentBrowserClient`'s **350+ other methods** continuously for runtime decisions.

### Two Complementary Roles

```
Browser Lifetime
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

  STARTUP                    RUNNING                     SHUTDOWN
  (BrowserMainParts)         (ContentBrowserClient)      (BrowserMainParts)
  ┌───────────────┐          ┌───────────────────┐       ┌───────────────┐
  │PreCreateThreads│         │ Called on every:   │       │PostMainMsg    │
  │PreMainMsgLoop  │         │  • navigation      │       │LoopRun        │
  │WillRunMainMsg  │         │  • process launch  │       │PostDestroy    │
  │                │         │  • Mojo binding    │       │Threads        │
  └──────┬─────────┘         │  • permission ask  │       └───────────────┘
         │                   │  • SW registration │              ▲
         ▼                   │  • URL load        │              │
    RunLoop::Run() ────────► │  • cookie access   │ ──────► quit_closure
                             │  • ...350+ hooks   │
                             └───────────────────┘
```

**BrowserMainParts** = "what happens at startup and shutdown" (lifecycle phases)
**ContentBrowserClient** = "what to decide at runtime" (policy decisions, called continuously)

### Runtime Decision Categories

During `RunLoop::Run()`, every significant event in the browser process triggers a callback to `ContentBrowserClient`. These are grouped by when they fire:

#### On Every Navigation

```cpp
// content calls these for EACH navigation:
CreateThrottlesForNavigation(handle)              // Add throttles to block/redirect/defer
OverrideNavigationParams(context, url, ...)       // Modify transition type, referrer
ShouldOverrideUrlLoading(params)                  // Override loading (e.g., Android intents)
WillComputeSiteForNavigation(context, url, ...)   // Pre-compute site for process selection
GetEffectiveURL(context, url)                     // Map URL for process model decisions
```

**Example:** Chrome adds Safe Browsing + enterprise policy throttles via `CreateThrottlesForNavigation()`. my_app uses defaults (no throttles).

#### On Every Process Launch

```cpp
// content calls these when spawning a child process:
RenderProcessWillLaunch(host)                     // Configure the new renderer
BrowserChildProcessHostCreated(host)              // Any child process (GPU, utility, etc.)
AppendExtraCommandLineSwitches(cmd, child_id)      // Add flags to child process command line
ShouldUseSpareRenderProcessHost(context, url)      // Use pre-spawned spare process?
ShouldTryToUseExistingProcessHost(context, url)    // Reuse existing renderer?
IsSuitableHost(process_host, instance)            // Is this process suitable for this site?
DoesSiteRequireDedicatedProcess(context, site)     // Must this site be isolated?
ShouldLockProcessToSite(context, site)             // Lock renderer to one site?
```

**Example:** Chrome gives extensions their own process, locks sites for site isolation. my_app uses defaults.

#### On Every Frame Creation / Mojo Binding

```cpp
// content calls these when a new frame appears or needs interfaces:
RegisterBrowserInterfaceBindersForFrame(rfh, map)  // Bind Mojo interfaces for this frame
RegisterBrowserInterfaceBindersForServiceWorker(ctx, map)  // Bind for service workers
ExposeInterfacesToRenderer(registry, rfh)          // Expose process-scoped interfaces
OverrideURLLoaderFactoryParams(process, origin, p) // Modify URL loader factory params
```

**Example:** my_app binds `mojom::NativeApi` here — this is how JS calls native clipboard, file dialogs, etc.

#### On Every URL Load

```cpp
// content calls these for resource loading:
CreateNonNetworkNavigationURLLoaderFactory(scheme, id)       // Factory for custom scheme nav
RegisterNonNetworkSubresourceURLLoaderFactories(pid, fid, ..) // Factory for custom subresources
ShouldAllowNoLongerUsedProcessToExit()            // Kill idle renderers?
ConfigureNetworkContextParams(context, in_mem, ...) // Network config (proxy, SSL, cache)
```

**Example:** my_app returns `AppURLLoaderFactory` for `myapp://` URLs — serves bundled HTML/CSS/JS from pak files.

#### On Permission / Policy Checks

```cpp
// content calls these when web content requests capabilities:
AllowServiceWorker(scope, site, context, rfh)      // Allow Service Worker registration?
AllowSharedWorker(url, site, name, origin, ...)     // Allow Shared Worker?
IsClipboardPasteAllowed(rfh, source, type)         // Allow paste operation?
IsClipboardPasteAllowedByPolicy(src, dst, data, cb) // Enterprise clipboard policy
IsDragAllowedByPolicy(source, data)                // Enterprise drag policy
IsFileAccessAllowed(path, absolute, profile)       // File access permitted?
IsJitDisabledForSite(context, url)                 // Disable V8 JIT?
```

**Example:** Chrome checks enterprise DLP policies for clipboard, blocks JIT on sensitive origins. my_app uses defaults (allow all).

#### On Security Decisions

```cpp
// content calls these for security enforcement:
CanCommitURL(process_host, url)                    // Can this process commit this URL?
ShouldEnableStrictSiteIsolation()                  // Isolate every site?
GetOriginsRequiringDedicatedProcess()              // Origins needing own process
ShouldIsolateErrorPage(in_main_frame)              // Isolate error pages?
ShouldBlockRendererDebugURL(url, context)           // Block chrome://crash etc.?
```

#### On UI Events

```cpp
// content calls these for UI-related decisions:
GetUserAgent()                                     // Every HTTP request
GetAcceptLangs(context)                            // Every HTTP request
GetWebContentsViewDelegate(web_contents)           // Creating a view for WebContents
IsFullscreenAllowedForUnfocusedWebContents(wc)     // Fullscreen when unfocused?
GetDefaultFavicon()                                // Default page icon
```

### Runtime Flow Example: User Clicks a Link

```
User clicks <a href="https://example.com">
  │
  ├─ Renderer sends navigation request to browser
  │
  ├─ ContentBrowserClient::CreateThrottlesForNavigation()
  │   └─ [Chrome] adds SafeBrowsingThrottle, PolicyThrottle
  │   └─ [my_app] returns empty (no throttles)
  │
  ├─ ContentBrowserClient::GetEffectiveURL()
  │   └─ Maps URL for process model decisions
  │
  ├─ ContentBrowserClient::DoesSiteRequireDedicatedProcess()
  │   └─ [Chrome] yes for most sites (site isolation)
  │   └─ [my_app] uses default (content decides)
  │
  ├─ ContentBrowserClient::ShouldTryToUseExistingProcessHost()
  │   └─ Should we reuse an existing renderer?
  │
  ├─ ContentBrowserClient::ShouldLockProcessToSite()
  │   └─ Lock this renderer to example.com?
  │
  ├─ ContentBrowserClient::RenderProcessWillLaunch()
  │   └─ [Chrome] configure process, add extensions
  │   └─ [my_app] (not overridden)
  │
  ├─ ContentBrowserClient::AppendExtraCommandLineSwitches()
  │   └─ Add flags to renderer command line
  │
  ├─ [Navigation commits — renderer loads page]
  │
  ├─ ContentBrowserClient::RegisterBrowserInterfaceBindersForFrame()
  │   └─ [my_app] binds mojom::NativeApi
  │   └─ [Chrome] binds extensions, autofill, translate, etc.
  │
  └─ Page is loaded and interactive
```

### my_app's Runtime Methods (6 of 350+)

| Method | When Called | What my_app Does |
|--------|------------|------------------|
| `GetUserAgent()` | Every HTTP request | Returns `"MyApp/1.0"` |
| `GetAcceptLangs()` | Every HTTP request | Returns `"en-US,en"` |
| `RegisterBrowserInterfaceBindersForFrame()` | Every new frame | Binds `mojom::NativeApi` |
| `CreateNonNetworkNavigationURLLoaderFactory()` | Every `myapp://` navigation | Returns `AppURLLoaderFactory` |
| `RegisterNonNetworkSubresourceURLLoaderFactories()` | Every `myapp://` subresource | Returns `AppURLLoaderFactory` |
| `CreateBrowserMainParts()` | Once at startup | Returns `AppBrowserMainParts` |

The other ~344 methods use content's defaults — designed to be safe and functional without embedder customization.

---

## Phase Reference

### What's Available at Each Phase

| Phase | FeatureList | UI Thread | IO Thread | ThreadPool | Message Loop |
|-------|------------|-----------|-----------|------------|-------------|
| PreEarlyInitialization | ✅ | ❌ | ❌ | exists (paused) | ❌ |
| PostEarlyInitialization | ✅ | ❌ | ❌ | exists (paused) | ❌ |
| ToolkitInitialized | ✅ | ❌ | ❌ | exists (paused) | ❌ |
| PreCreateMainMessageLoop | ✅ | ❌ | ❌ | exists (paused) | ❌ |
| PostCreateMainMessageLoop | ✅ | ✅ | ❌ | exists (paused) | ✅ |
| **PreCreateThreads** | ✅ | ✅ | ❌ | exists (paused) | ✅ |
| **PostCreateThreads** | ✅ | ✅ | ✅ | ✅ running | ✅ |
| **PreMainMessageLoopRun** | ✅ | ✅ | ✅ | ✅ running | ✅ |
| WillRunMainMessageLoop | ✅ | ✅ | ✅ | ✅ running | ✅ |
| OnFirstIdle | ✅ | ✅ | ✅ | ✅ running | ✅ |
| **PostMainMessageLoopRun** | ✅ | ✅ | ✅ | ✅ running | ✅ (draining) |
| PostDestroyThreads | ✅ | ✅ | ❌ | ❌ | ✅ (draining) |

Bold = phases most embedders use.

### What to Do Where

| Phase | Purpose | Rule of Thumb |
|-------|---------|---------------|
| `PreEarlyInitialization` | Signal handlers, crash setup | Platform-specific early setup only |
| `PostEarlyInitialization` | After content's early init | Rarely needed |
| `ToolkitInitialized` | Extra UI toolkit setup | Rarely needed |
| `PreCreateMainMessageLoop` | Before message loop | Rarely needed |
| `PostCreateMainMessageLoop` | Message loop exists | System monitors, early services |
| **`PreCreateThreads`** | Single-threaded init | **BrowserContext, singletons, prefs** |
| `PostCreateThreads` | IO thread alive | Services needing IO thread |
| **`PreMainMessageLoopRun`** | **"IN DOUBT, PUT THINGS HERE"** | **Window creation, UI, load URL** |
| `WillRunMainMessageLoop` | Tweak RunLoop | Capture quit closure |
| `OnFirstIdle` | Startup complete | Deferred low-priority work |
| **`PostMainMessageLoopRun`** | Cleanup | **Close windows, stop services** |
| `PostDestroyThreads` | Final teardown | Destroy PreCreateThreads objects |

---

## ContentBrowserClient Lifecycle Methods

In addition to `BrowserMainParts`, `ContentBrowserClient` itself has lifecycle-related methods:

| Method | When | Purpose |
|--------|------|---------|
| `CreateBrowserMainParts(is_integration_test)` | Before Phase 1 | Return the embedder's BrowserMainParts |
| `PostAfterStartupTask(location, runner, task)` | After startup | Schedule task after browser is ready |
| `IsBrowserStartupComplete()` | Anytime | Query if startup phases are done |
| `OnUiTaskRunnerReady(runner)` | After Phase 5 | UI task runner available |
| `IsShuttingDown()` | Anytime | Query if shutdown has started |
| `ThreadPoolWillTerminate()` | Before pool dies | Last chance for pool-dependent work |
| `OnWebContentsCreated(web_contents)` | Anytime after Phase 8 | Any WebContents was created |
| `RenderProcessWillLaunch(host)` | Anytime after Phase 8 | Renderer process about to start |
| `BrowserChildProcessHostCreated(host)` | Anytime after Phase 8 | Any child process created |

---

## my_app's Lifecycle Implementation

```cpp
// browser/app_browser_main_parts.cc

int AppBrowserMainParts::PreCreateThreads() {         // Phase 6
  browser_context_ = std::make_unique<AppBrowserContext>();
  NotificationManager::Initialize();
  return 0;
}

int AppBrowserMainParts::PreMainMessageLoopRun() {    // Phase 8
  wm_state_ = std::make_unique<wm::WMState>();
  if (!display::Screen::HasScreen()) {
    screen_ = views::CreateDesktopScreen();
  }
  views_delegate_ = std::make_unique<AppViewsDelegate>();

  GURL initial_url("myapp://app/index.html");
  if (command_line->HasSwitch(switches::kDevServer)) {
    initial_url = GURL(command_line->GetSwitchValueASCII(switches::kDevServer));
  }

  AppWindow::Create(browser_context_.get(), initial_url);
  return 0;
}

void AppBrowserMainParts::WillRunMainMessageLoop(     // Phase 9
    std::unique_ptr<base::RunLoop>& run_loop) {
  quit_closure_ = run_loop->QuitClosure();
  AppWindow::SetQuitClosure(quit_closure_);
}

void AppBrowserMainParts::PostMainMessageLoopRun() {  // Phase 11
  AppWindow::CloseAllWindows();
  NotificationManager::Shutdown();
  browser_context_.reset();
  views_delegate_.reset();
  screen_.reset();
  wm_state_.reset();
}
```

### What Happens at Each Phase

| Phase | my_app Action | Why Here |
|-------|---------------|----------|
| PreCreateThreads | Create `AppBrowserContext` | BrowserContext is a singleton-like object — must init before threads |
| PreCreateThreads | `NotificationManager::Initialize()` | MessageCenter needs early init |
| PreMainMessageLoopRun | Create `WMState`, `Screen`, `ViewsDelegate` | UI toolkit must be ready before creating windows |
| PreMainMessageLoopRun | Create `AppWindow` + load URL | Window creation needs all systems ready |
| WillRunMainMessageLoop | Capture quit closure | Need the RunLoop reference to quit later |
| PostMainMessageLoopRun | Close windows, reset context | Cleanup while threads still alive |

---

## The Quit Flow

```
User closes window (clicks X)
  │
  ├─ Native window close event
  ├─ Widget::Close() → async close
  ├─ WidgetIsZombie() → delete AppWindow
  ├─ ~AppWindow() removes from window list
  ├─ Last window? → quit_closure_.Run()
  │
  └─ RunLoop::Run() returns
       │
       ├─ PostMainMessageLoopRun()  ← cleanup
       ├─ ShutdownThreadsAndCleanUp()
       ├─ PostDestroyThreads()
       └─ process exits

Web page calls window.close()
  │
  ├─ Renderer sends RequestClose via Mojo
  ├─ RenderFrameHostImpl::RequestClose()
  ├─ WebContentsImpl::Close()
  ├─ AppWindow::CloseContents()
  ├─ widget_->Close()
  └─ (same flow as above)
```

---

## Chrome's Lifecycle (for comparison)

Chrome's `ChromeBrowserMainParts` is far more complex:

| Phase | Chrome Action |
|-------|---------------|
| PreEarlyInitialization | Crash reporting, process singleton lock, OS compatibility checks |
| PostEarlyInitialization | Field trials, metrics initialization |
| ToolkitInitialized | Platform-specific UI customization |
| PreCreateMainMessageLoop | Wire system monitor, power monitor |
| PostCreateMainMessageLoop | DBus setup (Linux), Bluetooth init |
| **PreCreateThreads** | Create Profile, prefs, local state, policy providers, extensions system |
| PostCreateThreads | Start network service, safe browsing, component updater |
| **PreMainMessageLoopRun** | Create browser window, restore session, start extensions, start sync, NTP |
| WillRunMainMessageLoop | (minimal) |
| OnFirstIdle | Deferred: update check, cleanup, diagnostics |
| **PostMainMessageLoopRun** | Close all browsers, stop services, flush profiles, stop sync |
| PostDestroyThreads | Final profile cleanup, metrics flush |

---

## Matching Pairs

Some phases are designed as **matching pairs** — what you create in one, you destroy in the other:

| Create Phase | Destroy Phase | Example |
|-------------|---------------|---------|
| PreCreateThreads | PostDestroyThreads | Singletons, thread-compatible objects |
| PreMainMessageLoopRun | PostMainMessageLoopRun | Windows, services, contexts |
| PostCreateMainMessageLoop | (before ShutdownThreadsAndCleanUp) | System monitors |

The general rule: **destroy in reverse order of creation**, and destroy in the phase where the required infrastructure still exists (e.g., don't destroy something needing IO thread in PostDestroyThreads).
