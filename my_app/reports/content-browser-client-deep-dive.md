# ContentBrowserClient — What It Can Do & Why

## Overview

`ContentBrowserClient` is the **largest embedder interface** in Chromium:
- **3,408 lines** in the header
- **363 virtual methods**
- Runs in the **browser process only** (the privileged, unsandboxed process)

It's the primary mechanism for an embedder to **control** browser behavior — every "should", "can", "allow", "create", "override" decision in the browser process routes through this interface.

### The Class Comment

```cpp
// Embedder API (or SPI) for participating in browser logic, to be implemented
// by the client of the content browser. See ChromeContentBrowserClient for the
// principal implementation. The methods are assumed to be called on the UI
// thread unless otherwise specified. Use this "escape hatch" sparingly, to
// avoid the embedder interface ballooning and becoming very specific to Chrome.
```

### Motivation

`//content` is **generic** — it implements the web platform without knowing about Chrome, extensions, safe browsing, or any product-specific feature. But real browsers need to make **policy decisions** that depend on the product:

- Should this site get its own process? (Chrome says yes for extensions)
- Can this renderer access this file? (Chrome checks download permissions)
- What user agent string to send? (Chrome includes "Chrome/XXX")
- Should this navigation be blocked? (Chrome checks Safe Browsing)

`ContentBrowserClient` is the **escape hatch** — `//content` calls the embedder whenever it needs a product-specific decision.

---

## All Categories (363 methods organized)

### 1. Lifecycle & Startup (8 methods)

**Motivation:** The embedder needs to hook into the browser process lifecycle — initialize its own subsystems, create UI, configure the app.

| Method | Purpose |
|--------|---------|
| `CreateBrowserMainParts(is_integration_test)` | Return BrowserMainParts for startup/shutdown hooks |
| `PostAfterStartupTask(from_here, task_runner, task)` | Post task after startup is complete |
| `IsBrowserStartupComplete()` | Query startup state |
| `OnUiTaskRunnerReady(task_runner)` | UI thread task runner available |
| `SetBrowserStartupIsCompleteForTesting()` | Test helper |
| `IsShuttingDown()` | Query shutdown state |
| `ThreadPoolWillTerminate()` | Thread pool about to shut down |
| `OnWebContentsCreated(web_contents)` | Any WebContents was created |

**my_app uses:** `CreateBrowserMainParts()` → returns `AppBrowserMainParts` which creates the window.

### 2. Process Model (20+ methods)

**Motivation:** Chromium's multi-process architecture is flexible — the embedder decides how many processes to create, when to reuse them, and which sites share processes.

| Method | Purpose |
|--------|---------|
| `ShouldUseProcessPerSite(context, url)` | One process per site? |
| `ShouldTryToUseExistingProcessHost(context, url)` | Reuse existing renderer? |
| `ShouldEmbeddedFramesTryToReuseExistingProcess(rfh)` | Iframes reuse parent process? |
| `ShouldReuseAnyExistingProcessForNewMainFrameSiteInstance(...)` | New tab reuses process? |
| `ShouldAllowProcessPerSiteForMultipleMainFrames(context, url)` | Multiple tabs in one process? |
| `ShouldUseSpareRenderProcessHost(context, url)` | Use pre-spawned spare process? |
| `ShouldAllowNoLongerUsedProcessToExit()` | Kill idle renderers? |
| `IsSuitableHost(process_host, instance)` | Is this process suitable for this site? |
| `MayReuseHost(process_host)` | Can this process be reused? |
| `GetProcessCountToIgnoreForLimit()` | Exclude some processes from limit |
| `GetMaxRendererProcessCountOverride()` | Override max renderer count |
| `SiteInstanceGotProcessAndSite(site_instance)` | Process assigned to SiteInstance |
| `ShouldSwapBrowsingInstancesForNavigation(...)` | Force new BrowsingInstance? |
| `RenderProcessWillLaunch(host)` | Renderer process about to start |
| `BrowserChildProcessHostCreated(host)` | Any child process created |
| `OnRendererProcessLockedStateUpdated(host, locked)` | Process lock state changed |
| `GetEffectiveURL(context, url)` | Map URL to effective URL for process decisions |
| `ShouldCompareEffectiveURLsForSiteInstanceSelection(...)` | Use effective URLs? |

**Example:** Chrome uses `ShouldUseProcessPerSite` to give extensions their own process. my_app uses defaults (content decides).

### 3. Site Isolation & Security (20+ methods)

**Motivation:** Site Isolation is Chromium's core security feature — each site gets its own process so a compromised renderer can't steal cross-site data. The embedder controls isolation policy.

| Method | Purpose |
|--------|---------|
| `DoesSiteRequireDedicatedProcess(context, site_info)` | Must this site be isolated? |
| `ShouldLockProcessToSite(context, site_info)` | Lock renderer to one site? |
| `ShouldEnableStrictSiteIsolation()` | Isolate every site? (desktop: yes) |
| `ShouldDisableSiteIsolation(effective_content_type)` | Disable isolation? (Android: sometimes) |
| `ShouldDisableOriginIsolation()` | Disable origin-level isolation? |
| `GetOverrideValueForOriginKeyedProcesses()` | Override origin-keyed process behavior? |
| `GetAdditionalSiteIsolationModes()` | Extra isolation modes |
| `GetOriginsRequiringDedicatedProcess()` | List of origins needing dedicated process |
| `PersistIsolatedOrigin(context, origin, source)` | Remember isolated origin across sessions |
| `ShouldUrlUseApplicationIsolationLevel(context, url)` | Isolated Web App level? |
| `ShouldAllowCrossProcessSandboxedFrameForPrecursor(...)` | Cross-process sandbox frame? |
| `DoesWebUIUrlRequireProcessLock(url)` | Lock WebUI URLs? |
| `CanCommitURL(process_host, url)` | Can this process commit this URL? |
| `IsFileAccessAllowed(path, absolute_path, profile_path)` | File access permitted? |
| `ForceSniffingFileUrlsForHtml()` | Sniff file:// URLs for HTML? |
| `IsJitDisabledForSite(context, url)` | Disable V8 JIT for this site? |
| `AreV8OptimizationsEnabledForSite(context, url)` | V8 optimizations allowed? |
| `DisallowV8FeatureFlagOverridesForSite(url)` | Block V8 flag overrides? |

**Example:** Chrome isolates all sites on desktop. Android WebView doesn't (too memory-expensive). my_app uses defaults.

### 4. Navigation (15+ methods)

**Motivation:** The embedder needs to intercept, modify, redirect, or block navigations based on product-specific policy (Safe Browsing, enterprise policy, intent handling).

| Method | Purpose |
|--------|---------|
| `CreateThrottlesForNavigation(handle)` | Add NavigationThrottles (block/redirect/defer) |
| `OverrideNavigationParams(context, url, ...)` | Modify navigation parameters |
| `ShouldStayInParentProcessForNTP(url, instance)` | Keep NTP in parent process? |
| `IsExplicitNavigation(transition)` | Is this user-initiated? |
| `AugmentNavigationDownloadPolicy(rfh, user_activation, ...)` | Modify download policy |
| `ShouldSkipBeforeUnloadDialog(rfh)` | Skip beforeunload confirmation? |
| `WillComputeSiteForNavigation(context, navigation_url, ...)` | Pre-compute site for navigation |
| `IsHandledURL(url)` | Does content handle this URL? |
| `ShouldPreconnectNavigation(rfh)` | Preconnect before navigation? |
| `ShouldAllowPrefetchRedirection(rfh, url)` | Allow prefetch redirect? |
| `ShouldDispatchPagehideDuringCommit(rfh, url)` | Dispatch pagehide during commit? |
| `ShouldAnimateBackForwardTransitions()` | Animate back/forward? |

**Example:** Chrome adds Safe Browsing throttles via `CreateThrottlesForNavigation()`. my_app uses defaults.

### 5. URL Loading & Custom Schemes (10+ methods)

**Motivation:** Embedders need custom URL schemes (chrome://, myapp://) and control over how URLs are loaded.

| Method | Purpose |
|--------|---------|
| `CreateNonNetworkNavigationURLLoaderFactory(scheme, id)` | Factory for custom scheme navigations |
| `RegisterNonNetworkSubresourceURLLoaderFactories(...)` | Factory for custom scheme subresources |
| `OverrideURLLoaderFactoryParams(process, origin, params)` | Modify URLLoader params |
| `GetAdditionalWebUISchemes(schemes)` | Register additional WebUI schemes |
| `GetAdditionalViewSourceSchemes(schemes)` | Schemes allowed in view-source: |
| `IsInternalScheme(url)` | Is this an internal scheme? |
| `HasCustomSchemeHandler(context, scheme)` | Custom protocol handler exists? |
| `HasWebRequestAPIProxy(context)` | Extension webRequest API active? |
| `IsWebUIAllowedToMakeNetworkRequests(origin)` | Can this WebUI fetch network? |

**my_app uses:** `CreateNonNetworkNavigationURLLoaderFactory()` and `RegisterNonNetworkSubresourceURLLoaderFactories()` to serve `myapp://` resources.

### 6. Network & Cookies (15+ methods)

**Motivation:** The embedder configures the network stack — proxy settings, cookie policy, SSL, CORS, request modification.

| Method | Purpose |
|--------|---------|
| `ConfigureNetworkContextParams(context, in_memory, relative_partition, params)` | Set proxy, cache, SSL, HSTS |
| `ShouldTreatAsFirstPartyWhenTopLevel(origin, site_for_cookies)` | Override first-party cookie logic |
| `ShouldIgnoreSameSiteCookieRestrictionsWhenTopLevel(origin, trustworthy)` | Ignore SameSite? |
| `DetermineAddressSpaceFromURL(url)` | Override IP address space for URL |
| `OnNetworkServiceDataUseUpdate(frame_id, ...)` | Data usage tracking |
| `ShouldSandboxNetworkService()` | Sandbox the network service? |
| `ShouldRunOutOfProcessSystemDnsResolution()` | Out-of-process DNS? |
| `CanAcceptUntrustedExchangesIfNeeded()` | Accept untrusted signed exchanges? |
| `LocalNetworkAccessRequestPolicyOverride(context, origin)` | Override local network access policy |
| `GrantAdditionalRequestPrivilegesToWorkerProcess(process_id, url)` | Extra worker network rights |
| `ModifyRequestHeadersForPrefetch(headers, context, url)` | Modify prefetch request headers |
| `GetDnsTxtResolverUrlPrefix()` | DNS TXT resolver URL prefix |

### 7. Mojo Interface Binding (5+ methods)

**Motivation:** The embedder exposes custom Mojo interfaces to renderers — this is how native features are accessed from web content.

| Method | Purpose |
|--------|---------|
| `RegisterBrowserInterfaceBindersForFrame(rfh, map)` | Bind frame-scoped interfaces |
| `RegisterBrowserInterfaceBindersForServiceWorker(context, map)` | Bind SW-scoped interfaces |
| `ExposeInterfacesToRenderer(registry, rfh)` | Process-scoped renderer interfaces |
| `BindBrowserControlInterface(pipe)` | Browser control interface |
| `BindGpuHostReceiver(receiver)` | GPU host interface |

**my_app uses:** `RegisterBrowserInterfaceBindersForFrame()` to bind `mojom::NativeApi` — the bridge for clipboard, file dialogs, menus, notifications.

### 8. Service Workers & Workers (8+ methods)

**Motivation:** Service Workers are powerful (intercept all fetches, run in background) — the embedder controls who can register them and what they can do.

| Method | Purpose |
|--------|---------|
| `AllowServiceWorker(scope, site, context, rfh)` | Allow SW registration? |
| `MayDeleteServiceWorkerRegistration(scope, context)` | Allow SW unregistration? |
| `ShouldTryToUpdateServiceWorkerRegistration(scope, context)` | Force SW update? |
| `AllowSharedWorker(url, site, name, origin, context, ...)` | Allow shared worker? |
| `ShouldServiceWorkerInheritPolicyContainerFromCreator(url)` | Inherit CSP? |

### 9. UI & Identity (10+ methods)

**Motivation:** The embedder defines the app's identity — user agent, language, favicon, product name.

| Method | Purpose |
|--------|---------|
| `GetUserAgent()` | User agent string |
| `GetUserAgentMetadata()` | Structured user agent (Client Hints) |
| `GetProduct()` | Product name for UA |
| `GetAcceptLangs(context)` | Accept-Language header |
| `GetApplicationLocale()` | App locale |
| `GetDefaultFavicon()` | Default page icon |
| `GetProductLogo()` | Product logo |
| `GetLoggingFileName(command_line)` | Log file path |

**my_app uses:** `GetUserAgent()` → "MyApp/1.0", `GetAcceptLangs()` → "en-US,en".

### 10. Permissions & Clipboard (10+ methods)

**Motivation:** Enterprise policies and user preferences control what web content can access — clipboard, file picker, drag-and-drop.

| Method | Purpose |
|--------|---------|
| `IsClipboardPasteAllowed(rfh, source, type)` | Allow paste operation? |
| `IsClipboardPasteAllowedByPolicy(source, dest, data, cb)` | Enterprise clipboard policy |
| `IsClipboardCopyAllowedByPolicy(source, type, data, cb)` | Enterprise copy policy |
| `IsDragAllowedByPolicy(source, data)` | Enterprise drag policy |
| `IsTransientActivationRequiredForShowFileOrDirectoryPicker()` | Require user gesture for picker? |
| `IsFileSystemAccessApiFilePickerAllowed(rfh)` | Allow File System Access API? |
| `MaybeOverrideSourceURLForClipboardAccess(context, rfh, url)` | Override clipboard source URL |

### 11. Privacy Sandbox (15+ methods)

**Motivation:** Chrome's Privacy Sandbox APIs (Topics, Attribution Reporting, FLEDGE, Shared Storage) need embedder integration for policy, UI consent, and data management.

| Method | Purpose |
|--------|---------|
| `HandleTopicsWebApi(origin, frame, caller, blocked, ...)` | Handle Topics API call |
| `NumVersionsInTopicsEpochs(rfh)` | Topics epoch configuration |
| `IsSharedStorageAllowed(context, ...)` | Allow Shared Storage access? |
| `IsSharedStorageSelectURLAllowed(context, ...)` | Allow sharedStorage.selectURL? |
| `OnSharedStorageWorkletHostCreated(rfh)` | Worklet created notification |
| `OnSharedStorageSelectURLCalled(rfh)` | selectURL called notification |
| `IsAttributionInternalsWebUIEnabled()` | Enable attribution internals? |
| `AreDeprecatedAutomaticBeaconCredentialsAllowed(context, origin)` | FLEDGE beacon policy |

### 12. Media & PiP (10+ methods)

**Motivation:** Media playback, device enumeration, Picture-in-Picture, and WebRTC need embedder hooks for policy and UI.

| Method | Purpose |
|--------|---------|
| `IsFullscreenAllowedForUnfocusedWebContents(contents)` | Allow fullscreen when unfocused? |
| `MaybeGetScopedPictureInPictureTucker(rfh)` | PiP auto-tuck behavior |
| `GetAutoPipInfo(rfh)` | Auto-PiP configuration |
| `PreferenceRankVideoDeviceInfos(context, infos)` | Rank video devices |
| `PreferenceRankAudioDeviceInfos(context, infos)` | Rank audio devices |
| `GetMediaDeviceIDSalt(context, salt_callback)` | Device ID salt for privacy |
| `StartRtcDiagnosticLogging(rfh, id, ...)` | WebRTC diagnostic logging |
| `IsMultiCaptureAllowed(url)` | Allow multiple screen capture? |
| `GetWideColorGamutHeuristic()` | Wide color gamut detection |

### 13. Back/Forward Cache & Prerender (8+ methods)

**Motivation:** BFCache and prerendering are performance optimizations — the embedder controls which pages are eligible and how they're prioritized.

| Method | Purpose |
|--------|---------|
| `ShouldAllowBackForwardCacheForCacheControlNoStorePage(url)` | BFCache for no-store? |
| `ShouldPrioritizeForBackForwardCache(rfh)` | Prioritize this page for BFCache? |
| `UsePrefetchPrerenderIntegration()` | Integrate prefetch with prerender? |
| `CreatePrerenderWebContentsDelegate(wc)` | Delegate for prerendered pages |
| `CreatePrefetchServiceDelegate(context)` | Prefetch service customization |
| `MaybePrewarmHttpDiskCache(context, navigation_url, ...)` | Pre-warm disk cache? |

### 14. Sandboxing (5+ methods)

**Motivation:** The embedder controls sandbox strictness for different service processes.

| Method | Purpose |
|--------|---------|
| `ShouldSandboxAudioService()` | Sandbox the audio service? |
| `ShouldSandboxNetworkService()` | Sandbox the network service? |
| `SetupEmbedderSandboxParameters(sandbox_type, command_line)` | Configure sandbox params |
| `AllowGpuLaunchRetryOnIOThread()` | Retry GPU launch? |
| `CanShutdownGpuProcessNowOnIOThread()` | Can GPU shut down? |

### 15. WebView / WebUI (10+ methods)

**Motivation:** WebUI pages (chrome:// URLs) and WebView have special security requirements.

| Method | Purpose |
|--------|---------|
| `GetWebContentsViewDelegate(web_contents)` | Platform-specific WebContents view |
| `IsInitialWebUIURL(url)` | Is this the initial WebUI URL? |
| `IsTopChromeWebUIURL(url)` | Is this a top-chrome WebUI? |
| `LogWebUIUsage(webui_or_url)` | Track WebUI usage |
| `IsBuiltinComponent(context, origin)` | Is this a built-in component? |
| `ShouldSendOutermostOriginToRenderer(context, url)` | Send outermost origin? |

### 16. Authentication & Identity (5+ methods)

**Motivation:** WebAuthn, FedCM (Federated Identity), and digital identity need embedder UI and policy.

| Method | Purpose |
|--------|---------|
| `CreateIdentityRequestDialogController(web_contents)` | FedCM dialog UI |
| `CreateDigitalIdentityProvider()` | Digital identity verification |
| `ShouldDisallowCredentialRequest(web_contents)` | Block credential request? |
| `RecordAssistedLogin(type)` | Record assisted login metric |

### 17. AI (3+ methods)

**Motivation:** Chrome's built-in AI features need Mojo bindings.

| Method | Purpose |
|--------|---------|
| `BindAIManager(rfh, receiver)` | Bind AI manager interface |
| `BindTranslationManager(rfh, receiver)` | Bind translation AI |
| `BindLanguageDetectionDriver(rfh, receiver)` | Bind language detection AI |

### 18. Error Handling & Diagnostics (5+ methods)

| Method | Purpose |
|--------|---------|
| `HasErrorPage(http_status_code)` | Custom error page exists? |
| `GetAlternativeErrorPageOverrideInfo(url, rfh, context, code)` | Alternative error page info |
| `ShouldBlockRendererDebugURL(url, context)` | Block chrome://crash, etc.? |
| `CrossProcessSubframeRenderProcessGone(process_id)` | Cross-process iframe crashed |
| `ShouldIsolateErrorPage(in_main_frame)` | Isolate error pages? |

### 19. Bounce Tracking Mitigation (5+ methods)

| Method | Purpose |
|--------|---------|
| `ShouldEnableBtm(context)` | Enable Bounce Tracking Mitigation? |
| `OnBtmServiceCreated(context, service)` | BTM service created |
| `GetBtmRemoveMask()` | What data to remove for bounce trackers |
| `ShouldBtmDeleteInteractionRecords(mask)` | Delete interaction records? |

### 20. Command Line & Build Config (5+ methods)

| Method | Purpose |
|--------|---------|
| `AppendExtraCommandLineSwitches(cmd, child_process_id)` | Add flags to child process |
| `GetApplicationClientGUIDForQuarantineCheck()` | App GUID for download quarantine |
| `GetChildProcessSuffix(flags)` | Child process binary suffix |
| `GetQuarantineConnectionCallback()` | Quarantine service connection |

---

## How Content Calls the Embedder

The pattern is always the same — content has a decision to make and asks the embedder:

```cpp
// Example: Should we use a spare process for this URL?
// content/browser/spare_render_process_host_manager_impl.cc

bool should_use = GetContentClient()->browser()
    ->ShouldUseSpareRenderProcessHost(browser_context, site_url);
```

```cpp
// Example: Create throttles to block/redirect this navigation
// content/browser/renderer_host/navigation_request.cc

auto throttles = GetContentClient()->browser()
    ->CreateThrottlesForNavigation(this);
```

```cpp
// Example: What URLLoaderFactory handles myapp:// navigations?
// content/browser/loader/navigation_url_loader_impl.cc

auto factory = GetContentClient()->browser()
    ->CreateNonNetworkNavigationURLLoaderFactory(scheme, frame_tree_node_id);
```

This happens **hundreds of times** across `content/browser/` — virtually every significant decision point has a `GetContentClient()->browser()->...` call.

---

## What my_app Uses (6 of 363)

| Method | What my_app Does |
|--------|-----------------|
| `CreateBrowserMainParts()` | Returns `AppBrowserMainParts` — creates window, manages lifecycle |
| `GetUserAgent()` | Returns `"MyApp/1.0"` |
| `GetAcceptLangs()` | Returns `"en-US,en"` |
| `RegisterBrowserInterfaceBindersForFrame()` | Binds `mojom::NativeApi` for JS-to-native IPC |
| `CreateNonNetworkNavigationURLLoaderFactory()` | Returns `AppURLLoaderFactory` for `myapp://` |
| `RegisterNonNetworkSubresourceURLLoaderFactories()` | Returns `AppURLLoaderFactory` for `myapp://` |

The other 357 methods use content's defaults — which are designed to be safe and functional without any embedder customization.

---

## What Chrome Uses (nearly all 363)

Chrome (`ChromeContentBrowserClient` at `chrome/browser/chrome_content_browser_client.cc`) is **~8,000 lines** and overrides most methods:

| Category | Chrome Examples |
|----------|----------------|
| Process model | Extensions get dedicated processes |
| Site isolation | Strict isolation on desktop, partial on Android |
| Navigation | Safe Browsing throttles, enterprise policy throttles |
| Permissions | Permission prompts (camera, location, notifications) |
| Network | Proxy from preferences, enterprise certificates |
| Service Workers | Extension SW support |
| Media | Device preference ranking, PiP policy |
| Privacy | Topics, Attribution, Shared Storage integration |
| DevTools | Chrome DevTools delegate |
| AI | Gemini Nano integration |

---

## The Design Philosophy

The comment on the class says it best:

> **"Use this escape hatch sparingly."**

Each method in `ContentBrowserClient` represents a place where `//content` couldn't make a decision on its own — it needed product-specific knowledge. The goal is to keep this interface **as small as possible**, but reality (363 methods) shows how many product-specific decisions a real browser makes.

The alternative would be even worse: hardcoding Chrome-specific logic inside `//content`, which would make it impossible for other embedders (Android WebView, my_app, Electron/CEF) to use the engine.
