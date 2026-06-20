# Chromium's `//content` Module

## 1. What It Is

`//content` is the **core engine for rendering web pages in a multi-process, sandboxed browser**. It implements the web platform (HTML5, CSS, JS, GPU acceleration, WebGL, WebRTC, Service Workers, etc.) but contains **zero browser-specific features**.

## 2. What's In vs Out

| In `//content` | In `//chrome` (not content) |
|---|---|
| HTML/CSS rendering (via Blink) | Extensions |
| JavaScript (via V8) | Autofill |
| Multi-process architecture | Sync |
| Sandboxing | Safe Browsing |
| Navigation | Translate |
| GPU compositing | Spelling |
| WebRTC, WebGL, WebGPU | Bookmarks, History |
| Service Workers, IndexedDB | Profiles, Settings |
| Mojo IPC between processes | Omnibox |

**Rule:** A feature belongs in `//content` only if it has a spec on chromestatus.com and goes through the Blink feature launch process. Everything else belongs higher up.

## 3. Layer Cake

```
┌──────────────────────────────────┐
│         //chrome                 │  ← Browser features (extensions, autofill, sync...)
├──────────────────────────────────┤
│         //content                │  ← Web platform engine (this module)
├──────────────────────────────────┤
│  //third_party/blink   //v8     │  ← Rendering engine + JS engine
├──────────────────────────────────┤
│  //base  //net  //ui  //mojo    │  ← Low-level libraries
└──────────────────────────────────┘
```

Code can only depend **downward**. `//content` never includes `//chrome`. This is enforced by DEPS rules.

## 4. The Embedder Model

`//content` doesn't know about Chrome. Instead, it defines **embedder interfaces** that higher layers implement:

| Interface | Process | What the Embedder Controls |
|---|---|---|
| `ContentClient` | All | User agent, resources, localized strings, scheme registration |
| `ContentBrowserClient` | Browser | Process model, permissions, network config, DevTools, navigation throttles, Mojo bindings |
| `ContentRendererClient` | Renderer | Renderer startup, frame creation, error pages |
| `ContentGpuClient` | GPU | GPU interface exposure |
| `ContentUtilityClient` | Utility | Service registration |

**Chrome** implements these as `ChromeContentBrowserClient`, etc. **my_app** implements them as `AppBrowserClient`, etc. **Android WebView** has its own implementations. This is how one engine supports multiple products.

### Known Embedders in the Chromium Tree

| Embedder | Directory | Purpose |
|---|---|---|
| Chrome | `//chrome` | The Chromium/Chrome browser |
| Android WebView | `//android_webview` | System WebView on Android |
| Chromecast | `//chromecast` | Cast Web Runtime |
| Cronet | `//components/cronet` | Android network library |
| Fuchsia WebEngine | `//fuchsia_web` | Web rendering on Fuchsia OS |
| Headless | `//headless` | Headless Chromium (automation) |
| content_shell | `//content/shell` | Minimal test embedder |
| **my_app** | `//my_app` | Custom productivity app embedder |

## 5. Multi-Process Architecture

```
Browser Process (privileged)
  │
  ├── Renderer Process 1 (sandboxed) ── renders web pages
  ├── Renderer Process 2 (sandboxed) ── renders web pages
  ├── GPU Process (semi-sandboxed)   ── compositing, WebGL, video decode
  └── Utility Process (sandboxed)    ── file parsing, network service, etc.
```

### Key Classes Per Process

| Browser Process | Renderer Process |
|---|---|
| `RenderProcessHost` (manages a renderer) | `RenderFrame` (one per frame) |
| `RenderFrameHost` (browser-side frame proxy) | `RenderWidget` (compositing) |
| `WebContents` (one tab/page) | `RenderThread` (main thread) |
| `NavigationRequest` (navigation state) | Blink `Document`, `LocalFrame` |
| `BrowserContext` (profile/storage) | |
| `SiteInstance` (security principal) | |

### IPC

Communication between processes uses **Mojo IPC** over platform pipes. Each `RenderFrameHost` (browser) ↔ `RenderFrame` (renderer) pair has Mojo interface channels for navigation, input, compositing, resource loading, etc.

Legacy Chrome IPC (`IPC::Message`) is being phased out in favor of Mojo.

## 6. `//content/public` — The API Surface

Embedders can **only** depend on `//content/public`, never on `//content/browser` or `//content/renderer` directly.

```
content/
├── public/           ← API for embedders (interfaces, enums, structs)
│   ├── app/          ← ContentMain, ContentMainDelegate
│   ├── browser/      ← Browser process APIs (WebContents, BrowserContext, ...)
│   ├── renderer/     ← Renderer process APIs (RenderFrame, ...)
│   ├── common/       ← Shared APIs + mojom interfaces
│   └── test/         ← Test utilities for embedders
├── browser/          ← Internal browser implementation (NOT for embedders)
├── renderer/         ← Internal renderer implementation
├── common/           ← Internal shared code
├── gpu/              ← GPU process internals
├── utility/          ← Utility process internals
└── shell/            ← content_shell (minimal test embedder)
```

### API Design Principles

- `//content/public` contains only interfaces, enums, structs, and (rarely) static functions
- Interfaces that content implements are pure abstract (only one implementation exists inside content)
- Interfaces that embedders implement have default (empty) implementations
- Observer interfaces (`WebContentsObserver`, `RenderFrameObserver`) only have void methods
- Only methods needed by embedders are exposed; internal methods stay in `foo_impl.h`

## 7. Key Public APIs

### Entry Point

| Class/Function | Purpose |
|---|---|
| `content::ContentMain()` | Entry point — starts the multi-process browser |
| `content::ContentMainDelegate` | Embedder's main delegate — creates all client objects, handles startup |
| `content::ContentMainParams` | Parameters for ContentMain (delegate, argc/argv) |

### Browser Process APIs

| Class | Purpose |
|---|---|
| `WebContents` | Represents a web page; the primary embedding surface. Owns the page's frame tree. |
| `WebContentsDelegate` | Embedder receives events: close requests, navigation, fullscreen, dialog requests |
| `WebContentsObserver` | Observe navigation starts/completions, title changes, renderer crashes, DOM events |
| `BrowserContext` | Profile/storage context — cookies, cache, IndexedDB, Service Workers, permissions |
| `RenderFrameHost` | Browser-side proxy for a renderer frame. Used to send messages, query state. |
| `RenderProcessHost` | Manages a renderer child process. Handles process lifecycle, IPC channels. |
| `NavigationHandle` | Represents an in-progress navigation. Provides URL, response headers, SSL info. |
| `NavigationThrottle` | Intercept/block/redirect/defer navigations at various stages |
| `ContentBrowserClient` | ~100+ virtual methods for embedder customization of the browser process |
| `BrowserMainParts` | Lifecycle hooks: PreCreateThreads, PreMainMessageLoopRun, PostMainMessageLoopRun |
| `StoragePartition` | Isolates storage (cache, cookies, etc.) per origin or group of origins |

### Renderer Process APIs

| Class | Purpose |
|---|---|
| `RenderFrame` | Renderer-side frame. Provides access to Blink's `WebLocalFrame`. |
| `RenderFrameObserver` | Observe frame lifecycle: creation, navigation, destruction |
| `ContentRendererClient` | Embedder customization of the renderer process |

### Common APIs

| Class | Purpose |
|---|---|
| `ContentClient` | Shared across all processes — resources, user agent, schemes |
| Various `.mojom` files | Mojo interface definitions for cross-process communication |

## 8. Navigation Flow (Simplified)

```
User types URL or clicks link
  │
  ├─ [Browser] NavigationRequest created
  │    ├─ NavigationThrottles consulted (embedder can block/redirect)
  │    ├─ Network request sent (via network service)
  │    ├─ Response received, security checks
  │    └─ Renderer process selected (new or existing, based on Site Isolation)
  │
  ├─ [Browser → Renderer] CommitNavigation message sent
  │    └─ Contains response body, URL, security info
  │
  └─ [Renderer] Blink loads the document
       ├─ HTML parsed, DOM built
       ├─ Subresources fetched (CSS, JS, images)
       ├─ JavaScript executed
       └─ Page rendered and composited
```

## 9. Site Isolation & Process Model

`//content` enforces **Site Isolation** — each site (scheme + eTLD+1) gets its own renderer process. This prevents one site's renderer from accessing another site's data, even if the renderer is compromised.

Key concepts:
- **SiteInstance** — a group of pages from the same site in the same BrowsingInstance
- **BrowsingInstance** — a group of tabs that can script each other (same window.open chain)
- **SiteInfo** — identifies a site for process allocation decisions

The `ContentBrowserClient` can customize process allocation via `ShouldTryToUseExistingProcessHost()`, `GetProcessCount()`, etc.

## 10. Frame Trees

A `WebContents` owns a tree of frames:

```
WebContents
  └─ FrameTree
       └─ FrameTreeNode (main frame)
            ├─ RenderFrameHost (current document)
            ├─ FrameTreeNode (iframe 1)
            │    └─ RenderFrameHost
            └─ FrameTreeNode (iframe 2)
                 └─ RenderFrameHost (possibly cross-process)
```

Cross-origin iframes can be rendered in separate processes (out-of-process iframes / OOPIF), with `RenderFrameProxyHost` objects acting as placeholders in the parent's process.

## 11. content_shell

`content/shell/` is the **minimal reference embedder** — a bare-bones browser that implements just enough of the embedder interfaces to render pages. Used for:

- Running web platform tests (layout tests / WPT)
- `content_browsertests` integration tests  
- Developer testing of web platform features without building all of Chrome

All its targets are marked `testonly = true`.

## 12. How my_app Uses //content

```
my_app/app/main.cc
  └─ content::ContentMain(AppMainDelegate)        // enters content's main loop

AppContentClient : content::ContentClient          // registers myapp:// scheme
AppBrowserClient : content::ContentBrowserClient   // customizes browser behavior
  ├─ CreateBrowserMainParts()                      // lifecycle hooks
  ├─ RegisterBrowserInterfaceBindersForFrame()      // Mojo NativeApi binding
  ├─ CreateNonNetworkNavigationURLLoaderFactory()   // myapp:// navigation
  └─ RegisterNonNetworkSubresourceURLLoaderFactories() // myapp:// subresources

AppWindow
  ├─ content::WebContents::Create()                // creates a web page
  ├─ content::WebContentsDelegate                  // handles close, console msgs
  └─ content::WebContentsObserver                  // watches title changes

AppBrowserContext : content::BrowserContext         // storage at ~/.config/MyApp
AppRendererClient : content::ContentRendererClient  // renderer process (minimal)
AppBrowserMainParts : content::BrowserMainParts     // startup/shutdown lifecycle
```

my_app uses **13 classes/interfaces** from `//content/public`. Everything else (windowing, menus, clipboard, notifications) comes from `//ui`, which is below `//content` in the dependency stack.

## 13. Key Source Files for Further Reading

| File | Topic |
|---|---|
| `content/README.md` | Module overview, content vs chrome |
| `content/public/README.md` | API design principles |
| `docs/process_model_and_site_isolation.md` | Process model and Site Isolation |
| `docs/navigation.md` | Navigation architecture |
| `docs/frame_trees.md` | Frame tree and MPArch concepts |
| `docs/session_history.md` | Back/forward navigation |
| `docs/render_document.md` | RenderDocument lifecycle |
| `docs/supported_platforms.md` | All official embedders |
| `docs/transcripts/wuwt-e03-content.md` | "What's Up With //content" interview transcript |
| `content/shell/` | Minimal embedder reference implementation |
