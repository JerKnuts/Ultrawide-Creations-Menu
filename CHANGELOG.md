# Changelog

All notable changes to Ultrawide Creations Menu are documented here.

## [Unreleased]

No unreleased changes.

## [0.5.17] - 2026-11-26

- Applied the compact layout before the warning splash and Home widgets are constructed.
- Removed the storefront's startup aspect cap to match its resize behavior.
- Logged the Creations movie and game window sizes separately to diagnose pillarboxing.

## [0.5.16] - 2026-11-26

- Waited for the game window before choosing the Creations layout dimensions.
- Used a six-row Load Order layout when ten compact rows would not fit.

## [0.5.15] - 2026-11-20

- Kept the Like icon clear of file size and version in Load Order.
- Limited mouse enable and disable actions in Load Order to the left checkbox.
  Clicking the row still selects it for keyboard reordering with `X`.

## [0.5.14] - 2026-11-16

- Reduced the Load Order Archive dialog from 1600 to 1400 logical pixels wide.
- Recentered the dialog frame, header, title, and both archive actions.

## [0.5.13] - 2026-11-16

- Constrained the animated download-progress fill to the compact Download
  button width.
- Prevented download progress from drawing behind neighboring detail actions.

## [0.5.12] - 2026-11-16

- Reduced the Load Order Archive dialog from 2000 to 1600 logical pixels wide.
- Recentered the frame, header, title, and both actions at the smaller size.

## [0.5.11] - 2026-11-16

- Reduced and recentered the Load Order Archive dialog width.
- Kept the title, background, and both archive actions aligned with the smaller
  window.

## [0.5.10] - 2026-11-16

- Shortened the detail page's storage-capacity bar from the left.
- Prevented the capacity bar and labels from overlapping the five-button action
  row shown for installed Creations.

## [0.5.9] - 2026-11-16

- Added a compact ten-row layout to the Load Order menu.
- Capped the Load Order list above the command footer so the final entries no
  longer run off the bottom of the ultrawide viewport.
- Kept row geometry, text, selection, scrolling, and scrollbar limits in sync.

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
