# Nugz Support Tool

Nugz is a Windows support utility for hardware inventory, platform diagnostics, security visibility, controlled maintenance, and local report export.

## Projects

- `Support/` - native Nugz Support Tool dashboard and diagnostics.
- `KeyGen.vcxproj` - standalone access-key generator.
- `website/` - static multilingual website for the project.

## Build

Open a Visual Studio Developer PowerShell and run:

```powershell
msbuild .\Support.sln /m /p:Configuration=Release /p:Platform=x64
```

Executables are written to `x64\Release\`.

## Website

Deploy the contents of `website/` with a static-site host. The site includes download links under `website/downloads/` when those binaries have been copied there locally.

## Notes

The support tool requires administrator privileges for some Windows checks. Review any system-changing action before confirming it. Generated `support.key` files are intentionally ignored by Git.
