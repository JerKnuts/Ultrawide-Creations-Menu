# Ultrawide Creations Menu test packages

Run from the project root in PowerShell.

```powershell
.\staging\New-TestPackage.ps1 -Label local
```

The script builds the current source, checks the DLL's embedded version,
and creates an MO2-ready ZIP and SHA-256 checksum under `build/staging/`.
Formal releases include a source snapshot and checksums under
`releases/<version>/`. Stage the finished release in the Ultrawide Creations
Menu folder under Starfield Modding.

Enable Native Ultrawide Menus before testing and disable previous test builds
or other mods that patch the Creations storefront layout.
