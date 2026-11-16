# Project maintenance rules

## Versioning

- Use `A.B.C`. Increment `A` for a final release milestone, `B` for a substantial feature or major development milestone, and `C` for smaller changes. Reset trailing components when incrementing a higher component.
- A coherent group of edits may use a single patch increment.
- Keep `VERSION`, `xmake.lua`, the README heading, package filenames, and embedded plugin metadata synchronized.
- Preserve each version under `releases/<version>/` before advancing. Include a source snapshot, and add the installable MO2 package and SHA-256 checksums after a successful build.
- Keep earlier test binaries under `releases/legacy/<embedded-version>/` with their original filenames.
- Update `CHANGELOG.md` for every version. Move unreleased entries into a dated version section when advancing the version.
- Rebuild binaries when the version changes so embedded metadata matches the package version.
