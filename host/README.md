# Host Layer

Linux-side windowing and UX integration. The host is the primary
environment. macOS exists only as an execution substrate.

Architecture reference: docs/architecture/ARCHITECTURE_V6.md
  - "Host Responsibilities" section
  - "Host Proxy Contract"
  - "Security and Trust Boundaries" diagram
  - "Performance Targets" table
  - Invariant 1: Linux Is the Primary Environment

## Subsystems

### proxy — Host Proxy Layer
Wayland window management, framebuffer presentation, input forwarding,
hotkey registration, clipboard sync, notification bridge, audio routing,
VM lifecycle coordination.
Performance targets: warm launch <200ms, clipboard sync <50ms,
window resize <16ms, hotkey summon instantaneous.

### wayland — Wayland Integration
Surface management and compositor protocol implementation.

### input — Input Forwarding
Keyboard, mouse, and hotkey event forwarding to guest.

### audio — Audio Routing
Audio stream routing from guest applications to host sound system.

### integration — UX Glue
Clipboard synchronization, notification bridging, file integration,
and native-feeling Linux UX behavior.
