# Ultrawide Creations Menu 0.5.8

Ultrawide Creations Menu adapts Starfield's Creations storefront to the wide
viewport supplied by Native Ultrawide Menus. It changes only the Creations
interface and leaves the main menu and other game menus to the companion mod.

## Features

- Applies the ultrawide layout on the first Creations entry.
- Centers the Home banner across seven card positions.
- Aligns banner arrows, tags, and slide indicators.
- Uses compact Home cards with more room for long titles and prices.
- Keeps loading artwork and spinners inside their image slots.
- Displays full-width discount ribbons and localized Bundle labels.
- Uses a centered eight-column grid for View All and search results.
- Reworks detail pages to clear the footer and keep action buttons visible.
- Constrains fullscreen gallery artwork by height while preserving 16:9.
- Centers fullscreen navigation arrows outside the image edges.
- Gives the Options dialog enough height for every row.

## Requirements

- Starfield 1.16.244
- A matching version of SFSE
- [Native Ultrawide Menus 6.0](https://www.nexusmods.com/starfield/mods/9410)

Disable other mods that patch the Creations storefront layout before testing.

## Installation with Mod Organizer 2

1. Install and enable Native Ultrawide Menus 6.0.
2. Install the MO2 archive from the [v0.5.8 GitHub release](../../releases/tag/v0.5.8).
3. Enable Ultrawide Creations Menu after Native Ultrawide Menus.
4. Disable any older Ultrawide Creations Menu test entry.
5. Launch Starfield through SFSE.

The archive installs one file:

```text
SFSE/Plugins/UltrawideCreationsMenu.dll
```

## Compatibility and testing

Version 0.5.8 is built specifically for Starfield 1.16.244 and has been tested
at 3440x1440. Other 21:9 and 32:9 resolutions need community verification.
Because the plugin patches runtime-specific native code, it should not be used
after a Starfield update until compatibility has been confirmed.

For a clean test, enable only Native Ultrawide Menus and Ultrawide Creations
Menu among UI-layout plugins. Report the Starfield resolution, game version,
SFSE version, and attach `Documents/My Games/Starfield/SFSE/Logs/Ultrawide
Creations Menu.log` when describing a problem.

## Build

Building requires XMake 3.0 or newer, Visual Studio 2022, and the CommonLibSF
submodule.

```powershell
git submodule update --init --recursive
xmake build UltrawideCreationsMenu
.\staging\New-TestPackage.ps1
```

The packaging script checks the DLL's embedded version and creates an MO2-ready
ZIP with a SHA-256 checksum under `build/staging/`.

## Releases

The [v0.5.8 GitHub release](../../releases/tag/v0.5.8) includes the installable
MO2 archive, source snapshot, and checksums. See [CHANGELOG.md](CHANGELOG.md) for
the standalone mod's release history.
