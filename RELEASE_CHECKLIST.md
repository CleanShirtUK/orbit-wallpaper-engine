# Release Checklist

## Source

- [ ] Working tree is clean.
- [ ] `VERSION` contains the intended version.
- [ ] `CHANGELOG.md` contains the release entry.
- [ ] No build artifacts are tracked.
- [ ] No user-specific paths are present in shipped runtime files.
- [ ] Remaining `PS3_WAVE_*` references are intentional compatibility aliases only.

## Build and validation

- [ ] `./tools/release-check.sh` passes.
- [ ] `./install.sh --check` passes.
- [ ] `make` succeeds.
- [ ] Renderer starts and remains active.
- [ ] Standalone settings UI opens.
- [ ] Orbit embedded settings UI still opens when tested with Orbit.
- [ ] Intro replay works.
- [ ] Renderer restart works.
- [ ] Palette integration works when explicitly configured.
- [ ] Standalone/default startup plays the intro without requiring Orbit.
- [ ] Steam watcher starts on Hyprland when enabled.
- [ ] Shader browser loads.
- [ ] A compatible shader can be installed and applied.
- [ ] A broken shader rolls back safely.
- [ ] A blacklisted shader is absent from the normal catalogue and cannot be installed directly.

## Policy and documentation

- [ ] `LICENSE` is present.
- [ ] `THIRD_PARTY_NOTICES.md` is current.
- [ ] `SHADER_POLICY.md` is current.
- [ ] `AI_DISCLOSURE.md` is current.
- [ ] KDE Shader Wallpaper credit is present and prominent.
- [ ] No third-party catalogue shaders are bundled.
- [ ] GitHub shader-removal issue template is enabled.

## GitHub

- [ ] Repository description is set.
- [ ] Topics are set.
- [ ] Default branch is correct.
- [ ] Release tag matches `v0.1.0`.
- [ ] Release notes match the changelog.
- [ ] Public repository tree matches the reviewed local tree.
