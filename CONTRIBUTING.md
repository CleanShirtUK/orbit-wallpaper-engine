# Contributing

Contributions are welcome.

## Before opening a pull request

Please:

1. keep changes focused;
2. run the release checks;
3. avoid bundling third-party shaders unless their provenance and redistribution terms have been reviewed;
4. preserve third-party copyright/licence notices;
5. document new configuration variables;
6. test changes on a Wayland compositor that supports wlr-layer-shell when the change affects renderer behaviour.

Run:

```bash
./tools/release-check.sh
```

## Code areas

- `renderer.c` — Wayland/EGL/GLES renderer and shader adaptation.
- `wave.frag` — bundled default shader.
- `settings/` — shared and standalone QML settings UI.
- `tools/orbit-wallpaper-helper` — shader catalogue, install and renderer-control backend.
- `scripts/orbit-wallpaper-steam-watch` — optional Hyprland Steam-game watcher.
- `systemd/` — optional integration services.
- `install.sh` / `uninstall.sh` — user installation lifecycle.

## Shader catalogue changes

Read `SHADER_POLICY.md` before changing catalogue filtering, licence handling or blacklist behaviour.

A shader with no declared licence may be shown under the project's documented `upstream-unverified` policy. Explicitly blocked/unrecognised licences and blacklisted IDs must not be offered by the normal catalogue.

## AI-assisted contributions

AI-assisted contributions are welcome, but contributors remain responsible for reviewing, testing and licensing what they submit.

Please do not present generated code as independently verified merely because it was produced by an AI system.

See `AI_DISCLOSURE.md` for this project's own development disclosure.
