# Execution Policies and Degradation Rules

Encodes the architecture's execution strategy decisions:
- Invariant 2: Execution strategy must match app category
- "Degradation Rules" section
- "Execution Strategy overview" matrix

Maps application categories to execution strategies:
| App Type                        | Preferred Strategy          |
| ------------------------------- | --------------------------- |
| Electron                        | Native runtime substitution |
| Tauri                           | Hybrid bridge               |
| CLI                             | Darling                     |
| Lightweight AppKit              | Cocoa-lite compatibility    |
| SwiftUI-heavy/system-integrated | VM-assisted execution       |

Architecture reference: docs/architecture/ARCHITECTURE_V6.md
  - "Execution Strategy overview"
  - "Degradation Rules" section
  - "Architectural Invariants"
