# VM Layer

macOS guest integration for Tier 4B VM-assisted execution.
Per-window capture and streaming, bridge daemon, lifecycle management.

Architecture reference: docs/architecture/ARCHITECTURE_V6.md
  - "VM-Assisted Architecture" section
  - "VM Streaming overview" diagram
  - "Bridge Daemon Contract"
  - "Host Responsibilities" section
  - "Guest Responsibilities" section

## Subsystems

### bridge — macOS Guest Bridge Daemon
Window enumeration, framebuffer capture, event injection,
notification forwarding, accessibility permission management.
Never manages Linux compositor state or owns execution policy.

### capture — Per-Window Framebuffer Capture
CGWindow capture on the guest side, avoiding full desktop streaming.
Enables Linux-native window management and compositor integration.

### encode — Frame Encoding
Encodes captured framebuffers for VirtIO transport to the host.

### lifecycle — VM Lifecycle Management
Start, suspend, resume, and shutdown orchestration for macOS VMs.

### virtio — Shared Transport Layer
VirtIO-based communication channel between host and guest.
