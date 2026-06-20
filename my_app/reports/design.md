# my_app — Custom Chromium Embedder Design Report

## 1. Overview

**my_app** is a custom Chromium embedder that provides a native desktop application shell powered by Chromium's `//content` layer. It renders web-based UI in a native window with full access to OS APIs through Mojo IPC.

**Goals:**
- Maximum control over rendering engine, networking, and process model
- Lean binary size compared to Electron/Chrome
- Low-latency, type-safe IPC between native and web layers
- Cross-platform: Linux, Windows, macOS

**Approach:** Build directly on `//content/public` (not a fork of content_shell), leveraging Chromium's `//ui` layer for native features.

---

## 2. Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                      Browser Process                         │
│                                                              │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────────────┐  │
│  │  AppWindow   │  │  NativeApi   │  │ AppURLLoaderFactory│  │
│  │  (Widget +   │  │  Impl        │  │ (myapp:// scheme)  │  │
│  │  WebContents)│  │  (Mojo)      │  │                    │  │
│  └──────┬───────┘  └──────┬───────┘  └────────┬───────────┘  │
│         │                 │                    │              │
│         │            Mojo IPC            myapp://             │
│         ▼                 │                    │              │
│  ┌────────────────────────┴────────────────────┘              │
│  │              content::WebContents                         │
│  └─────────────────────┬─────────────────────────────────────┘
│                        │                                      │
│  ┌─────────────────────┴─────────────────────────────────┐   │
│  │              Native Bridges                            │   │
│  │  ClipboardBridge │ FileDialogBridge │ MenuManager      │   │
│  │  NotificationMgr │ SystemTray                          │   │
│  └────────────────────────────────────────────────────────┘   │
└────────────────────────┬─────────────────────────────────────┘
                         │ IPC (Mojo channels)
┌────────────────────────▼─────────────────────────────────────┐
│                    Renderer Process                           │
│  ┌───────────────────────────────────────────────────────┐   │
│  │         Blink (HTML/CSS/JS rendering engine)          │   │
│  │         ┌─────────────────────────────────┐           │   │
│  │         │  Web Frontend (HTML/CSS/JS)     │           │   │
│  │         │  loaded from myapp://app/       │           │   │
│  │         └─────────────────────────────────┘           │   │
│  └───────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────────────┐
│                      GPU Process                              │
│  Hardware-accelerated compositing, WebGL, video decode        │
└──────────────────────────────────────────────────────────────┘
```

---

## 3. Startup Flow

```
main()                                          [app/main.cc]
 └─ content::ContentMain(AppMainDelegate)
     │
     ├─ BasicStartupComplete()                  [app/main_delegate.cc]
     │   └─ Creates AppContentClient
     │       └─ Registers myapp:// as standard, secure scheme
     │
     ├─ PreSandboxStartup()                     [app/main_delegate.cc]
     │   └─ Loads my_app.pak (repacked resources)
     │
     ├─ CreateContentBrowserClient()            [app/main_delegate.cc]
     │   └─ Returns AppBrowserClient
     │
     ├─ CreateContentRendererClient()           [app/main_delegate.cc]
     │   └─ Returns AppRendererClient
     │
     └─ [Browser process starts]
         │
         ├─ AppBrowserClient::CreateBrowserMainParts()
         │   └─ Returns AppBrowserMainParts      [browser/app_browser_client.cc]
         │
         ├─ PreCreateThreads()                   [browser/app_browser_main_parts.cc]
         │   ├─ Creates AppBrowserContext
         │   │   └─ Storage dir: ~/.config/MyApp
         │   └─ NotificationManager::Initialize()
         │
         ├─ PreMainMessageLoopRun()              [browser/app_browser_main_parts.cc]
         │   ├─ Creates wm::WMState
         │   ├─ Creates display::Screen (via views::CreateDesktopScreen)
         │   ├─ Creates AppViewsDelegate
         │   │   └─ OnBeforeWidgetInit → DesktopNativeWidgetAura for top-level
         │   ├─ Checks --dev-server flag
         │   └─ AppWindow::Create(context, url)
         │       ├─ Creates WebContents
         │       ├─ Creates WebView (displays WebContents)
         │       ├─ Creates Widget (CLIENT_OWNS_WIDGET + TYPE_WINDOW)
         │       └─ Loads initial URL (myapp://app/ or --dev-server URL)
         │
         ├─ WillRunMainMessageLoop()
         │   └─ Captures quit closure for app exit
         │
         └─ [Message loop runs — app is alive]
```

---

## 4. Component Details

### 4.1 AppMainDelegate (`app/main_delegate.h/.cc`)

The central dispatcher. Implements `content::ContentMainDelegate`.

| Method | Purpose |
|--------|---------|
| `BasicStartupComplete()` | Creates `AppContentClient`, calls `SetContentClient()`, initializes logging |
| `PreSandboxStartup()` | Loads `my_app.pak` resource bundle from `DIR_ASSETS` |
| `CreateContentBrowserClient()` | Returns `AppBrowserClient` (browser process) |
| `CreateContentRendererClient()` | Returns `AppRendererClient` (renderer process) |

### 4.2 AppContentClient (`common/app_content_client.h/.cc`)

Shared across all process types. Implements `content::ContentClient`.

| Method | Purpose |
|--------|---------|
| `AddAdditionalSchemes()` | Registers `myapp://` as standard, secure, CORS-enabled, CSP-bypassing |
| `GetLocalizedString()` | Delegates to `l10n_util` |
| `GetDataResource()` | Delegates to `ResourceBundle` |

### 4.3 AppBrowserClient (`browser/app_browser_client.h/.cc`)

The browser process embedder. Implements `content::ContentBrowserClient`.

| Method | Purpose |
|--------|---------|
| `CreateBrowserMainParts()` | Returns `AppBrowserMainParts` |
| `GetUserAgent()` | Returns `"MyApp/1.0"` |
| `GetAcceptLangs()` | Returns `"en-US,en"` |
| `RegisterBrowserInterfaceBindersForFrame()` | Binds `mojom::NativeApi` for Mojo IPC |
| `CreateNonNetworkNavigationURLLoaderFactory()` | Returns `AppURLLoaderFactory` for `myapp://` navigations |
| `RegisterNonNetworkSubresourceURLLoaderFactories()` | Returns `AppURLLoaderFactory` for `myapp://` subresources |

### 4.4 AppBrowserContext (`browser/app_browser_context.h/.cc`)

Profile/storage context. Implements `content::BrowserContext`.

- Storage path: `~/.config/MyApp` (created on construction)
- Not off-the-record
- Returns `nullptr` for most optional delegates (downloads, permissions, push, etc.)
- Supports Service Workers, IndexedDB, CacheStorage via the storage path

### 4.5 AppBrowserMainParts (`browser/app_browser_main_parts.h/.cc`)

Browser process lifecycle. Implements `content::BrowserMainParts`.

| Phase | Action |
|-------|--------|
| `PreCreateThreads()` | Creates `AppBrowserContext`, initializes `NotificationManager` |
| `PreMainMessageLoopRun()` | Initializes windowing (WMState, Screen, ViewsDelegate), creates first `AppWindow` |
| `WillRunMainMessageLoop()` | Captures quit closure from `RunLoop` |
| `PostMainMessageLoopRun()` | Closes all windows, shuts down notifications, resets context |

**AppViewsDelegate:** Custom `views::ViewsDelegate` subclass defined in this file. Overrides `OnBeforeWidgetInit()` to create `DesktopNativeWidgetAura` for top-level windows (required for desktop Linux/Windows windowing).

### 4.6 AppWindow (`browser/app_window.h/.cc`)

The main application window. Combines three roles:

| Base Class | Role |
|------------|------|
| `views::WidgetDelegate` | Native window properties (title, resize, controls) |
| `content::WebContentsDelegate` | Handles web content events (close, console messages) |
| `content::WebContentsObserver` | Observes navigation/title changes |

**Ownership model:**
- `AppWindow` owns `WebContents` (via `unique_ptr`)
- `AppWindow` owns `Widget` (via `unique_ptr`, `CLIENT_OWNS_WIDGET`)
- `Widget` owns the view tree (including `WebView`)
- `WebView` is a non-owning `raw_ptr` (owned by Widget's view hierarchy)

**Lifecycle:**

```
Create:
  new AppWindow → WebContents + WebView + Widget → Show + LoadURL

Close (user or JS):
  CloseContents() → widget_->Close()
    → async native close → WidgetIsZombie()
    → web_view_ = nullptr (avoid dangling ptr)
    → widget_.reset() (destroys widget + view tree)
    → delete this (destroys AppWindow + WebContents)
    → ~AppWindow: remove from static window list, quit if last

Shutdown:
  CloseAllWindows() → for each window: null web_view_, reset widget_, delete window
```

**Static management:**
- `GetWindows()` — static vector of all live AppWindow pointers
- `SetQuitClosure()` — stores the RunLoop quit closure; called when last window closes

### 4.7 AppURLLoaderFactory (`browser/app_url_loader_factory.h/.cc`)

Serves bundled web resources for the `myapp://` URL scheme.

Extends `network::SelfDeletingURLLoaderFactory`. Self-owned (destroyed when the Mojo pipe disconnects).

**Resource mapping:**

| URL Path | Resource ID | MIME Type |
|----------|-------------|-----------|
| `/index.html` (or `/`) | `IDR_MY_APP_INDEX_HTML` | `text/html` |
| `/app.js` | `IDR_MY_APP_APP_JS` | `application/javascript` |
| `/app.css` | `IDR_MY_APP_APP_CSS` | `text/css` |

**Serving flow:**
1. Extract path from `request.url.path()`
2. Look up resource ID in static table
3. Load raw data from `ResourceBundle::GetRawDataResource()`
4. Detect gzip compression (magic bytes `0x1F 0x8B`)
5. Decompress with `compression::GzipUncompress()` if compressed
6. Write to Mojo data pipe via `BeginWriteData/EndWriteData`
7. Send `OnReceiveResponse` + `OnComplete` to client

**Why gzip decompression?** Chromium's grit tool compresses BINDATA resources with gzip by default in official builds. `GetRawDataResource()` returns the raw (compressed) bytes. Since `Content-Encoding: gzip` doesn't work for custom URL schemes, we decompress before serving.

### 4.8 NativeApi Mojo Interface (`common/mojom/native_api.mojom`)

Defines the IPC contract between renderer (JS) and browser (native) processes.

```mojom
module my_app.mojom;

struct MenuItem {
  int32 id;
  string label;
  bool enabled;
  bool checked;
};

interface NativeApi {
  ReadClipboardText() => (string text);
  WriteClipboardText(string text);
  ReadClipboardHtml() => (string html);
  ShowOpenFileDialog(string title, array<string> extensions)
      => (array<string> paths);
  ShowSaveFileDialog(string title, string default_name,
                     array<string> extensions) => (string? path);
  ShowNotification(string title, string body) => (string notification_id);
  SetSystemTrayTooltip(string tooltip);
  SetSystemTrayVisible(bool visible);
  ShowContextMenu(array<MenuItem> items) => (int32 selected_id);
};
```

**Binding:** `AppBrowserClient::RegisterBrowserInterfaceBindersForFrame()` maps `mojom::NativeApi` → `NativeApiImpl::Create()` using `mojo::MakeSelfOwnedReceiver`.

### 4.9 NativeApiImpl (`browser/ipc/native_api_impl.h/.cc`)

Browser-side implementation of the `NativeApi` Mojo interface. Each method delegates to a native bridge class:

| Method | Bridge | Chromium API |
|--------|--------|-------------|
| `ReadClipboardText/Html` | `ClipboardBridge` | `ui::Clipboard` (async, callback-based) |
| `WriteClipboardText` | `ClipboardBridge` | `ui::ScopedClipboardWriter` |
| `ShowOpenFileDialog` | `FileDialogBridge` | `ui::SelectFileDialog` |
| `ShowSaveFileDialog` | `FileDialogBridge` | `ui::SelectFileDialog` |
| `ShowNotification` | `NotificationManager` | `message_center::MessageCenter` |
| `SetSystemTrayTooltip/Visible` | `SystemTray` | Stub (platform-specific impl deferred) |
| `ShowContextMenu` | `MenuManager` | `ui::SimpleMenuModel` + `views::MenuRunner` |

### 4.10 Native Bridge Classes

#### ClipboardBridge (`browser/native/clipboard_bridge.h/.cc`)
- `ReadText()` — `ui::Clipboard::GetForCurrentThread()->ReadText()` with UTF-16→UTF-8 conversion
- `WriteText()` — `ui::ScopedClipboardWriter` (RAII, atomic commit)
- `ReadHtml()` — `ui::Clipboard::ReadHTML()` with UTF-16→UTF-8 conversion
- All read operations are async (callback-based) matching Chromium's clipboard API

#### FileDialogBridge (`browser/native/file_dialog_bridge.h/.cc`)
- Implements `ui::SelectFileDialog::Listener`
- Self-deleting: `new FileDialogBridge(callback)` → fires callback → `delete this`
- Supports: `SELECT_OPEN_MULTI_FILE`, `SELECT_SAVEAS_FILE`
- File type filtering via extensions

#### MenuManager (`browser/native/menu_manager.h/.cc`)
- Implements `ui::SimpleMenuModel::Delegate`
- Builds `SimpleMenuModel` from `mojom::MenuItem` array
- Runs via `views::MenuRunner` with `CONTEXT_MENU` type
- Self-deleting via `DeleteSoon` after menu closes (avoids use-after-free in callback chain)

#### NotificationManager (`browser/native/notification_manager.h/.cc`)
- Static `Initialize()`/`Shutdown()` wrapping `message_center::MessageCenter`
- `Show()` creates `message_center::Notification` with UUID, adds to MessageCenter

#### SystemTray (`browser/native/system_tray.h/.cc`)
- Abstract base class with `SetTooltip()`, `SetVisible()`
- Singleton via `base::NoDestructor`
- Current implementation: `SystemTrayStub` (logs only)
- Platform-specific backends (Linux StatusIconLinux, macOS NSStatusItem, Windows Shell_NotifyIcon) deferred

---

## 5. Resource Pipeline

```
Source files:                    Build:                        Runtime:
resources/web/index.html  ──┐
resources/web/app.js      ──┼─→ grit → my_app_resources.pak ──┐
resources/web/app.css     ──┘                                  │
                                                               ├─→ repack → my_app.pak
content_resources.pak     ─────────────────────────────────────┤
blink_resources.pak       ─────────────────────────────────────┤
ui_resources.pak          ─────────────────────────────────────┤
...other paks...          ─────────────────────────────────────┘
                                                                    │
                                                     PreSandboxStartup
                                                     loads my_app.pak
                                                            │
                                                     AppURLLoaderFactory
                                                     serves from ResourceBundle
```

**GRIT manifest:** `resources/my_app_resources.grd` defines three BINDATA includes.

**Resource IDs:** Registered in `tools/gritsettings/resource_ids.spec` starting at 11900.

**Repack target:** `BUILD.gn:repack("pak")` combines app resources with Chromium's content, blink, UI, and mojo resources into a single `my_app.pak`.

---

## 6. Build System

### Targets (`my_app/BUILD.gn`)

| Target | Type | Purpose |
|--------|------|---------|
| `my_app_resources_grit` | `grit()` | Compiles web resources into `.pak` |
| `pak` | `repack()` | Combines all `.pak` files into `my_app.pak` |
| `my_app_lib` | `static_library` | All embedder code (browser, renderer, common) |
| `my_app_app` | `static_library` | `AppMainDelegate` |
| `my_app` | `executable` | Final binary (Linux/Win) or `mac_app_bundle` (macOS) |

### Dependencies

```
my_app (executable)
 └─ my_app_app (main delegate)
     └─ my_app_lib (embedder code)
         ├─ content/public/{app,browser,common,renderer}
         ├─ ui/{views,base,aura,display,gfx,wm,shell_dialogs,message_center}
         ├─ mojo/public/cpp/bindings
         ├─ my_app/common/mojom (generated Mojo bindings)
         ├─ services/network/public/{cpp,mojom}
         ├─ third_party/zlib/google:compression_utils
         ├─ net, url, skia, base
         └─ [platform] ui/aura, ui/wm/public (if use_aura)
```

### Build Configurations

**Debug (fast iteration):**
```
is_debug = true
is_component_build = true
```

**Release (optimized):**
```
is_debug = false
is_official_build = true
is_component_build = false
symbol_level = 0
use_thin_lto = true
chrome_pgo_phase = 0
```

---

## 7. File Listing

```
my_app/
├── BUILD.gn                                    # Build targets
├── DEPS                                        # Include rules
├── app/
│   ├── main.cc                                 # Entry point
│   ├── main_delegate.cc                        # ContentMainDelegate
│   └── main_delegate.h
├── browser/
│   ├── app_browser_client.cc                   # ContentBrowserClient
│   ├── app_browser_client.h
│   ├── app_browser_context.cc                  # BrowserContext (~/.config/MyApp)
│   ├── app_browser_context.h
│   ├── app_browser_main_parts.cc               # Lifecycle + AppViewsDelegate
│   ├── app_browser_main_parts.h
│   ├── app_url_loader_factory.cc               # myapp:// resource serving
│   ├── app_url_loader_factory.h
│   ├── app_window.cc                           # Window (Widget + WebContents)
│   ├── app_window.h
│   ├── ipc/
│   │   ├── native_api_impl.cc                  # Mojo NativeApi browser-side
│   │   └── native_api_impl.h
│   └── native/
│       ├── clipboard_bridge.cc                 # ui::Clipboard wrapper
│       ├── clipboard_bridge.h
│       ├── file_dialog_bridge.cc               # ui::SelectFileDialog wrapper
│       ├── file_dialog_bridge.h
│       ├── menu_manager.cc                     # SimpleMenuModel + MenuRunner
│       ├── menu_manager.h
│       ├── notification_manager.cc             # message_center wrapper
│       ├── notification_manager.h
│       ├── system_tray.cc                      # Abstract + stub
│       └── system_tray.h
├── common/
│   ├── app_content_client.cc                   # ContentClient + scheme registration
│   ├── app_content_client.h
│   ├── app_switches.cc                         # CLI switches (--dev-server)
│   ├── app_switches.h
│   └── mojom/
│       ├── BUILD.gn                            # mojom() target
│       └── native_api.mojom                    # NativeApi interface definition
├── renderer/
│   ├── app_renderer_client.cc                  # ContentRendererClient (minimal)
│   └── app_renderer_client.h
├── resources/
│   ├── my_app_resources.grd                    # GRIT manifest
│   └── web/
│       ├── app.css                             # Stylesheet
│       ├── app.js                              # Frontend JS
│       └── index.html                          # Main page
└── tools/
    └── args_release.gn                         # Release build args
```

**Total: 39 source files**

---

## 8. Design Decisions & Rationale

| Decision | Rationale |
|----------|-----------|
| `CLIENT_OWNS_WIDGET` ownership | Gives explicit control over Widget lifetime; works with `WidgetIsZombie()` for clean shutdown |
| `DesktopNativeWidgetAura` via `AppViewsDelegate` | Required for desktop Linux/Windows; plain `NativeWidgetAura` needs parent/context |
| Gzip decompression in URLLoaderFactory | Grit compresses BINDATA resources; `Content-Encoding: gzip` doesn't work for custom schemes |
| `SelfDeletingURLLoaderFactory` | Self-owned via Mojo pipe; no manual lifecycle management |
| `MakeSelfOwnedReceiver` for NativeApiImpl | One impl per renderer frame; auto-destroyed when pipe disconnects |
| `chrome_pgo_phase = 0` in release args | Avoids requiring PGO profiles that need `gclient runhooks` |
| `web_view_ = nullptr` before `widget_.reset()` | Prevents dangling `raw_ptr` since WebView is owned by Widget's view tree |
| `~AppBrowserContext` calls `NotifyWillBeDestroyed()` + `ShutdownStoragePartitions()` | Required by `BrowserContext` contract for clean shutdown |
| Storage at `~/.config/MyApp` via `DIR_HOME` | `DIR_APP_DATA` doesn't exist in this Chromium version; hardcoded Linux path |

---

## 9. Runtime Flags

| Flag | Purpose |
|------|---------|
| `--no-sandbox` | Required when running without suid sandbox helper |
| `--dev-server=URL` | Load from external URL instead of bundled `myapp://` resources |

---

## 10. Known Limitations

1. **System tray is a stub** — platform-specific implementations (Linux StatusIconLinux, macOS NSStatusItem, Windows NOTIFYICONDATA) not yet implemented
2. **Mojo JS bindings not wired** — WebUI module infrastructure needed to call `NativeApi` from JS; C++ side is fully functional
3. **Storage path is Linux-only** — uses `DIR_HOME/.config/MyApp`; needs platform abstraction for macOS/Windows
4. **Single window** — no tab support; each `AppWindow` has one `WebContents`
5. **No DevTools** — DevTools frontend not integrated yet
6. **Shutdown warnings** — benign `MojoDiscardableSharedMemoryManagerImpls leaked` and `Unable to terminate process` warnings during exit
