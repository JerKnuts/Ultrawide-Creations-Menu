# Ultrawide Creations Menu architecture

Native Ultrawide Menus supplies the engine viewport and cursor bounds. The
Ultrawide Creations Menu SFSE plugin checks for that dependency as the
storefront initializes and reads the game render rectangle. It removes the
16:9 width cap in the Marketplace menu's own resize function. The original
function then derives drawing scale and pointer coordinates from the movie
viewport.

On the first storefront entry, the shared scene can still report its old
16:9 bounds. The plugin uses the largest visible Starfield window's client
area as the startup fallback, then prefers the engine scene after Native
Ultrawide Menus has widened it. SFSE post-load installs the layout before the
first disclaimer movie is constructed. If the game window does not exist yet,
the current desktop dimensions provide the startup aspect.

Storefront-specific patches adjust Home, View All, search, details, Load Order,
and Options. The Load Order pass derives a footer-safe list bottom from the
logical viewport and changes its row geometry and scroll window together. Its
archive popup uses a separate centered compact rectangle set.
Detail page geometry uses the first ultrawide render rectangle seen in the
session. The artwork draw guard checks the current cache entry before drawing
a tile. The storage-capacity widget begins after the full installed-item action
row rather than sharing its horizontal space.

The fullscreen detail gallery uses a centered 16:9 rectangle derived from
the logical viewport height. Its horizontal half-size and arrow offsets are
updated with the rectangle. The vertical position retains the stock anchor
because the gallery widget continues to expose a 2160-high coordinate space.
Fullscreen arrow calls are redirected through two narrow position helpers so
their centers follow the resized image rather than the old gallery midpoint.

The package contains one SFSE DLL. The viewport dependency is distributed
separately on Nexus.
