# Changelog

All notable changes to Ultrawide Creations Menu are documented here.

## [Unreleased]

No unreleased changes.

## [0.5.8] - 2026-11-15

- Finalized the standalone project documentation and release structure.
- Simplified dependency handling to require only Native Ultrawide Menus.

## [0.5.7] - 2026-11-15

- Fixed the fullscreen gallery arrows overlapping at the image center.
- Positioned both arrows from absolute image-edge coordinates.

## [0.5.6] - 2026-11-15

- Added fullscreen gallery arrow positioning helpers.
- Centered the arrows vertically on the resized artwork and moved them beyond
  its left and right edges. This release still allowed both arrows to inherit
  the same horizontal position and was superseded by 0.5.7.

## [0.5.5] - 2026-11-15

- Raised height-constrained fullscreen artwork into the visible center.
- Retained the gallery's stock vertical anchor and moved its arrows upward.

## [0.5.4] - 2026-11-15

- Constrained fullscreen gallery artwork by the available ultrawide height.
- Preserved the artwork's 16:9 aspect ratio and recentered it horizontally.

## [0.5.3] - 2026-11-15

- Installed the Creations layout during SFSE post-load, before the first-entry
  disclaimer is constructed.
- Added a desktop-size startup fallback when the Starfield scene and window are
  not yet available.

## [0.5.2] - 2026-11-15

- Added a live Starfield window-size fallback for first-entry initialization.
  The first disclaimer still initialized before the covered menu callbacks, so
  this release was superseded by 0.5.3.

## [0.5.1] - 2026-11-15

- Renamed the MO2 archive so Quick Install defaults to Ultrawide Creations Menu.

## [0.5.0] - 2026-11-15

- Created Ultrawide Creations Menu as a standalone SFSE plugin.
- Collected the Creations storefront layout and stability changes into a
  standalone plugin.
- Integrated with Native Ultrawide Menus 6.0 for the shared viewport and cursor.
- Removed the Creations-only 16:9 viewport cap without changing the game's
  shared aspect constant.
- Added compact Home cards and banners, eight-column result grids, corrected
  discounts and Bundle badges, loading-artwork guards, dialog fixes, compact
  detail pages, and horizontal detail actions.
