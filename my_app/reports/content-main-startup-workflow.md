# `content::ContentMain()` Startup Workflow

Based on source at `content/app/content_main.cc`, `content_main_runner_impl.cc`, `browser_main_runner_impl.cc`, and `browser_main_loop.cc`.

## Top-Level Entry

```
main()
  └─ content::ContentMain(ContentMainParams)           [content_main.cc:367]
       └─ RunContentProcess(params, runner)             [content_main.cc:206]
            ├─ 1. Platform init (one-time)
            ├─ 2. runner->Initialize(params)
            ├─ 3. runner->Run()
            └─ 4. runner->Shutdown()
```

## Phase 1: Platform Init (`RunContentProcess`, one-time)

```
RunContentProcess()                                     [content_main.cc:206]
  ├─ EnableTerminationOnOutOfMemory()
  ├─ [Linux] setenv("DBUS_SESSION_BUS_ADDRESS", "disabled:")  // prevent dbus auto-launch hangs
  ├─ CommandLine::Init(argc, argv)
  ├─ EnableTerminationOnHeapCorruption()
  ├─ SetProcessTitleFromCommandLine()
  ├─ InitTimeTicksAtUnixEpoch()
  ├─ [POSIX] setlocale(LC_ALL, ""), setlocale(LC_NUMERIC, "C")
  ├─ [POSIX] SetupSignalHandlers()                     // ignore SIGPIPE, reset others
  ├─ [Mac] InitializeMac()
  ├─ ui::RegisterPathProvider()
  └─ runner->Initialize(params)                         // → Phase 2
```

## Phase 2: ContentMainRunnerImpl::Initialize

This is where the process type is determined and delegate callbacks fire:

```
ContentMainRunnerImpl::Initialize()                     [content_main_runner_impl.cc:783]
  │
  ├─ Create base::AtExitManager
  │
  ├─ delegate->BasicStartupComplete()                   ◄── EMBEDDER HOOK
  │   └─ [my_app] Creates AppContentClient, calls SetContentClient()
  │       registers myapp:// scheme, initializes logging
  │
  ├─ SetContentClient(delegate->CreateContentClient())
  │   └─ ContentClient::AddAdditionalSchemes()
  │       └─ Registers custom URL schemes (myapp://)
  │
  ├─ RegisterContentSchemes()                           // lock in all URL schemes
  │
  ├─ [Browser] delegate->PreSandboxStartup()            ◄── EMBEDDER HOOK
  │   └─ [my_app] Loads my_app.pak into ResourceBundle
  │
  ├─ [Browser] Initialize tracing (Perfetto)
  │
  ├─ [Browser] delegate->PostEarlyInitialization()      ◄── EMBEDDER HOOK
  │
  ├─ Determine process type from --process-type flag:
  │   ├─ "" (empty)      → Browser process
  │   ├─ "renderer"      → Renderer process
  │   ├─ "gpu-process"   → GPU process
  │   ├─ "utility"       → Utility process
  │   └─ "zygote"        → Zygote (Linux fork server)
  │
  └─ return -1 (continue to Run)
```

## Phase 3: ContentMainRunnerImpl::Run

Dispatches to the correct process main function:

```
ContentMainRunnerImpl::Run()                            [content_main_runner_impl.cc:1100]
  │
  ├─ Initialize FeatureList, FieldTrials
  ├─ Initialize Perfetto tracing
  ├─ Initialize Mojo core
  ├─ Reconfigure PartitionAlloc
  │
  ├─ Check --process-type:
  │   │
  │   ├─ [Browser: --process-type is empty]
  │   │   └─ RunBrowser()                               // → Phase 3a
  │   │
  │   ├─ [Renderer: --process-type=renderer]
  │   │   └─ RendererMain()
  │   │
  │   ├─ [GPU: --process-type=gpu-process]
  │   │   └─ GpuMain()
  │   │
  │   └─ [Utility: --process-type=utility]
  │       └─ UtilityMain()
  │
  └─ return exit_code
```

## Phase 3a: RunBrowser (Browser Process Only)

```
ContentMainRunnerImpl::RunBrowser()                     [content_main_runner_impl.cc:1168]
  │
  ├─ Setup FeatureList for browser
  ├─ Initialize Mojo core (broker mode)
  ├─ Create memory pressure monitor
  │
  ├─ delegate->PreBrowserMain()                         ◄── EMBEDDER HOOK
  │
  ├─ BrowserTaskExecutor::Create()                      // UI + IO thread task runners
  ├─ Setup variations IDs provider
  │
  ├─ delegate->PostEarlyInitialization()                ◄── EMBEDDER HOOK
  │
  ├─ Start HangWatcher (detects hung threads)
  ├─ Initialize Perfetto tracing backend
  ├─ Start ThreadPool
  ├─ Initialize PowerMonitor
  ├─ Create ProcessPriorityTracker
  ├─ Create DiscardableSharedMemoryManager
  ├─ Create MojoIpcSupport                              // Mojo broker for child processes
  │
  └─ RunBrowserProcessMain()
       ├─ delegate->RunProcess("")                      ◄── EMBEDDER HOOK (optional override)
       │   └─ If returns >= 0, use that exit code
       └─ [default] BrowserMain()                       // → Phase 4
```

## Phase 4: BrowserMain → BrowserMainRunnerImpl

```
BrowserMain()                                           [browser_main.cc]
  └─ BrowserMainRunnerImpl::Initialize()                [browser_main_runner_impl.cc:69]
       │
       ├─ Initialize Skia
       ├─ [Win] OLE initialization
       ├─ Initialize fonts
       │
       ├─ Create BrowserMainLoop
       ├─ BrowserMainLoop::Init()
       ├─ BrowserMainLoop::EarlyInitialization()
       │   └─ parts->PreEarlyInitialization()           ◄── EMBEDDER HOOK
       │
       ├─ BrowserMainLoop::InitializeToolkit()          // GTK, Aura, etc.
       ├─ BrowserMainLoop::PreCreateMainMessageLoop()
       ├─ BrowserMainLoop::CreateMainMessageLoop()      // creates the UI message loop
       ├─ BrowserMainLoop::PostCreateMainMessageLoop()
       ├─ Initialize InputMethod
       │
       └─ BrowserMainLoop::CreateStartupTasks()         // → Phase 5
```

## Phase 5: BrowserMainLoop::CreateStartupTasks

Tasks run **sequentially** on the main thread (synchronous on desktop, async on Android):

```
CreateStartupTasks()                                    [browser_main_loop.cc:856]
  │
  ├─ Task 1: PreCreateThreads()
  │   └─ parts->PreCreateThreads()                      ◄── EMBEDDER HOOK
  │       └─ [my_app] Creates AppBrowserContext, initializes NotificationManager
  │
  ├─ Task 2: CreateThreads()
  │   ├─ Release ThreadPool execution fence (tasks start running)
  │   ├─ Create and register IO thread (BrowserProcessIOThread)
  │   └─ Post startup-complete callback
  │
  ├─ Task 3: PostCreateThreads()
  │   └─ parts->PostCreateThreads()                     ◄── EMBEDDER HOOK
  │
  ├─ Task 4: PreMainMessageLoopRun()
  │   ├─ Setup font rendering, Skia display globals
  │   ├─ GPU host font params
  │   ├─ parts->PreMainMessageLoopRun()                 ◄── EMBEDDER HOOK
  │   │   └─ [my_app] Creates WMState, Screen, ViewsDelegate
  │   │       Creates first AppWindow → WebContents → loads URL
  │   ├─ Initialize First-Party Sets
  │   └─ Schedule OnFirstIdle callback
  │
  └─ All tasks done → return to BrowserMain
```

## Phase 6: Message Loop

```
BrowserMainRunnerImpl::Run()                            [browser_main_runner_impl.cc:145]
  └─ BrowserMainLoop::RunMainMessageLoop()              [browser_main_loop.cc:1103]
       │
       ├─ parts->WillRunMainMessageLoop(run_loop)       ◄── EMBEDDER HOOK
       │   └─ [my_app] Captures quit closure for app exit
       │
       └─ run_loop->Run()                               // ← BLOCKS HERE
            │                                           // App is alive, processing events
            │                                           // until quit_closure is called
            └─ [quit_closure called when last window closes]
```

## Phase 7: Shutdown

```
[run_loop exits]
  │
  ├─ BrowserMainRunnerImpl::Shutdown()                  [browser_main_runner_impl.cc:152]
  │   ├─ BrowserMainLoop::PreShutdown()
  │   ├─ Set "exited main message loop" flag
  │   ├─ parts->PostMainMessageLoopRun()                ◄── EMBEDDER HOOK
  │   │   └─ [my_app] CloseAllWindows, shutdown notifications, reset context
  │   ├─ BrowserMainLoop::ShutdownThreadsAndCleanUp()
  │   │   ├─ Shutdown IO thread
  │   │   ├─ Shutdown ThreadPool
  │   │   └─ Destroy BrowserMainLoop
  │   ├─ Shutdown InputMethod
  │   └─ Reset BrowserMainLoop
  │
  ├─ ContentMainRunnerImpl::Shutdown()                  [content_main_runner_impl.cc:1334]
  │   ├─ Tear down MojoIpcSupport
  │   ├─ delegate->ProcessExiting()                     ◄── EMBEDDER HOOK
  │   ├─ Shutdown BrowserTaskExecutor
  │   └─ Reset AtExitManager (runs atexit callbacks)
  │
  └─ return exit_code to main()
```

## All Embedder Hooks in Execution Order

| # | Hook | Phase | What my_app Does |
|---|------|-------|------------------|
| 1 | `BasicStartupComplete()` | Initialize | Create ContentClient, init logging |
| 2 | `PreSandboxStartup()` | Initialize | Load my_app.pak resources |
| 3 | `PostEarlyInitialization()` | Initialize | (not overridden) |
| 4 | `CreateContentBrowserClient()` | Initialize | Return AppBrowserClient |
| 5 | `CreateContentRendererClient()` | Initialize | Return AppRendererClient |
| 6 | `PreBrowserMain()` | RunBrowser | (not overridden) |
| 7 | `CreateBrowserMainParts()` | RunBrowser | Return AppBrowserMainParts |
| 8 | `PreEarlyInitialization()` | Startup tasks | (not overridden) |
| 9 | `PreCreateThreads()` | Startup tasks | Create BrowserContext, init notifications |
| 10 | `PostCreateThreads()` | Startup tasks | (not overridden) |
| 11 | `PreMainMessageLoopRun()` | Startup tasks | Create window, load URL |
| 12 | `WillRunMainMessageLoop()` | Message loop | Capture quit closure |
| 13 | `PostMainMessageLoopRun()` | Shutdown | Close windows, cleanup |
| 14 | `ProcessExiting()` | Shutdown | (not overridden) |

## Visual Timeline

```
Time ──────────────────────────────────────────────────────────────────────►

│ Phase 1  │ Phase 2    │ Phase 3a        │ Phase 4+5              │ P6   │ P7      │
│ Platform │ Initialize │ RunBrowser      │ BrowserMain            │ Run  │Shutdown │
│ init     │            │                 │                        │      │         │
│          │ Basic      │ PreBrowser      │ PreCreate  PreMainMsg  │ Msg  │ Post    │
│ cmdline  │ Startup    │ Main            │ Threads    LoopRun     │ Loop │ Main    │
│ signals  │ PreSandbox │ TaskExecutor    │ Create     ┌─────────┐ │ Run  │ MsgLoop │
│ locale   │ Schemes    │ ThreadPool     │ Threads    │my_app    │ │      │ Run     │
│          │            │ PowerMonitor    │ PostCreate │window    │ │ ◄──► │         │
│          │            │ MojoIPC         │ Threads    │created   │ │ idle │ Cleanup │
│          │            │                 │            └─────────┘ │      │         │
```

## Process Type Dispatch

The same `my_app` binary serves as both browser and child processes. The `--process-type` flag determines behavior:

```
my_app                                    → Browser process (no flag)
my_app --process-type=renderer ...        → Renderer process (spawned by browser)
my_app --process-type=gpu-process ...     → GPU process (spawned by browser)
my_app --process-type=utility ...         → Utility process (spawned by browser)
my_app --process-type=zygote ...          → Zygote (Linux fork server)
```

The browser process spawns child processes by re-executing itself with `--process-type=<type>`. Each child process goes through Phase 1–2, then in Phase 3 dispatches to its specific main function (`RendererMain`, `GpuMain`, `UtilityMain`) instead of `RunBrowser`.

### Child Process Startup (simplified)

```
ContentMainRunnerImpl::Run() [child process]
  ├─ delegate->CreateContentRendererClient()            // or GPU/Utility client
  ├─ Initialize FieldTrials, FeatureList
  ├─ Initialize Mojo (connect to browser's broker)
  ├─ Create HangWatcher
  └─ RendererMain() / GpuMain() / UtilityMain()
       └─ Enter child process message loop
```

## Key Data Structures

| Structure | Purpose |
|-----------|---------|
| `ContentMainParams` | Carries delegate, argc/argv, autorelease pool from main() to ContentMain |
| `ContentMainRunner` | Interface for Initialize/Run/Shutdown lifecycle |
| `ContentMainRunnerImpl` | Concrete implementation; owns AtExitManager, MojoIpcSupport |
| `BrowserMainRunner` | Browser-specific runner; owns BrowserMainLoop |
| `BrowserMainLoop` | Owns IO thread, startup tasks, message loop, BrowserMainParts |
| `BrowserMainParts` | Embedder lifecycle hooks (what my_app overrides) |
| `StartupTaskRunner` | Runs PreCreateThreads → CreateThreads → PostCreateThreads → PreMainMessageLoopRun |
| `MainFunctionParams` | Parameters passed to process-specific main functions |

## Source File Map

| File | Role |
|------|------|
| `content/app/content_main.cc` | `ContentMain()` + `RunContentProcess()` — top-level entry |
| `content/app/content_main_runner_impl.cc` | `Initialize()`, `Run()`, `RunBrowser()`, `Shutdown()` |
| `content/browser/browser_main.cc` | `BrowserMain()` — bridges to BrowserMainRunnerImpl |
| `content/browser/browser_main_runner_impl.cc` | `Initialize()` (Skia, fonts, BrowserMainLoop), `Run()`, `Shutdown()` |
| `content/browser/browser_main_loop.cc` | `CreateStartupTasks()`, `PreMainMessageLoopRun()`, `RunMainMessageLoop()` |
| `content/public/app/content_main.h` | Public API: `ContentMain()`, `ContentMainParams` |
| `content/public/app/content_main_delegate.h` | Public API: embedder delegate interface |
| `content/public/browser/browser_main_parts.h` | Public API: embedder lifecycle hooks |
