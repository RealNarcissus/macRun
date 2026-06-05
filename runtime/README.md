# Runtime Layer

Execution backends for macOS applications across tiers 0-3.

Architecture reference: docs/architecture/ARCHITECTURE_V6.md
  - "Execution Tier Model" section
  - Tier 0 (Runtime Substitution)
  - Tier 1 (CLI Compatibility)
  - Tier 2 (Lightweight Cocoa Compatibility)
  - Tier 3 (ARM64 Translation)
  - "Runtime Shim Contract"
  - "Translation Layer Contract"

## Subsystems

### shims — Runtime Substitution (Tier 0)
Electron runtime mapping, .asar extraction, Tauri platform adaptation.
Platform API substitution: keychain→libsecret, notifications→libnotify,
shell integration→xdg-open, paths→XDG.

### darling — Compatibility Runtime (Tier 1-2)
Mach-O loading, syscall translation, POSIX-compatible execution.
Foundation for CLI compatibility and lightweight Cocoa support.

### cocoa — Lightweight Cocoa (Tier 2)
Sufficient AppKit compatibility, not macOS reimplementation.
Rendering: AppKit → CoreGraphics → Cairo → Wayland.
Intentional scope: avoids deep SwiftUI, advanced CoreAnimation, Metal rendering.

### arm64 — Binary Translation (Tier 3)
QEMU user-mode Apple Silicon binary execution.
Performance acceptable for utilities, AI apps, editors, launchers.

### common — Shared Types
Runtime-layer types shared across execution backends.
