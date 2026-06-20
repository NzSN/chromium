# Chromium `//content` — Comprehensive Overview

## A. Internal Structure

### Top-Level Directories

```
content/
├── public/          ← API for embedders (interfaces, enums, structs only)
│   ├── app/         ← ContentMain, ContentMainDelegate
│   ├── browser/     ← Browser process APIs (WebContents, BrowserContext, ...)
│   ├── renderer/    ← Renderer process APIs (RenderFrame, ...)
│   ├── common/      ← Shared APIs + mojom interfaces
│   ├── gpu/         ← ContentGpuClient
│   ├── utility/     ← ContentUtilityClient
│   └── test/        ← Test utilities for embedders
│
├── browser/         ← Browser process internals (404 entries — largest subsystem)
│   ├── renderer_host/         Frame tree, RFH, navigation, compositing, input
│   ├── web_contents/          WebContentsImpl — the central hub
│   ├── service_worker/        Service Worker lifecycle (163 files)
│   ├── storage_partition_impl Storage isolation per profile/partition
│   ├── network/               Network service integration
│   ├── gpu/                   GPU process management
│   ├── devtools/              DevTools protocol implementation
│   ├── accessibility/         a11y tree management
│   ├── back_forward_cache/    BFCache implementation
│   ├── tracing/               Tracing/perfetto integration
│   ├── bluetooth/             Web Bluetooth
│   ├── cache_storage/         Cache API
│   ├── indexed_db/            IndexedDB backend
│   └── ... (100+ more subdirectories)
│
├── renderer/        ← Renderer process internals (115 entries)
│   ├── accessibility/         Renderer-side a11y
│   ├── media/                 Media rendering
│   └── ... (agent scheduling, security policy utils, etc.)
│
├── common/          ← Shared between browser + renderer (104 entries)
│   ├── *.mojom                Mojo interface definitions for internal IPC
│   ├── *_param_traits.*       IPC serialization traits
│   └── url_schemes.*          URL scheme registration
│
├── gpu/             ← GPU process entry + client
├── utility/         ← Utility process entry + client
├── child/           ← Shared child process code (renderer, GPU, utility)
├── services/        ← Internal content services
├── shell/           ← content_shell (minimal test embedder)
├── test/            ← Test infrastructure
├── web_test/        ← Web platform test runner
├── zygote/          ← Linux zygote (fork server for child processes)
└── app/             ← ContentMain entry point + main runner
```

### Key Classes

| Class | File | Lines | Role |
|-------|------|-------|------|
| `WebContentsImpl` | `browser/web_contents/web_contents_impl.h` | ~9000 | Central hub; implements 14 delegate interfaces |
| `RenderFrameHostImpl` | `browser/renderer_host/render_frame_host_impl.h` | ~5650 | Browser-side document/frame; implements 21 interfaces |
| `RenderProcessHostImpl` | `browser/renderer_host/render_process_host_impl.h` | ~3000 | Manages one renderer process |
| `NavigationRequest` | `browser/renderer_host/navigation_request.h` | ~4000 | Represents one navigation attempt |
| `StoragePartitionImpl` | `browser/storage_partition_impl.h` | ~1000 | Per-partition storage isolation |
| `ChildProcessSecurityPolicyImpl` | `browser/child_process_security_policy_impl.h` | ~800 | Per-process security state (singleton) |
| `FrameTree` | `browser/renderer_host/frame_tree.h` | ~500 | Tree of frames in a WebContents |
| `FrameTreeNode` | `browser/renderer_host/frame_tree_node.h` | ~600 | One node in the frame tree |

### `content/browser/renderer_host/` Layering

This directory is the heart of `//content`. Key rule: **code here cannot call up to `WebContents`** except through delegate interfaces (`RenderFrameHostDelegate`, `RenderViewHostDelegate`, etc.). This keeps the frame/navigation layer decoupled from the tab/page layer.

```
WebContentsImpl
  │ implements RenderFrameHostDelegate, NavigatorDelegate, etc.
  │
  ▼ (delegate pattern — renderer_host/ calls UP through delegates)
FrameTree ← FrameTreeNode ← RenderFrameHostManager
  │                              │
  ├─ RenderFrameHostImpl         ├─ NavigationRequest
  ├─ RenderFrameProxyHost        └─ NavigationController
  ├─ RenderViewHostImpl               └─ NavigationEntry
  └─ RenderWidgetHostImpl
       └─ RenderWidgetHostViewBase
```

---

## B. Navigation System

### Life of a Navigation (end-to-end)

```
1. TRIGGER
   User types URL / clicks link / JS calls location.href
     │
2. BEFORE UNLOAD
   ├─ Browser asks renderer: "run beforeunload handler"
   ├─ User can cancel (if handler calls event.preventDefault())
   └─ If cancelled → navigation aborted
     │
3. NAVIGATION START
   ├─ NavigationRequest created
   ├─ WebContentsObserver::DidStartNavigation() fires
   ├─ NavigationThrottles consulted:
   │   ├─ WillStartRequest() → PROCEED / CANCEL / DEFER / BLOCK
   │   ├─ [redirects] WillRedirectRequest()
   │   └─ WillProcessResponse()
   │
4. NETWORK REQUEST
   ├─ URL fetched via network service (or non-network factory for custom schemes)
   ├─ Redirects followed (WillRedirectRequest throttle for each)
   ├─ Response received:
   │   ├─ 204/205 → no document created, navigation cancelled
   │   ├─ Content-Disposition: attachment → download, no navigation
   │   └─ Normal response → proceed to commit
   │
5. PROCESS SELECTION
   ├─ SiteInstance determined for the response URL
   ├─ Renderer process selected (new or existing)
   │   ├─ Same-site: reuse current process
   │   └─ Cross-site: swap to new process (OOPIF or full swap)
   ├─ Speculative RenderFrameHost may be created
   │
6. COMMIT
   ├─ Browser sends CommitNavigation to renderer
   ├─ Renderer loads the response, creates document
   ├─ Renderer sends DidCommitNavigation back to browser
   ├─ Security state updated (origin, permissions)
   ├─ Session history entry created (NavigationEntry)
   ├─ WebContentsObserver::DidFinishNavigation() fires
   │
7. LOADING
   ├─ HTML parsed, DOM built
   ├─ Subresources fetched (CSS, JS, images, fonts)
   ├─ JavaScript executed
   ├─ DOMContentLoaded event → WebContentsObserver::DOMContentLoaded()
   ├─ load event → WebContentsObserver::DidFinishLoad()
   └─ DidStopLoading() — all frames done
```

### WebContentsObserver Navigation Hooks

| Hook | When |
|------|------|
| `DidStartNavigation(NavigationHandle*)` | Navigation begins (before network request) |
| `DidRedirectNavigation(NavigationHandle*)` | HTTP redirect received |
| `ReadyToCommitNavigation(NavigationHandle*)` | Response received, about to commit |
| `DidFinishNavigation(NavigationHandle*)` | Navigation committed or failed |
| `DidStartLoading()` | First frame starts loading |
| `DOMContentLoaded(RenderFrameHost*)` | DOM parsed (DOMContentLoaded event) |
| `DidFinishLoad(RenderFrameHost*, GURL)` | Frame finished loading (load event) |
| `DidStopLoading()` | All frames finished loading |
| `DidFailLoad(RenderFrameHost*, GURL, int error)` | Frame load failed |

### NavigationThrottle

Embedders can intercept navigations by registering `NavigationThrottle` subclasses:

```cpp
class MyThrottle : public content::NavigationThrottle {
  ThrottleCheckResult WillStartRequest() override;     // before network request
  ThrottleCheckResult WillRedirectRequest() override;  // on each redirect
  ThrottleCheckResult WillProcessResponse() override;  // after response headers
  ThrottleCheckResult WillCommitWithoutUrlLoader() override; // non-network navigations
};

// Return values:
//   PROCEED    — allow navigation
//   CANCEL     — cancel silently
//   BLOCK      — cancel and show error
//   DEFER      — pause; call Resume() later
```

### Session History

```
NavigationController (one per WebContents)
  └─ NavigationEntry list [0..N]     ← back/forward history
       └─ FrameNavigationEntry       ← per-frame URL/state
            └─ SiteInstance           ← security context
```

---

## C. Rendering Pipeline

### Overview: From HTML to Pixels

```
HTML/CSS/JS → Blink → cc (compositor) → viz → GPU → Display

[Renderer Process]                    [GPU Process / Browser Process]
┌─────────────────────┐              ┌──────────────────────────┐
│ Blink (main thread) │              │ Viz Display Compositor   │
│  ├─ Parse HTML/CSS  │              │  ├─ Aggregates frames    │
│  ├─ Layout          │              │  ├─ Damage tracking      │
│  ├─ Paint           │              │  └─ Outputs to display   │
│  └─ Produce layers  │              │                          │
│         │           │              │ GPU rasterization        │
│    ┌────▼────┐      │              │  ├─ Skia/Ganesh          │
│    │ cc      │      │  compositor  │  ├─ OpenGL / Vulkan      │
│    │ (impl   │──────│──frames────►─│  └─ Dawn (WebGPU)        │
│    │ thread) │      │  via viz     │                          │
│    └─────────┘      │              └──────────────────────────┘
└─────────────────────┘
```

### Stage 1: Blink (Main Thread)

1. **Parse** — HTML → DOM tree, CSS → style rules
2. **Style** — Compute styles for each DOM node
3. **Layout** — Calculate geometry (position, size) for each element
4. **Pre-paint** — Build paint property trees (transforms, clips, effects)
5. **Paint** — Generate paint operations (`cc::DisplayItemList`) for each layer
6. **Commit** — Send layer tree to cc compositor thread (blocks main thread briefly)

### Stage 2: cc Compositor (Compositor Thread)

`cc/` = "content collator" — embedded in both renderer and browser processes.

1. **Tiling** — Break layers into tiles for incremental rasterization
2. **Rasterization** — Convert paint ops to GPU textures (via Skia on GPU process)
3. **Animation** — Handle scroll, pinch, CSS animations without touching main thread
4. **Draw** — Produce a `CompositorFrame` (quads referencing GPU textures)
5. **Submit** — Send frame to viz via `CompositorFrameSink` Mojo interface

```
cc::Layer (main thread)         ←→    cc::LayerImpl (compositor thread)
     │                                     │
     │ Commit (copy data)                  │ Draw
     │                                     ▼
     │                           CompositorFrame
     │                                     │
     └─────────────────────────────────────▼
                                    viz service
```

### Stage 3: Viz (Display Compositor)

`components/viz/` — composites frames from all sources (renderer, browser UI, video).

- **Host** (browser process) — privileged, manages frame sink hierarchy
- **Client** (renderer, browser UI) — submits compositor frames
- **Service** (GPU process) — aggregates frames, outputs to display

```
Renderer 1 ──CompositorFrame──►┐
Renderer 2 ──CompositorFrame──►├─► Viz SurfaceAggregator ──► Display ──► Screen
Browser UI ──CompositorFrame──►┘
```

### Stage 4: GPU (Rasterization + Output)

- **Skia/Ganesh** — 2D rasterization (paint ops → textures)
- **OpenGL / Vulkan / Dawn** — GPU API backends
- **Output** — SwapBuffers to window system (X11, Wayland, Win32, Cocoa)

### Threading Model

| Thread | Process | Role |
|--------|---------|------|
| Main thread | Renderer | Blink: DOM, layout, paint, JS |
| Compositor thread | Renderer | cc: tiling, animation, frame submission |
| Main thread | Browser | UI events, navigation, IPC |
| IO thread | Browser/Renderer | Mojo IPC message dispatch |
| GPU main thread | GPU | Command buffer processing, Skia rasterization |
| Display compositor thread | GPU | Viz aggregation + display output |

---

## D. Mojo IPC

### Core Concepts

```
Process A                              Process B
┌─────────────┐    message pipe    ┌─────────────┐
│ Remote<Foo> │◄═══════════════════│ Receiver<Foo>│
│ (sender)    │    typed messages   │ (impl)      │
└─────────────┘                    └─────────────┘
```

- **mojom** files define interfaces with methods, structs, enums
- **Remote\<T\>** — send-side endpoint; calls methods asynchronously
- **Receiver\<T\>** — receive-side endpoint; dispatches to implementation
- **PendingRemote / PendingReceiver** — unbound endpoints passed across processes
- Messages dispatched as **scheduled tasks** on the bound sequence (not inline)

### mojom Example

```mojom
// content/common/frame.mojom
interface FrameHost {
  DidCommitNavigation(DidCommitParams params) => ();
  CreateChildFrame(int32 child_routing_id, string frame_name);
};
```

Generated code provides:
- `mojom::FrameHost` — abstract C++ interface
- `mojom::FrameHostProxy` — Remote-side stub
- Serialization/deserialization for all parameters
- Optional JS bindings (`*.mojom-webui.js`)

### Key Mojo Interfaces in //content

| Interface | Direction | Purpose |
|-----------|-----------|---------|
| `mojom::FrameHost` | Renderer → Browser | Frame lifecycle events (commit, child creation) |
| `mojom::LocalFrame` | Browser → Renderer | Frame commands (load URL, stop, execute script) |
| `mojom::LocalMainFrameHost` | Renderer → Browser | Main frame events (close, focus, context menu) |
| `blink::mojom::RendererHost` | Renderer → Browser | Renderer-level events |
| `mojom::NavigationClient` | Both | Navigation commit protocol |
| `viz::mojom::CompositorFrameSink` | Renderer → GPU | Submit compositor frames |
| `network::mojom::URLLoaderFactory` | Browser/Renderer | Create URL loaders for fetching |
| `network::mojom::NetworkContext` | Browser only | Network configuration (cookies, proxy, SSL) |

### Interface Binding Patterns

**1. Frame-scoped (per RenderFrameHost):**
```cpp
// Browser side — bind when renderer requests
void ContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    RenderFrameHost* rfh,
    mojo::BinderMapWithContext<RenderFrameHost*>* map) {
  map->Add<mojom::NativeApi>(base::BindRepeating(&NativeApiImpl::Create));
}
```

**2. Process-scoped (per RenderProcessHost):**
```cpp
void ContentBrowserClient::ExposeInterfacesToRenderer(
    BrowserInterfaceBrokerRegistry* registry, ...) {
  registry->ForProcess()->Add<mojom::SomeService>(...);
}
```

**3. Associated interfaces (ordered with existing channel):**
Used when message ordering relative to navigation/frame messages matters.

---

## E. Network Service

### Architecture

```
Browser Process                          Utility Process (or in-process)
┌───────────────────┐                   ┌───────────────────────┐
│ StoragePartition  │                   │ Network Service       │
│  └─ NetworkContext│──Mojo──────────►──│  └─ NetworkContext     │
│     (trusted)     │                   │     ├─ URLLoaderFactory│
│                   │                   │     ├─ CookieManager   │
│ RenderProcessHost │                   │     ├─ HostResolver    │
│  └─ URLLoader     │──Mojo──────────►──│     ├─ WebSocket       │
│     Factory       │                   │     ├─ WebTransport    │
│     (restricted)  │                   │     └─ SSLConfig       │
└───────────────────┘                   └───────────────────────┘
```

### Key Principles

- **Feature-oblivious** — implements HTTP, DNS, sockets, TLS only. No Safe Browsing, no autofill, no Chrome features.
- **Trusted interfaces** — `NetworkContext` and `NetworkService` should only be accessed by the browser process. Renderers get restricted `URLLoaderFactory` instances.
- **Out-of-process** (default on desktop) — runs on the IO thread of a utility process. If it crashes, it's restarted automatically.
- **In-process** on Android — for performance reasons.

### Key Classes

| Class | Location | Purpose |
|-------|----------|---------|
| `NetworkService` | `services/network/` | Singleton service; creates NetworkContexts |
| `NetworkContext` | `services/network/` | Per-profile network stack (cookies, cache, proxy) |
| `URLLoader` | `services/network/` | One network request |
| `URLLoaderFactory` | `services/network/` | Creates URLLoaders; security boundary |
| `CookieManager` | `services/network/` | Cookie CRUD operations |
| `HostResolver` | `services/network/` | DNS resolution |

### Request Flow

```
Renderer (Blink)
  └─ blink::ResourceLoader
       └─ URLLoaderFactory::CreateLoader(request)        [Mojo to browser/network]
            │
            ├─ Browser intercepts? (NavigationThrottle, etc.)
            │
            └─ Network Service
                 ├─ DNS resolve (HostResolver)
                 ├─ TCP/TLS connect
                 ├─ HTTP request
                 ├─ CORS check (if cross-origin)
                 ├─ CORB/ORB check (block sensitive responses)
                 ├─ Response headers → URLLoaderClient::OnReceiveResponse
                 └─ Response body → data pipe → renderer
```

### Non-Network URL Loaders

Custom schemes (like `myapp://`, `chrome://`, `file://`) use **non-network URLLoaderFactories** registered by the embedder:

```cpp
ContentBrowserClient::CreateNonNetworkNavigationURLLoaderFactory("myapp")
ContentBrowserClient::RegisterNonNetworkSubresourceURLLoaderFactories(...)
```

---

## F. Storage Subsystems

### StoragePartition

The isolation boundary for all per-profile storage. Each `BrowserContext` has at least one `StoragePartition`.

```
StoragePartition
  ├─ NetworkContext          ← cookies, HTTP cache, SSL state
  ├─ DOMStorageContext       ← localStorage, sessionStorage
  ├─ IndexedDBControl       ← IndexedDB databases
  ├─ CacheStorageControl    ← Cache API (Service Worker caches)
  ├─ FileSystemContext      ← File System Access API
  ├─ ServiceWorkerContext   ← Service Worker registration/lifecycle
  ├─ BackgroundSyncContext  ← Background Sync API
  ├─ QuotaManager           ← Storage quota management
  ├─ SharedStorageManager   ← Shared Storage API (Privacy Sandbox)
  ├─ LocalStorageControl    ← Local Storage implementation
  └─ PlatformNotificationContext ← Web Notifications
```

### Service Workers

Service Workers provide offline capability, push notifications, and background sync.

**Key files:** `content/browser/service_worker/` (163 files)

```
Registration Flow:
  JS: navigator.serviceWorker.register('/sw.js')
    → Browser: ServiceWorkerContextWrapper::RegisterServiceWorker()
    → Fetch /sw.js script
    → Install event
    → Activate event
    → Service Worker controls pages

Fetch Interception:
  Page fetches URL → ServiceWorkerContainerHost intercepts
    → Dispatches "fetch" event to Service Worker
    → SW can respond from cache, network, or synthesized response
```

**Key classes:**
| Class | Role |
|-------|------|
| `ServiceWorkerContextWrapper` | Browser-side API for SW management |
| `ServiceWorkerVersion` | One version of a registered SW script |
| `ServiceWorkerRegistration` | Registration (scope + versions) |
| `EmbeddedWorkerInstance` | Manages the worker process/thread |
| `ServiceWorkerDatabase` | SQLite storage for registrations |

### IndexedDB

Client-side structured storage (NoSQL database).

- Runs in the browser process (storage backend)
- Renderer accesses via Mojo IPC
- Data stored in LevelDB per-origin
- Quota managed by `QuotaManager`

### Cache Storage (Cache API)

Used by Service Workers to cache request/response pairs.

- `CacheStorageManager` — per-partition cache management
- `CacheStorageCache` — one named cache
- Stored on disk, quota-managed
- Accessed via `caches.open()` / `caches.match()` in JS

---

## G. Security Model

### Multi-Layer Defense

```
Layer 1: Process Isolation (sandbox)
  └─ Renderer processes run in a restrictive sandbox
     └─ No direct file system, network, or device access

Layer 2: Site Isolation
  └─ Each site gets its own renderer process
     └─ Compromised renderer can't access other sites' data

Layer 3: Browser-Enforced Checks
  └─ ChildProcessSecurityPolicy tracks per-process permissions
     └─ "Jail" checks: locked process can only access its site
     └─ "Citadel" checks: unlocked process can't access sensitive data

Layer 4: Network Response Filtering
  └─ CORB/ORB blocks sensitive cross-site responses from reaching renderer
     └─ Even if renderer is compromised, it never sees the data
```

### Sandboxing

| Platform | Sandbox Technology |
|----------|-------------------|
| Linux | Seccomp-BPF + namespaces (via Zygote fork) |
| Windows | Restricted tokens + job objects + integrity levels |
| macOS | Seatbelt (sandbox profiles) |
| Android | SELinux + seccomp |

Renderer processes are the most restrictively sandboxed. GPU and utility processes have less restrictive sandboxes.

### Site Isolation

**Goal:** Prevent a compromised renderer from accessing cross-site data.

**Key abstractions:**
| Concept | Purpose |
|---------|---------|
| `SiteInfo` | Security principal — typically scheme + eTLD+1 |
| `SiteInstance` | Instance of a principal in a browsing context group |
| `SiteInstanceGroup` | Group of SiteInstances sharing a process |
| `BrowsingInstance` | Group of tabs that can script each other |
| `ProcessLock` | Restricts a renderer process to one site |

**Cross-origin iframes:** Rendered in separate processes (OOPIF — Out-of-Process iFrames) with `RenderFrameProxyHost` as placeholder in parent's process.

```
Tab: example.com
  └─ main frame (process A, locked to example.com)
       ├─ <iframe src="ads.com"> (process B, locked to ads.com)
       │    └─ RenderFrameProxyHost in process A
       └─ <iframe src="example.com/page"> (process A, same site)
```

### ChildProcessSecurityPolicy

Singleton that tracks what each child process is allowed to do:

```cpp
// Browser checks before honoring renderer requests:
ChildProcessSecurityPolicy::GetInstance()
  ->CanAccessDataForOrigin(process_id, origin)    // Can this process see this origin's data?
  ->CanReadFile(process_id, file_path)             // Can this process read this file?
  ->CanCommitURL(process_id, url)                  // Can this process commit this URL?
  ->HasWebUIBindings(process_id)                   // Does this process have chrome:// privileges?
```

### CORS / CORB / ORB

| Check | Where | Purpose |
|-------|-------|---------|
| **CORS** | Network service | Cross-Origin Resource Sharing — server opt-in for cross-origin access |
| **CORB** | Network service | Cross-Origin Read Blocking — blocks HTML/JSON/XML from cross-origin subresource loads |
| **ORB** | Network service | Opaque Response Blocking — successor to CORB, broader protection |

These checks run in the network service, before response bodies reach the renderer. Even a compromised renderer never receives the blocked data.

---

## H. Complete Data Flow Example

A page load from start to rendered pixels, showing all subsystems interacting:

```
1. User types "https://example.com" in omnibox

2. [Browser] NavigationRequest created
   └─ NavigationThrottles consulted (embedder can block)

3. [Browser → Network Service] URLLoader created
   └─ DNS resolve → TCP connect → TLS handshake → HTTP GET /
   └─ CORS/CORB checks on response

4. [Browser] Process selection
   └─ SiteInstance for example.com
   └─ ChildProcessSecurityPolicy: lock renderer to example.com

5. [Browser → Renderer] CommitNavigation (Mojo)
   └─ Response headers + body data pipe

6. [Renderer: Blink main thread]
   ├─ Parse HTML → DOM tree
   ├─ Parse CSS → style rules
   ├─ Compute styles → layout tree
   ├─ Layout → geometry (position, size)
   ├─ Paint → display item lists (paint ops)
   └─ Commit to compositor thread

7. [Renderer: cc compositor thread]
   ├─ Tile layers
   ├─ Rasterize tiles (Skia → GPU textures)
   ├─ Build CompositorFrame (quads + texture refs)
   └─ Submit to viz (Mojo CompositorFrameSink)

8. [GPU: viz display compositor]
   ├─ Aggregate frames (renderer + browser UI)
   ├─ Draw final output
   └─ SwapBuffers → display

9. [Ongoing]
   ├─ User scrolls → cc handles on compositor thread (no Blink)
   ├─ JS modifies DOM → Blink re-layouts → cc re-rasterizes
   ├─ Service Worker intercepts subresource fetches
   └─ IndexedDB/Cache API for offline data
```

---

## Source References

| Topic | Key Files |
|-------|-----------|
| Navigation | `docs/navigation.md`, `content/browser/renderer_host/navigation_request.h` |
| Process Model | `docs/process_model_and_site_isolation.md`, `content/browser/renderer_host/render_process_host_impl.h` |
| Frame Trees | `docs/frame_trees.md`, `content/browser/renderer_host/frame_tree.h` |
| WebContents | `content/browser/web_contents/web_contents_impl.h` |
| RenderFrameHost | `content/browser/renderer_host/render_frame_host_impl.h` |
| Compositor (cc) | `docs/how_cc_works.md`, `cc/README.md` |
| Viz | `components/viz/README.md` |
| Network Service | `services/network/README.md` |
| Mojo IPC | `docs/mojo_and_services.md` |
| Storage | `content/public/browser/storage_partition.h` |
| Service Workers | `content/browser/service_worker/` |
| Security | `content/browser/child_process_security_policy_impl.h` |
| Blink Public API | `third_party/blink/public/README.md` |
| Sandbox | `docs/design/sandbox.md` |
| Session History | `docs/session_history.md` |
| Content API | `content/public/README.md` |
| renderer_host | `content/browser/renderer_host/README.md` |
