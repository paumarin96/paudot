# Directional shadow sampling

This fork adds the spatial shader function:

```glsl
float sample_directional_shadow(uint light_index, vec3 position)
```

`position` is in **view space**, matching `VERTEX` in `fragment()`. The return
value is visibility: `0.0` means fully shadowed, `1.0` means fully lit. It uses
the renderer's cascade selection, split blending, filtering, shadow distance
fade, and shadow opacity. Forward+ also uses its normal soft-shadow sampling.
The geometric normal interpolated at the current fragment supplies normal bias.
An offset changes the sampling position, not that normal.

Use it in spatial `fragment()` or `light()` functions, including helpers called
from them. It cannot be called from `vertex()` or other shader types. For
`light()`, the new read-only `uint LIGHT_INDEX` identifies the current light in
its renderer buffer; check `LIGHT_IS_DIRECTIONAL` before treating it as a
directional index. Synthetic lightmap-specular callbacks use `0xFFFFFFFFu`, which
returns `1.0`. Buffer indices are assigned by the renderer and are not stable
node identifiers.

`directional_shadow.gdshader` is a complete example. For direct visualization,
you can also use:

```glsl
shader_type spatial;
render_mode unshaded;

void fragment() {
	ALBEDO = vec3(sample_directional_shadow(0u, VERTEX));
}
```

Supported rendering methods: **Forward+ and Mobile**. Compatibility returns
`1.0`, as do depth-only passes, `shadows_disabled`, invalid indices, and lights
without enabled shadows. The sampled position must remain inside the shadow
coverage prepared for the current camera. Large offsets cannot sample occluders
that were culled from the shadow maps.

This reads realtime directional shadow maps. It does not sample baked lightmap
shadow masks, screen-space contact shadows, or projector textures. Do not
multiply the result by `ATTENUATION` for the same directional light unless you
intend to apply its default shadow a second time.

Adapted for this engine version from:
https://medium.com/@ShaderError/godot-custom-shader-built-ins-functions-part-2-3-4a1772c12dfe

## GPU verification

After building the development editor, run from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File .\build-support\shaders\test.ps1
```

This renders pixel checks on Forward+, Mobile, and Compatibility, and verifies
that vertex-stage calls, vertex helpers, canvas shaders, and incorrect argument
types are rejected. Logs and preview images are written to `bin/shader-smoke`.
