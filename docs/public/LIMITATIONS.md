# MacRun Limitations, Tiers & Compatibility Matrix

This document provides a realistic assessment of the capabilities, limitations, and planned development tiers of the **MacRun** platform.

---

## 1. Execution Tiers Matrix

MacRun divides application compatibility into five execution tiers. Lower-numbered tiers run with native-level performance, while higher tiers rely on virtualization to handle complex macOS-native requirements.

| Tier | Strategy | Supported Applications | Status |
| --- | --- | --- | --- |
| **Tier 0** | **Runtime Substitution** | Electron, Tauri, Wails apps (e.g., Obsidian, Claude Desktop, Cursor, Windsurf) | **Functional** (Validated with Obsidian and Claude Desktop) |
| **Tier 1** | **CLI Compatibility** | Mach-O CLI binaries, Homebrew tools, Swift compiler | **Infrastructure Phase** (Darling adapters established) |
| **Tier 2** | **Lightweight Cocoa** | Simple native AppKit/Cocoa graphical applications | **Infrastructure Phase** (Cairo/Wayland rendering adapter interface designed) |
| **Tier 3** | **ARM64 Translation** | Apple Silicon CLI binaries running on x86_64 host processors | **Planned / Exploratory** |
| **Tier 4B**| **VM-Assisted Streaming**| Complex, deeply integrated macOS apps (e.g., Raycast, Alfred, SwiftUI-heavy utilities) | **Planned / Exploratory** (Reversed WSLg concept) |

---

## 2. Tested & Validated Applications

The architecture has been proven against the following real-world applications:

### **Claude Desktop** (Tier 0)
- **Status**: **Launches and renders**.
- **Capabilities**: Conversation management, file upload, window scaling, clipboard transfer, and standard desktop notifications operate correctly.
- **Degradation Notes**: Auto-updater stubbed (Class C normalization), secure credential storage bridged to `libsecret`.

### **Obsidian** (Tier 0)
- **Status**: **Fully operational**.
- **Capabilities**: Filesystem reads/writes, plugins, community themes, hotkeys, and markdown rendering function identically to a native Linux experience.
- **Degradation Notes**: Path-mapping shimmed to XDG directories.

---

## 3. Explicit Non-Goals & Architectural Limits

To maintain a sustainable engineering scope, the following are explicitly out of scope:

1. **No SwiftUI Parity**: SwiftUI is a complex, state-retaining rendering engine. Reimplementing SwiftUI on Linux userspace is not planned. Graphical applications utilizing complex SwiftUI components will be routed to Tier 4B VM-assisted execution.
2. **No Metal Acceleration**: We do not plan to write a general-purpose Metal-to-Vulkan translation layer. Heavy graphic design or gaming applications requiring native Metal drivers are unsupported.
3. **No AppKit Completeness**: Tier 2 target is "sufficient AppKit" for simple utilities. Elements like advanced accessibility hooks, complex system compositors, and custom window styles are flattened or ignored.
4. **No Kernel Substrate**: MacRun does not support macOS kernel extensions (KEXTs) or Hypervisor.framework applications.
5. **No Apple Ecosystem Integrations**: Apple ID sign-ins, iCloud sync, iMessage, and App Store DRM are not supported.
6. **No Production Sandbox Guarantee**: While processes run in standard Linux environments, the platform does not implement the Darwin sandbox. It is intended for developer desktop compatibility rather than secure untrusted binary execution.
