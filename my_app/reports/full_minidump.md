# Full Minidump Generation

## Overview

Crashpad normally generates `MiniDumpNormal` dumps — thread stacks, 256 bytes around the faulting IP, loaded modules, system info, and exception info only. This report describes how to enable full process memory dumps.

## Architecture

### Entry Point: `MinidumpFileWriter::InitializeFromSnapshot()`

**File:** `third_party/crashpad/crashpad/minidump/minidump_file_writer.cc:181-198`

After the normal memory list is populated (thread stacks, extra memory, exception memory), the method iterates `ProcessSnapshot::MemoryMap()`:

```cpp
const ProcessMemory* process_memory = process_snapshot->Memory();
auto memory_map = process_snapshot->MemoryMap();
for (const auto& region : memory_map) {
  const MINIDUMP_MEMORY_INFO& info = region->AsMinidumpMemoryInfo();
  if (info.State == MEM_COMMIT &&
      info.Protect != PAGE_NOACCESS &&
      !(info.Protect & PAGE_GUARD) &&
      info.RegionSize <= 512 * 1024 * 1024) {
    auto mem_snapshot = std::make_unique<internal::MemorySnapshotGeneric>();
    mem_snapshot->Initialize(process_memory, info.BaseAddress, info.RegionSize);
    full_memory_snapshots_.push_back(std::move(mem_snapshot));
    memory_list->AddMemory(
        std::make_unique<SnapshotMinidumpMemoryWriter>(mem_snapshot_ptr));
  }
}
```

Each readable, committed memory region is wrapped in a `MemorySnapshotGeneric` (which lazily reads via `ProcessMemory::Read()` on write) and appended to the `MINIDUMP_MEMORY_LIST` stream.

### Platform Memory Map Providers

| Platform | `MemoryMap()` implementation | Status |
|---|---|---|
| Linux | `ProcessSnapshotLinux::MemoryMap()` — reads `/proc/pid/maps` via `ProcessReaderLinux` | ✅ Working |
| Windows | `ProcessSnapshotWin::MemoryMap()` — uses `VirtualQueryEx` | ✅ Working |
| Fuchsia | `ProcessSnapshotFuchsia::MemoryMap()` — uses `zx_object_get_info` | ✅ Working |
| macOS | `ProcessSnapshotMac::MemoryMap()` — returns empty (stub) | ❌ Not implemented |
| iOS | `ProcessSnapshotIOS::MemoryMap()` — returns empty (stub) | ❌ Not implemented |

### Linux Implementation Details

**Files:**
- `snapshot/linux/memory_map_region_snapshot_linux.h` — `MemoryMapRegionSnapshotLinux` class
- `snapshot/linux/memory_map_region_snapshot_linux.cc` — converts `MemoryMap::Mapping` to `MINIDUMP_MEMORY_INFO`

Mappings are filtered to:
- `readable == true` — only regions that can be read
- `range.Size() <= 512MB` — skip impractically large or special regions

### Filters Applied

| Filter | Reason |
|---|---|
| `State == MEM_COMMIT` | Only committed memory has valid content |
| `Protect != PAGE_NOACCESS` | No-access regions cannot be read |
| `!(Protect & PAGE_GUARD)` | Guard pages trigger faults on access |
| `RegionSize <= 512MB` | Prevent oversized single regions (graphics memory, sparse mappings) |

If a region becomes inaccessible between map capture and read (e.g. `pread64 I/O error`), `SnapshotMinidumpMemoryWriter::WriteObject()` fills it with `0xfe` bytes.

### Ownership

`MinidumpFileWriter::full_memory_snapshots_` (`std::vector<std::unique_ptr<const MemorySnapshot>>`) owns the `MemorySnapshotGeneric` objects. They must outlive the `SnapshotMinidumpMemoryWriter` objects that hold raw pointers to them.

## Configuration

To enable crash reporting and full dumps:

```
--enable-crash-reporter --crash-dumps-dir=<path> [--no-sandbox]
```

The `--crash-dumps-dir` flag sets the database directory. The dump appears in `pending/<uuid>.dmp`.

## Validation

Tested with `my_app` on Linux x86_64:

| Metric | Normal Dump | Full Dump |
|---|---|---|
| File size | ~300 KB – 5 MB | ~345 MB (browser process) |
| Contents | Stacks + 256B + modules | All committed memory |
| Generation time | < 1 second | Several seconds |
