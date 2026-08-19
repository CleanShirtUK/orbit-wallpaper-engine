# Step 3 compatibility testing

This extraction deliberately retains the existing ps3-wave-wallpaper runtime
paths and service name. It must be proven equivalent before the project rename.

## Backend comparison

Run the old and new catalog commands and compare their JSON:

```sh
~/.local/bin/orbit-settings shader-catalog > /tmp/catalog-old.json
./tools/orbit-wallpaper-helper shader-catalog > /tmp/catalog-new.json
python3 -m json.tool /tmp/catalog-old.json >/tmp/catalog-old.pretty
python3 -m json.tool /tmp/catalog-new.json >/tmp/catalog-new.pretty
diff -u /tmp/catalog-old.pretty /tmp/catalog-new.pretty
```

Timestamps/cache paths may differ. Shader entries, supported flags and licensing
decisions should not.

## Standalone backend smoke test

```sh
./tools/orbit-wallpaper-helper renderer-status
./tools/orbit-wallpaper-helper shader-catalog
```

Do not test `shader-install --apply` until the catalogue comparison succeeds.

## Standalone menu

For a source-tree test, temporarily place the extracted files where the launcher
expects them or launch Quickshell against the settings QML directly. The
Makefile is intentionally not changed during this compatibility step.
