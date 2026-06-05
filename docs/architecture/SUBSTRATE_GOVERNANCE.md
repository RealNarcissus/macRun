# Substrate Governance

Architecture reference: docs/architecture/SUBSTRATE_MODEL.md Sections 7-9

## Component Classification

| Component | Classification | Source | Version Strategy |
|-----------|---------------|--------|-----------------|
| Darling | Forked | Custom `c3e5d0a` fork | Pinned commit hash |
| QEMU (user-mode) | System dependency / Pre-built binary | Upstream 8.2.0 static | Pinned release tag, pre-built binary preferred |
| QEMU (system VM) | System dependency / Pre-built binary | Upstream 9.0.0 | Pinned release tag |
| Electron runtimes | Externally managed | Upstream Linux Electron releases | Pinned matrix: 22.x, 24.x, 28.x |
| WebKitGTK | System dependency | Host package manager | Minimum 2.40.0 |
| Cairo | System dependency | Host package manager | System-provided |
| libarchive | System dependency | Host package manager | System-provided |
| libsecret | System dependency | Host package manager | System-provided |
| nlohmann/json | Vendored | Upstream v3.11.3 | Pinned in third_party/ |

## Build Isolation Rules

Per SUBSTRATE_MODEL.md Section 9:

1. **Darling**: Compiled inside isolated containers/chroots with dedicated LLVM/clang sysroots.
   The orchestrator never links against Darling libraries.

2. **QEMU**: Static binaries pulled from pre-compiled toolchains or built out-of-tree.
   Never linked into the platform build.

3. **Electron**: Binaries are never compiled. Pre-packaged Linux binaries cached in
   `~/.cache/macrun/electron/`. Integrity verified via SHA256 checksums.

4. **WebKitGTK**: Installed via host package manager. Adapter probes at runtime.
   Never linked at compile time into the orchestrator.

5. **Adapters**: Compiled as modular units, tested with mock objects.
   No live substrate dependencies during CI test runs.

6. **Orchestration**: Zero compile-time dependencies on Darling/QEMU/Electron/WebKitGTK.

## Upgrade Policy

Per SUBSTRATE_MODEL.md Section 7:

- **Cadence**: Bi-monthly evaluation cycle for substrate version bumps
- **Rolling updates**: Prohibited. All foundational runtimes are pinned.
- **Patch policy**: Security patches may be fast-tracked. Feature upgrades follow the bi-monthly cycle.
- **Replacement feasibility**: Each substrate has documented replacement candidates (see Section 3-4 of SUBSTRATE_MODEL.md).

## Directory Layout

```
runtime/third_party/
├── GOVERNANCE.md          ← this file
├── darling/               ← Darling build scripts and patches
├── qemu/                  ← QEMU user-mode acquisition scripts
├── electron/              ← Electron runtime cache management
├── webkit/                ← WebKitGTK system probe
└── patches/               ← Upstream patches for all substrates
```
