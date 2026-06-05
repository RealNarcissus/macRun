# Module Ownership

This document maps architecture components to repository directories and
defines subsystem ownership boundaries.

Architecture reference: docs/architecture/ARCHITECTURE_V6.md
  - "Component Responsibilities" diagram
  - "Interface Contracts" section
  - "Security and Trust Boundaries" diagram

## Ownership Map

```
Architecture Component       Repository Directory        Trust Boundary
──────────────────────────────────────────────────────────────────────────
macrun Launcher              platform/macrun/            Linux Host (Trusted)
Capability Detection         platform/detector/          Linux Host (Trusted)
compat-db                    compat-db/                  Linux Host (Trusted)
Darling Runtime              runtime/darling/            Compatibility Boundary
Runtime Shims                runtime/shims/              Compatibility Boundary
Cocoa Compatibility          runtime/cocoa/              Compatibility Boundary
ARM64 Translation            runtime/arm64/              Compatibility Boundary
Host Proxy                   host/proxy/                 Linux Host (Trusted)
Wayland Backend              host/wayland/               Linux Host (Trusted)
Input Forwarding             host/input/                 Linux Host (Trusted)
Audio Routing                host/audio/                 Linux Host (Trusted)
UX Integration               host/integration/           Linux Host (Trusted)
Bridge Daemon                vm/bridge/                  macOS Guest VM
Frame Capture                vm/capture/                 macOS Guest VM
Frame Encode                 vm/encode/                  macOS Guest VM
VM Lifecycle                 vm/lifecycle/               Linux Host (Trusted)
VirtIO Transport             vm/virtio/                  Host ↔ Guest
Tooling                      tooling/                    Linux Host (Trusted)
Tests                        tests/                      N/A
```

## Trust Boundaries

The architecture defines three trust domains:

### Linux Host (Trusted)
- platform/, host/, compat-db/, tooling/
- Primary operating environment
- Owns execution policy, UX integration, and detection

### Compatibility Runtime Boundary
- runtime/
- macOS application execution via compatibility layers
- Communicates with host through explicit IPC/streams

### macOS Guest VM
- vm/bridge/, vm/capture/, vm/encode/
- macOS internal execution
- Communicates with host through VirtIO and shared filesystems

## Cross-Boundary Communication

No subsystem may directly depend on another subsystem's internals.

Allowed interactions:
- platform ↔ compat-db: Detection signature queries (read-only)
- platform ↔ runtime: Execution backend selection
- host ↔ vm: Frame transport, input forwarding, clipboard sync (protocol-defined)
- host ↔ runtime: Wayland surface integration (protocol-defined)

Disallowed interactions:
- No subsystem reads another subsystem's internal state directly
- No implicit execution assumptions
- No shared mutable state across trust boundaries
- No undocumented internal behavior dependencies

## Ownership Rules

1. Each directory has a single owning subsystem
2. Cross-subsystem changes require interface contract review
3. Architecture invariants (ARCHITECTURE_V6.md) override all other conventions
4. Violating trust boundaries requires explicit architectural review
