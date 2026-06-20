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
  ║                     INITIALIZATION                           ║
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
