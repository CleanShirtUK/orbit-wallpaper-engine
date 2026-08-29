# Multi-monitor Coordinate Model

## Existing behaviour

The renderer creates one layer/EGL surface per enabled output. `wl_output`
geometry supplies each output's desktop position and the layer configure event
supplies its rendered width and height. The renderer computes the bounding
rectangle of all enabled outputs:

- `min_x`, `min_y`: virtual desktop origin;
- `max_x - min_x`, `max_y - min_y`: virtual desktop extent;
- `item->x - min_x`, `item->y - min_y`: output origin within that extent.

`wave.frag` uses this contract directly:

```glsl
vec2 local = gl_FragCoord.xy / u_resolution;
vec2 global = (u_origin + gl_FragCoord.xy) / u_canvas;
```

With render-scale enabled, all three geometry values are multiplied by the
render scale so they remain in the same coordinate space as the offscreen
framebuffer. Negative output positions are normalized by subtracting `min_x`
and `min_y`; gaps between outputs remain part of the bounding rectangle.

When multi-monitor scaling is disabled, `u_origin` is zero and `u_canvas` is
the local output size. This preserves independent per-output rendering.

## Standard contract

Every prepared shader receives these optional Orbit uniforms:

| Uniform | Meaning |
| --- | --- |
| `u_orbit_local_resolution` | Current output's render resolution. |
| `u_orbit_virtual_resolution` | Bounding resolution of enabled outputs. |
| `u_orbit_output_origin` | Current output's offset from the virtual origin. |
| `u_orbit_scale_between_monitors` | `1.0` for virtual mode, otherwise `0.0`. |

Existing `u_resolution` remains local for compatibility. Existing `u_origin` and
`u_canvas` retain the wave shader contract, with their values selected by the
global mode. Shadertoy-style `mainImage` shaders are adapted in the renderer's
wrapper: virtual mode uses virtual `iResolution` and adds
`u_orbit_output_origin` to the `fragCoord` argument.

The common transformation is therefore equivalent to:

```glsl
vec2 orbit_frag_coord = mix(
    gl_FragCoord.xy,
    gl_FragCoord.xy + u_orbit_output_origin,
    u_orbit_scale_between_monitors);
```

## Trade-offs

This is intentionally a renderer/wrapper contract rather than a source rewrite
of every downloaded shader. It works for the installed collection because its
downloaded shaders use `mainImage` and `iResolution`. It does not silently
rewrite arbitrary shaders that use `gl_FragCoord` directly in a custom `main`,
or shaders whose visual design depends on fixed per-monitor pixel dimensions.
Those shaders continue to render safely but may need explicit shader-specific
support to look intentional in virtual mode.

Changing `iResolution` to the virtual size also changes aspect-ratio calculations
in shaders. That is required for a truly continuous normalized canvas, but it
means a shader designed to fill each monitor independently can appear cropped or
have a different aspect. Users can disable the global option for that behavior.

## Installed shader audit

The current installed collection was inspected by coordinate convention:

- Already compatible: `wave.frag`, which directly uses `u_origin` and `u_canvas`.
- Trivially adapted by the `mainImage` wrapper: `002-Blue.frag`, `aurora_terrestris.frag`, `basewarpfbm.frag`, `Booting.frag`, `CineShader_Lava.frag`, `Color_Grid.frag`, `Craziness.frag`, `Electric_Sinusoid.frag`, `Ether.frag`, `Fractal111Gaz.frag`, `Kirby.frag`, `Lignettes.frag`, `Matrix.frag`, `Neon_Parallax.frag`, `Night_Sky.frag`, `Octograms.frag`, `PS3_MenuColor.frag`, `Rainbow_Twister.frag`, and `Xyptonjtroz.frag`.
- Adapted by the wrapper but sensitive to local pixel density, sampling offsets, or aspect/design dimensions: `Crazy_Springs.frag`, `Dez.frag`, `Fractal_Tiling.frag`, and `seascape.frag`. `seascape.frag` compiled and rendered successfully in live testing, but remains Category B because its `1.0 / iResolution.x` epsilon and pixel-offset sampling change with the virtual canvas.
- Requires shader-specific work before it can be considered safely generalized: `AlienVoxel.frag` (its existing body uses the reserved `switch` token under the current GLES compiler), `SuperPlumber.frag`, and `Wolfenstein.frag`.

No audited downloaded shader directly uses `gl_FragCoord` outside the standard
`mainImage` parameter path. The two fixed-dimension shaders are not rewritten;
their existing semantics are preserved and the mode remains user-selectable.
