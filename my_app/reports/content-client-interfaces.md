# ContentClient, ContentBrowserClient, ContentRendererClient

## The Three Embedder Clients

These are the primary interfaces `//content` provides for embedders to customize behavior. Each runs in a different process context:

```
ContentMainDelegate (app/main_delegate.cc)
  │
  ├─ content_client_   → ContentClient        → ALL processes
  ├─ browser_client_   → ContentBrowserClient  → Browser process only
  └─ renderer_client_  → ContentRendererClient → Renderer process only
```

---

## 1. ContentClient — All Processes

### Role

**Read-only shared state** across all processes. Answers questions ("what schemes exist?", "give me this resource") but never controls behavior.

It is the **first embedder code that runs** and the **last to be destroyed**.

### Global Singleton

```cpp
// content/public/common/content_client.cc
static ContentClient* g_client;

void SetContentClient(ContentClient* client) { g_client = client; }
ContentClient* GetContentClient()            { return g_client; }
```

Set during `BasicStartupComplete()` — before anything else:

```cpp
// my_app/app/main_delegate.cc
std::optional<int> AppMainDelegate::BasicStartupComplete() {
  content_client_ = std::make_unique<AppContentClient>();
  content::SetContentClient(content_client_.get());
  return std::nullopt;
}
```

### What It Provides

1. **Cross-process configuration** — URL schemes, origin trials
2. **Cross-process resources** — localized strings, images, binary data from pak files
3. **Sub-client gateway** — `browser()` / `renderer()` / `gpu()` / `utility()` for content's internal dispatch

Everything on ContentClient must make sense in **every** process type.

### The `Schemes` Struct

```cpp
struct Schemes {
  std::vector<std::string> standard_schemes;        // URL parsing (host/path/query)
  std::vector<std::string> referrer_schemes;        // Sent in Referer header
  std::vector<std::string> secure_schemes;          // Treated as HTTPS-equivalent
  std::vector<std::string> local_schemes;           // file://-equivalent
  std::vector<std::string> no_access_schemes;       // Cannot be accessed by web content
  std::vector<std::string> cors_enabled_schemes;    // CORS applies
  std::vector<std::string> csp_bypassing_schemes;   // Bypass Content-Security-Policy
  std::vector<std::string> empty_document_schemes;  // about:blank-equivalent
  std::vector<std::string> service_worker_schemes;  // Can register Service Workers
  std::vector<std::string> extension_schemes;       // Chrome extension-like
  std::vector<std::string> isolated_app_schemes;    // Isolated Web App
};
```

### Scheme Registration Flow

```
ContentMainRunnerImpl::Initialize()
  └─ RegisterContentSchemes()                    [content/common/url_schemes.cc]
       ├─ Register built-in schemes (chrome://, chrome-devtools://, etc.)
       ├─ GetContentClient()->AddAdditionalSchemes(&schemes)  ◄── EMBEDDER
       │   └─ [my_app] Adds "myapp" to standard, secure, cors_enabled, csp_bypassing
       ├─ url::AddStandardScheme("myapp"), url::AddSecureScheme("myapp"), ...
       └─ url::LockSchemeRegistries()            // NO MORE CHANGES AFTER THIS
```

### All Virtual Methods

| Category | Method | Purpose |
|----------|--------|---------|
| **Schemes** | `AddAdditionalSchemes(Schemes*)` | Register custom URL schemes |
| **Resources** | `GetLocalizedString(id)` | Localized UI string |
| | `GetDataResource(id, scale)` | Binary data from pak |
| | `GetDataResourceBytes(id)` | Resource as RefCountedMemory |
| | `GetDataResourceString(id)` | Resource as string |
| | `GetNativeImageNamed(id)` | Platform-native image |
| **Plugins/CDM** | `AddPlugins(plugins)` | Register plugins |
| | `AddContentDecryptionModules(cdms)` | Register DRM modules |
| **Diagnostics** | `SetActiveURL(url, origin)` | Crash report context |
| | `SetGpuInfo(info)` | GPU info for crash reports |
| | `GetProcessTypeNameInEnglish(type)` | Human-readable process name |
| **Security** | `GetOriginTrialPolicy()` | Origin trial configuration |
| | `IsFilePickerAllowedForCrossOriginSubframe()` | File picker policy |
| **Navigation** | `ShouldIgnoreDuplicateNavs(handle)` | Suppress duplicate navs |
| **Mojo** | `ExposeInterfacesToBrowser(runner, binders)` | Child→browser interfaces |
| **Sub-clients** | `browser()` / `renderer()` / `gpu()` / `utility()` | Internal dispatch gateway |

### my_app's Implementation

```cpp
// my_app/common/app_content_client.cc

void AppContentClient::AddAdditionalSchemes(Schemes* schemes) {
  schemes->standard_schemes.push_back("myapp");
  schemes->secure_schemes.push_back("myapp");
  schemes->cors_enabled_schemes.push_back("myapp");
  schemes->csp_bypassing_schemes.push_back("myapp");
}

// GetLocalizedString, GetDataResource, GetDataResourceBytes,
// GetDataResourceString, GetNativeImageNamed
// — all delegate to ui::ResourceBundle
```

### Key Characteristic: Does NOT Control Behavior

| | ContentClient | ContentBrowserClient | ContentRendererClient |
|---|---|---|---|
| **Role** | Identity & resources | Control browser behavior | Control renderer behavior |
| **Verb** | "What is / Give me" | "Should / Do / Create" | "Should / Do / Create" |
| **Examples** | `GetDataResource()`, `AddAdditionalSchemes()` | `CreateBrowserMainParts()`, `ShouldLockProcessToSite()` | `RenderFrameCreated()`, `PrepareErrorPage()` |
| **Mutates state?** | No (declares, provides) | Yes (decides, creates, blocks) | Yes (hooks, modifies) |

---

## 2. ContentBrowserClient — Browser Process Only

### Role

The **biggest** embedder interface (~100+ virtual methods). Controls everything the browser process does: process model, navigation, permissions, networking, Mojo bindings, DevTools.

### Key Methods by Category

**Lifecycle:**

| Method | Purpose |
|--------|---------|
| `CreateBrowserMainParts(is_integration_test)` | Return embedder's BrowserMainParts for startup/shutdown hooks |

**Process Model:**

| Method | Purpose |
|--------|---------|
| `ShouldTryToUseExistingProcessHost(context, url)` | Reuse existing renderer? |
| `GetProcessCount()` | Max renderer processes |
| `ShouldLockProcessToSite(context, lock_url)` | Site isolation per-process lock |

**Navigation:**

| Method | Purpose |
|--------|---------|
| `CreateThrottlesForNavigation(handle)` | Add NavigationThrottles (block/redirect) |
| `ShouldOverrideUrlLoading(params)` | Override URL loading (Android intents, etc.) |
| `OverrideNavigationParams(context, url, ...)` | Modify navigation parameters |

**Permissions:**

| Method | Purpose |
|--------|---------|
| `AllowServiceWorker(scope, site_for_cookies, ...)` | Allow SW registration? |
| `AllowSharedWorker(url, site_for_cookies, ...)` | Allow shared worker? |

**Network:**

| Method | Purpose |
|--------|---------|
| `ConfigureNetworkContextParams(context, params)` | Proxy, SSL, cache settings |
| `CreateNonNetworkNavigationURLLoaderFactory(scheme, id)` | Custom scheme navigation |
| `RegisterNonNetworkSubresourceURLLoaderFactories(...)` | Custom scheme subresources |

**Mojo IPC:**

| Method | Purpose |
|--------|---------|
| `RegisterBrowserInterfaceBindersForFrame(rfh, map)` | Frame-scoped Mojo bindings |
| `ExposeInterfacesToRenderer(registry, rfh)` | Process-scoped renderer interfaces |

**UI:**

| Method | Purpose |
|--------|---------|
| `GetUserAgent()` | User agent string |
| `GetAcceptLangs(context)` | Accept-Language header |
| `GetDefaultDownloadName()` | Default filename for downloads |
| `OverrideWebPreferences(rfh, prefs)` | Modify renderer preferences |

**Security:**

| Method | Purpose |
|--------|---------|
| `GetSandboxType()` | Sandbox restrictions per process |
| `IsHandledURL(url)` | Whether content handles this URL |

**DevTools:**

| Method | Purpose |
|--------|---------|
| `CreateDevToolsManagerDelegate()` | DevTools customization |

### my_app's Implementation

```cpp
// my_app/browser/app_browser_client.cc

class AppBrowserClient : public ContentBrowserClient {
  CreateBrowserMainParts()                            → AppBrowserMainParts
  GetUserAgent()                                      → "MyApp/1.0"
  GetAcceptLangs()                                    → "en-US,en"
  RegisterBrowserInterfaceBindersForFrame()            → binds mojom::NativeApi
  CreateNonNetworkNavigationURLLoaderFactory()         → myapp:// navigation
  RegisterNonNetworkSubresourceURLLoaderFactories()    → myapp:// subresources
};
```

---

## 3. ContentRendererClient — Renderer Process Only

### Role

Customizes renderer behavior. Smaller than the browser client (~30 virtual methods). Runs in the **sandboxed** renderer process.

### Key Methods

**Startup:**

| Method | Purpose |
|--------|---------|
| `RenderThreadStarted()` | Renderer thread initialized — register observers, schemes |
| `RenderFrameCreated(RenderFrame*)` | New frame — register frame observers, inject JS |

**Error Handling:**

| Method | Purpose |
|--------|---------|
| `PrepareErrorPage(rfh, error, html)` | Customize error page HTML |
| `PrepareErrorPageForHttpStatusError(rfh, url, status, html)` | HTTP error pages |

**Security:**

| Method | Purpose |
|--------|---------|
| `AllowPopup()` | Allow window.open? |
| `ShouldFork(url, http_method, is_initial_nav)` | Force cross-process navigation? |

**Scripts:**

| Method | Purpose |
|--------|---------|
| `RunScriptsAtDocumentStart(frame)` | Inject scripts at document start |
| `RunScriptsAtDocumentEnd(frame)` | Inject scripts at document end |

### my_app's Implementation

```cpp
// my_app/renderer/app_renderer_client.cc

class AppRendererClient : public ContentRendererClient {
  RenderThreadStarted()    → (empty — no customization yet)
  RenderFrameCreated()     → (empty — no customization yet)
};
```

Minimal — my_app doesn't customize renderer behavior yet.

---

## 4. Why Three Separate Classes?

### Process Isolation

The same binary runs as all process types, but each process only creates **its own** sub-client:

```
┌─── Browser Process ─────────────────────┐    ┌─── Renderer Process ──────────────────┐
│                                         │    │                                       │
│  ContentClient        ✅ exists         │    │  ContentClient        ✅ exists       │
│    ├─ browser()       ✅ AppBrowserClient│    │    ├─ browser()       ❌ nullptr      │
│    ├─ renderer()      ❌ nullptr        │    │    ├─ renderer()      ✅ AppRenderer  │
│    ├─ gpu()           ❌ nullptr        │    │    ├─ gpu()           ❌ nullptr      │
│    └─ utility()       ❌ nullptr        │    │    └─ utility()       ❌ nullptr      │
│                                         │    │                                       │
│  Can: create processes, access files,   │    │  Can: parse HTML, run JS, paint       │
│       manage navigation, open dialogs   │    │  Cannot: access files, create         │
│                                         │    │          processes, open dialogs       │
│                      PRIVILEGED         │    │                      SANDBOXED        │
└─────────────────────────────────────────┘    └───────────────────────────────────────┘
```

### Dispatch During Startup

```cpp
// content/app/content_main_runner_impl.cc

if (process_type.empty()) {
  // Browser → creates browser client ONLY
  browser_client = delegate->CreateContentBrowserClient();

} else if (process_type == switches::kRendererProcess) {
  // Renderer → creates renderer client ONLY
  renderer_client = delegate->CreateContentRendererClient();

} else if (process_type == switches::kGpuProcess) {
  // GPU → creates gpu client ONLY
  gpu_client = delegate->CreateContentGpuClient();
}
```

### Why Not Merge Into ContentClient?

1. **Security** — ContentBrowserClient has privileged APIs (create processes, access files). If on ContentClient, a compromised renderer could theoretically reach them. Separate classes make it structurally impossible — `ContentBrowserClient` is never instantiated in the renderer process.

2. **Clarity** — Each interface is focused on one process's concerns. Merging would create a 200+ method god-object, mostly `nullptr`-checked per process type.

3. **Different capabilities** — browser process is privileged, renderer is sandboxed. They have fundamentally different powers.

### The Sub-Client Accessors Are Internal Plumbing

```cpp
// Inside content/browser/ code — this is how content calls back to embedder:
GetContentClient()->browser()->CreateBrowserMainParts(...);
GetContentClient()->browser()->ShouldUseSpareRenderProcessHost(...);
```

The `browser()` / `renderer()` accessors exist for content's internal dispatch, not as an invitation to merge the classes. They let `//content` reach the right process-specific embedder hook without a separate global for each.

---

## 5. Scale Comparison

| Client | Total Methods | my_app Overrides | Chrome Overrides |
|--------|---------------|------------------|------------------|
| `ContentClient` | ~15 | 6 | ~15 |
| `ContentBrowserClient` | ~100+ | 6 | ~100+ |
| `ContentRendererClient` | ~30 | 2 | ~30 |

my_app overrides only what it needs. Chrome overrides nearly everything.

---

## 6. Summary

```
ContentClient          = "Who am I?" (identity, resources, schemes)
                         Runs everywhere. Read-only. ~15 methods.
                         Does NOT control behavior.

ContentBrowserClient   = "What can the browser do?" (privileged operations)
                         Browser process only. Fat. ~100+ methods.
                         Controls navigation, processes, permissions, networking.

ContentRendererClient  = "How should rendering behave?" (rendering customization)
                         Renderer process only. Medium. ~30 methods.
                         Controls frame creation, error pages, script injection.
```

If it's browser-only → `ContentBrowserClient`.
If it's renderer-only → `ContentRendererClient`.
If it must work in every process → `ContentClient`.
