# Running Codex Desktop on Linux via macRun

This guide details the step-by-step process to execute the modern macOS **Codex Desktop** application natively on Linux using the **macRun** platform. 

Codex Desktop is a **Class D: Client-Server** application: it features an Electron front-end shell that communicates via a stdio-based Model Context Protocol (MCP) pipe with a bundled macOS command-line backend app-server (`codex`). It also relies on a local SQLite database module (`better-sqlite3`) to manage configuration, local projects, and user session states.

By following this guide, you will compile the required Linux-native SQLite module, substitute the macOS backend helper with its Linux-native equivalent, and execute the app with correct window/theme layouts and zero-glitch solid backgrounds.

---

## Screenshots of Successful Execution

Here is Codex Desktop running on Ubuntu 24.04 via the macRun platform under our newly implemented C++ dynamic runtime shims:

### Light Mode Layout
![Codex in Light Mode](images/codex_light.png)

### Dark Mode Layout
![Codex in Dark Mode](images/codex_dark.png)

---

## Step 1: Install Platform Prerequisites & Build macRun

Ensure your host environment has all standard build tools, Node.js, and compiler suites installed.

```bash
# 1. Compile the macRun orchestrator CLI
cmake -B build
cmake --build build

# 2. Deploy the core integration shims into the runtime cache (~/.cache/macrun/shims)
./runtime/shims/install.sh

# 3. Cache the negotiated Electron 42.3.3 substrate
./runtime/third_party/electron/acquire.sh --all
```

> [!NOTE]
> macRun dynamically inspects the Codex bundle framework metadata and negotiates the closest matching runtime version. It automatically resolves that Codex was built against **Electron 42.1.0** and targets the cached host environment's **Electron 42.3.3** binary for execution.

---

## Step 2: Extract the Codex macOS App Bundle

If you have downloaded the macOS installer (`.dmg`), extract the `.app` bundle using `7z`:

```bash
# Extract the bundle into a temporary workspace
7z x "/path/to/Codex Installer.dmg" -o"/tmp/codex-run/"
```
This will yield the folder `/tmp/codex-run/Codex Installer/Codex.app`.

---

## Step 3: Resolve the Linux-Native CLI App-Server

The bundled CLI app-server inside the macOS bundle (`Contents/Resources/app.asar.unpacked/bin/codex`) is a Mach-O ARM64/x86_64 binary. Linux cannot execute it directly. We must substitute it with a Linux-native ELF version.

You can install the official Linux-native npm package globally to get the compiled CLI binary:

```bash
# Install the Codex NPM module containing the Linux-native CLI
npm install -g @openai/codex
```

This installs the Linux-native Rust app-server CLI on your system. Note down the path to this executable. Usually, it is installed at:
`~/.local/bin/codex`
or:
`~/.local/lib/node_modules/@openai/codex/node_modules/@openai/codex-linux-x64/vendor/x86_64-unknown-linux-musl/bin/codex`

---

## Step 4: Compile & Substitute the Linux-Native SQLite Module

Because Codex uses `better-sqlite3` for local storage, the macOS `.node` compiled binary in the app bundle will crash on Linux due to ABI and platform mismatches. Furthermore, because Electron 42 uses V8 v13+ (which enforces strict V8 Sandbox rules), you must compile a patched version of `better-sqlite3@12.9.0` for Electron 42.3.3.

### 1. Set Up a Build Directory
```bash
mkdir -p /tmp/build-sqlite
cd /tmp/build-sqlite
npm init -y
npm install better-sqlite3@12.9.0
```

### 2. Apply V8 v13+ Compatibility Patches
Open the following source files inside `/tmp/build-sqlite/node_modules/better-sqlite3/` and apply the compatibility edits:

#### File A: `src/util/macros.cpp` (Line 30)
Find the `OnlyAddon` definition and update it to pass the pointer tag under V8 13:
```cpp
#if defined(V8_MAJOR_VERSION) && V8_MAJOR_VERSION >= 13
#define OnlyAddon static_cast<Addon*>(info.Data().As<v8::External>()->Value(v8::kExternalPointerTypeTagDefault))
#else
#define OnlyAddon static_cast<Addon*>(info.Data().As<v8::External>()->Value())
#endif
```

#### File B: `src/better_sqlite3.cpp` (Line 60)
Find `v8::External::New` and update it to provide the pointer tag under V8 13:
```cpp
#if defined(V8_MAJOR_VERSION) && V8_MAJOR_VERSION >= 13
v8::Local<v8::External> data = v8::External::New(isolate, addon, v8::kExternalPointerTypeTagDefault);
#else
v8::Local<v8::External> data = v8::External::New(isolate, addon);
#endif
```

#### File C: `src/util/helpers.cpp`
To resolve compiler template argument ambiguity, ensure that `nullptr` is passed instead of `0` to the `SetNativeDataProperty` method.

### 3. Compile targeting Electron v42.3.3
```bash
npx -y @electron/rebuild --version 42.3.3
```

### 4. Swap the Binary in the Codex Bundle
Copy the newly compiled native `.node` file to overwrite the macOS-native ones inside the extracted Codex workspace:

```bash
# Overwrite the unpacked asar module binary
cp /tmp/build-sqlite/node_modules/better-sqlite3/build/Release/better_sqlite3.node \
  "/tmp/codex-run/Codex Installer/Codex.app/Contents/Resources/app.asar.unpacked/node_modules/better-sqlite3/build/Release/better_sqlite3.node"
```

---

## Step 5: Launch Codex Desktop

Execute the `macrun-cli` binary pointing to the extracted `.app` package and specifying the Linux-native CLI path:

```bash
# Set CODEX_CLI_PATH to your local Linux ELF binary and execute
CODEX_CLI_PATH="/home/charleton/.local/bin/codex" \
MACRUN_ALLOW_DARWIN_NATIVE=1 \
MACRUN_DIAG_RENDERER=1 MACRUN_DIAG_MAIN=1 \
./build/tooling/macrun-cli/macrun-cli --launch "/tmp/codex-run/Codex Installer/Codex.app"
```

> [!TIP]
> **What's Happening Behind the Scenes**:
> * `macrun-cli` unpacks the assets, injects the `boot-shim.js` at startup, and fires the Electron v42 runtime.
> * The shims intercept calls to `BrowserWindow`, `WebContentsView`, and `BrowserView` prototypes, redirecting runtime transparency changes (`#00000000`) and macOS vibrancy settings to solid colors matching your system's active light/dark theme.
> * The Rust backend launches via the MCP stdio pipe, connecting the UI.
> * The newly built `better-sqlite3.node` loads successfully, allowing database actions to hydrate without warnings.
