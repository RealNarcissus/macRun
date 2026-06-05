# Protocol Definitions

Interface contract specifications, IPC protocol definitions, and wire format
descriptions for cross-subsystem communication.

The architecture requires that all subsystem communication occurs through
explicit contracts. No subsystem may directly depend on undocumented internal
behavior, shared mutable state, or implicit execution assumptions.

Architecture reference: docs/architecture/ARCHITECTURE_V6.md
  - "Interface Contracts" section
  - All subsystem contract subsections

## Protocol Categories

- **macrun ↔ detector**: Capability query and tier resolution
- **macrun ↔ runtime**: Execution backend selection and launch
- **host proxy ↔ VM bridge**: Frame transport, input forwarding, clipboard sync
- **host proxy ↔ integration**: Wayland surface, audio routing, notifications
- **compat-db ↔ detector**: Detection signature queries

No protocols have been specified yet. This directory is ready for
protocol design work.
