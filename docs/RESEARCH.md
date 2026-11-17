# Ultrawide Creations Menu research

## Supported runtime

The current native offsets and stock-byte checks target Starfield 1.16.244.
Every executable patch verifies its expected bytes before writing. A game
update requires a new compatibility review even when the menu appears to open.

## Viewport integration

Native Ultrawide Menus supplies the wide engine scene and matching cursor
bounds. The Creations storefront has an additional 16:9 cap in its own resize
method at RVA `0x13C59D3`. Replacing that branch allows the stock storefront
path to derive its scale and pointer offsets from the wide movie viewport while
leaving the game's shared aspect constant untouched.

The first-entry disclaimer is constructed before the storefront resize and
first-update callbacks. Ultrawide Creations Menu therefore installs its static
layout during SFSE post-load. It prefers the live engine scene, then the largest
visible Starfield window, and finally the current desktop dimensions while the
game window is still being created.

## Home and results layouts

The Home banner and card rows are native widgets with explicit rectangles,
spacing, and draw calls. The compact layout centers a seven-card banner and
rows within the 3840-wide logical storefront. View All and search pages use an
eight-column grid with reduced horizontal and vertical spacing.

Artwork textures are borrowed from the storefront cache. Rapid browsing can
evict an entry before its card draws, so the draw guard confirms that the
assigned texture still matches the active cache entry. Loading backdrops fill
the artwork slot while spinner geometry remains square.

Discount ribbons and Bundle badges are separate native draw groups. Their
rectangles, localized labels, and transforms must move together. Discounted
bundles keep the green percentage ribbon above the dark Bundle label.

## Load Order page

The stock Load Order list ends at logical Y=1700 and spaces eight 168-pixel
rows across its height. That bottom edge is below the footer-safe area at
3440x1440. The compact layout derives the list bottom from the active logical
viewport, reduces the entry widgets to 96 pixels, and updates the native row
count, spacing, scrolling, selection, and text bounds as one unit.

The Load Order Archive popup is a separate 2400x500 native dialog. Its compact
layout keeps the stock two-row height while narrowing the frame, header, title,
and both action bounds to a centered 1400-pixel width.

## Detail page

The detail page uses a gallery, description column, action list, thumbnail
strip, and footer with independent native bounds. At ultrawide scale, the stock
vertical layout extends below the command bar. The compact layout shortens the
gallery, description, and action regions together and changes the action list
to a horizontal row so drawing and input remain aligned.

Installed Creations can expose five action buttons. Their compact horizontal
row reaches into the stock storage-capacity region, so the capacity widget's
left edge follows the end of the complete action row with a fixed gap.

The fullscreen gallery target is a stock 3240x1823 rectangle. A 3440x1440
storefront is about 1607 logical pixels tall, so the stock target extends beyond
both vertical edges. The plugin derives a 16:9 target from the available height
and retains the viewer's stock vertical anchor, placing the image below the
header and above the footer.

Fullscreen arrows are positioned by two native calls that otherwise use the
legacy 2160-pixel gallery midpoint. The redirected calls use absolute X
coordinates outside the resized image and its true vertical center.

## Validation

The primary test configuration is 3440x1440 with Native Ultrawide Menus 6.0.
A release pass should cover the first Creations entry after a cold launch,
Home, View All, search, Options, free and paid items, discounts, bundles,
Load Order navigation through the first and last entries, detail actions,
thumbnail navigation, fullscreen gallery navigation, mouse and controller
input, repeated menu entry, and an extended artwork-browsing session.
