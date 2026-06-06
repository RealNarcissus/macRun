# Architecture Documentation

## Canonical Specifications

### ARCHITECTURE_V6.md
The authoritative architecture specification for the entire platform. All implementation decisions must be traceable to sections within this document.

### SUBSTRATE_MODEL.md
Formalizes the classification, ownership boundaries, and lifecycle orchestration of all runtime substrates.

### FAILURE_MODEL.md
Details the platform failure modes, graceful degradation pathways, and process-level crash containment boundaries.

### COMPAT_DB.md
Formalizes the schemas, validation invariants, directories, and version evolution mechanisms of the Compatibility Database.

### DEGRADATION_MODEL.md
Governs all degradation behavior across every execution tier. Defines 7 degradation categories (Transparent Substitution through Hard Failure), escalation policy, diagnostics requirements, and prohibited behaviors. Canonical for unsafe compatibility, experimental modes, and shim-based degradation.

### SHIM_GOVERNANCE.md
Specifies shim ownership, activation rules (always-active, conditional, adapter-owned), preload ordering, mutation boundaries, teardown expectations, and observability requirements. Canonical for all shims in `runtime/shims/`.

### SEMANTIC_DIAGNOSTICS.md
Defines the renderer semantic observability infrastructure: diagnostic shims (`renderer-diag.js`, `main-diag.js`), semantic failure categories (16 classifications), blank-window investigation procedure (9-step diagnostic flowchart), and integration with the degradation governance model.

### ELECTRON_API_NORMALIZATION.md
Governs all Electron API normalization behavior. Defines 4 risk classes (Class A: Cosmetic No-Op through Class D: Forbidden), the centralized normalization registry structure, version-scoping rules, diagnostic format `[MACRUN:NORMALIZATION]`, degradation integration, and the procedure for adding new governed normalizations. Authoritative for all Electron API patches.

### COMPATIBILITY_SPECTRUM.md
Codifies the empirically discovered Electron application compatibility spectrum across four architecture classes: Class A (Self-Contained), Class B (API Drift), Class C (IDE-Class), and Class D (Client-Server). Defines the classification decision matrix, detection pipeline integration, and compat-db metadata extensions. Authoritative for all application classification decisions.

### RUNTIME_NEGOTIATION.md
Formalizes the governance of Electron runtime version selection and substrate negotiation. Defines the multi-version runtime matrix, the negotiation algorithm, selection invariants, and the new `[MACRUN:SUBSTRATE]` diagnostic domain. Documents the renderer layout and compositor diagnostics frontier. Authoritative for all Electron runtime substrate selection decisions.

## Future Documents
- ADRs (Architecture Decision Records) in [adr/](file:///home/charleton/Desktop/agentProjects/Codex/macRun/docs/architecture/adr/)
- Per-subsystem design docs in [docs/design/](file:///home/charleton/Desktop/agentProjects/Codex/macRun/docs/design/)
