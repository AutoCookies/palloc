# Building the contents of `bin/`

The **bin/** folder holds Windows artifacts used for **malloc override** when building palloc as a shared library (DLL):

| Artifact | Purpose |
|----------|--------|
| **palloc-redirect.dll** (and **palloc-redirect.lib**) | Redirection module. `palloc.dll` imports `pa_allocator_init` and `pa_allocator_done` from it and calls `_pa_redirect_entry` so the C runtime’s malloc/free can be redirected to palloc. |
| **palloc-redirect32.dll** / **.lib** | Same for 32‑bit (x86) builds. |
| **palloc-redirect-arm64.dll** / **.lib** | Same for native ARM64. |
| **palloc-redirect-arm64ec.dll** / **.lib** | Same for ARM64EC. |
| **painject.exe**, **painject32.exe**, **painject-arm64.exe** | Optional tools to inject `palloc.dll` into an executable’s import table (see [readme.md](readme.md)). |

---

## How the DLL and .lib are produced

### Option A: Build from source (default when prebuilts are missing)

If **palloc-redirect\<suffix\>.lib** and **palloc-redirect\<suffix\>.dll** are **not** present in **bin/**, the main CMake build will build them from source:

- **bin/CMakeLists.txt** builds a shared library from **bin/palloc-redirect.c**.
- The resulting **.dll** and import **.lib** are created in the build tree; the main **palloc** target links to that library and copies the DLL next to **palloc.dll**.

So you do **not** need to put anything in **bin/** for a normal Windows build:

```cmd
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The redirect DLL is built automatically and copied next to **palloc.dll**.

**Note:** The current **palloc-redirect.c** is a **stub**: it exports `pa_allocator_init` / `pa_allocator_done` and notifies palloc via `_pa_redirect_entry`, but it does **not** perform IAT hooking of the C runtime. So with this stub, full malloc/free override will not occur until the redirect module is extended (or replaced with a prebuilt that does the hooking).

### Option B: Use prebuilt .dll and .lib in `bin/`

If you already have **palloc-redirect\<suffix\>.dll** and **palloc-redirect\<suffix\>.lib** (e.g. from another build or a prebuilt package), place them in **bin/**:

- **x64:** `bin/palloc-redirect.dll`, `bin/palloc-redirect.lib`
- **x86:** `bin/palloc-redirect32.dll`, `bin/palloc-redirect32.lib`
- **ARM64:** `bin/palloc-redirect-arm64.dll`, `bin/palloc-redirect-arm64.lib`
- **ARM64EC:** `bin/palloc-redirect-arm64ec.dll`, `bin/palloc-redirect-arm64ec.lib`

The root CMake will detect them and use these instead of building from **bin/palloc-redirect.c**.

### Option C: Build only the redirect into `bin/` (standalone)

To build **only** the redirect DLL and import library and have them written into **bin/** (e.g. to create prebuilts):

1. From the repo root, configure with the desired generator and architecture, and turn off the main library if you only want the redirect:

   ```cmd
   cmake -B build -G "Visual Studio 17 2022" -A x64 -DPA_BUILD_SHARED=ON -DPA_WIN_REDIRECT=ON
   ```

2. Build the **palloc-redirect** target. The output will be under **build/bin/** (or similar). To get files into **bin/**:

   - Copy them manually, or  
   - Set in **bin/CMakeLists.txt** something like:

     ```cmake
     set_target_properties(palloc-redirect PROPERTIES
       RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
       LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
       ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
     )
     ```

   then rebuild; the **.dll** and **.lib** will appear in **bin/**.

---

## Building the injector tools (painject)

The **painject** tools (e.g. **painject.exe** for x64) are **not** built by this repo. They are the same idea as **minject** in the [mimalloc](https://github.com/microsoft/mimalloc) project: they patch an executable’s import table to load **palloc.dll**.

Options:

- Use the injector from mimalloc’s **bin/** and, if needed, rename or configure it to use **palloc.dll**.
- Or build/maintain a small standalone tool that adds **palloc.dll** to an executable’s import table (e.g. with the same logic as minject) and name it **painject**.

See **bin/readme.md** for usage of **painject** once you have the executables.
