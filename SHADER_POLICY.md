# Shader Catalogue Policy

Orbit Wallpaper Engine includes an optional browser for shaders indexed by the KDE Shader Wallpaper project.

## What Orbit distributes

The Orbit Wallpaper Engine repository does **not** bundle third-party catalogue shaders.

The repository ships only the project's built-in `wave.frag`. Catalogue shaders are fetched from upstream only when a user chooses to install one.

## Licence metadata

Orbit reads licence and attribution information from available shader source comments and upstream metadata.

The catalogue uses these states:

- **Permissive** — a recognised permissive/open licence was declared.
- **Upstream unverified** — no licence declaration was found in the shader source.
- **Blocked / unknown** — an explicit licence is recognised as unsuitable, or an explicit licence string cannot be classified.

By project policy, shaders with **no declared licence** may be offered from the curated upstream catalogue and are visibly marked as `upstream-unverified`.

This is a project risk decision. It is **not** a representation that an undeclared shader is in the public domain, free of copyright, or legally cleared for every use.

Shaders with an explicit blocked or unrecognised licence are not offered through the normal catalogue.

## Blacklist

`shader-blacklist.json` contains shader IDs that Orbit must not offer.

The blacklist is applied dynamically to both cached and freshly-fetched catalogue data. A direct `shader-install` call is also required to reject a blacklisted shader.

A blacklist entry may include:

```json
{
  "id": "shader-uuid",
  "reason": "Removed at rights holder request",
  "issue": 123
}
```

## Removal requests

Rights holders, authors, upstream maintainers or other affected parties can request that a shader be removed from Orbit's catalogue through the **Shader removal / blacklist request** GitHub issue form.

Please do not publish sensitive personal information in an issue. If private supporting information is needed, state that in the request.

Maintainers should treat credible rights-holder and upstream-removal requests as high priority and can add the shader ID to `shader-blacklist.json` without waiting for an upstream catalogue change.

## Manual shaders

Orbit also supports manually supplied `.frag` files. Files manually installed by a user are outside the catalogue policy; users are responsible for ensuring they have the rights required for their use.
