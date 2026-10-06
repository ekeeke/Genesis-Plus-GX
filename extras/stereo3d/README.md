# Experimental Stereo 3D for Genesis Plus GX

Passive row-interleaved 3D support on the intended ASUS monitor is
**unconfirmed**. Its exact model is unknown. Side-by-Side transports the two
eye images; the final RetroArch shader pass interleaves their rows.

## Build and run

The following build was verified on macOS:

```sh
make -f Makefile.libretro -j4 platform=osx ARCHFLAGS=
```

Output: `genesis_plus_gx_libretro.dylib` in the repository root, arm64, RGB565.
On Linux, use `make -f Makefile.libretro -j4 platform=unix`; this build was not
verified here. Windows/mingw-w64 was also not verified: the available local
Ubuntu image contains neither a compiler nor make, and no local mingw-w64
toolchain is installed. No permanent regression harnesses were added.
This Genesis Plus GX checkout has no `.github/workflows` directory;
RetroArch source files and CI harnesses were not changed.

1. In RetroArch, select **Settings → Drivers → Video → gl**, then restart
   RetroArch. The `.glslp` preset targets OpenGL `gl`, not Metal, Vulkan or
   slang. The shader supports legacy and modern OpenGL GLSL syntax.
2. **Load Core → Install or Restore a Core**: select the built library
   (the menu label may vary by version). Then use **Load Content** to open
   your own legally obtained copy of Sonic for Mega Drive/Genesis.
   This experiment includes no ROM and uses no game-specific hacks.
3. Under **Quick Menu → Core Options → Video**, enable **Stereo 3D**.
   Default offsets: **Plane B = 0**, **Plane A = 2**, **Sprite = 2**.
   Each value is the displacement of **each eye**; total eye separation is
   twice that value: `2` means a difference of `4` game pixels.
   Each offset ranges from `-16` to `16`. Positive values move the left eye
   image left and the right eye image right; negative values reverse the
   direction of depth. **Window** always remains at screen depth.
4. Under **Quick Menu → Shaders → Video Shaders = On → Load Preset**, select
   `extras/stereo3d/full-sbs-interleaved.glslp` and apply it.
   Keep a single pass, with no additional passes/FBOs, scaling or filters
   after it. The preset intentionally specifies no intermediate scale:
   its final pass draws directly into the final framebuffer.
5. Under **Settings → Video → Scaling**, disable Integer Scale and set
   **Aspect Ratio = 4:3** for the first Sonic test. To preserve the original
   proportions exactly, first record the core's display aspect ratio with
   Stereo 3D disabled, then reproduce it with a Custom viewport. Full SBS
   reports twice the width and twice the aspect ratio; row-interleaved
   output needs a viewport with the proportions of **one eye**, rather than
   two images side by side. Use Core Provided when inspecting raw SBS output
   without the shader.
6. Under **Shader Parameters**, try **Row parity** (`0/1`). If necessary,
   toggle **Swap eyes** in the shader or **Swap Eyes** in the core.
   Enabling both swaps cancels them out. Row parity also exchanges the eyes
   assigned to rows, but is intended to align them with the panel's polarization.
7. To restore ordinary output, disable Stereo 3D and the shader, and restore
   your previous Aspect Ratio. Normal geometry and the NTSC filter are restored.

## Monitor test conditions

Use the panel's **native physical resolution** and 1:1 output, with no scaling
by the monitor, OS or compositor after the shader. On HiDPI displays, logical
window dimensions do not equal physical pixels. Prefer fullscreen on the
intended monitor; disable display overscan, screen rotation and fractional
scaling. In windowed mode, framebuffer rows must map directly to physical
panel rows, and the window's vertical position must remain stable.
Recalibrate Row parity after moving the window vertically, changing the
display/resolution or viewport height, or switching fullscreen modes.

Parity comes from `floor(gl_FragCoord.y)` in the final OpenGL framebuffer,
not from the source game's scanline. OpenGL counts rows from the bottom;
Row parity compensates for this orientation and the panel's unknown
polarization assignment. Letterboxing and viewport position are accounted
for by the actual framebuffer coordinate; moving the entire window on the
screen requires manual compensation. Do not use a recording or stream that
is subsequently scaled to evaluate 3D.

First test: Sonic, B=0/A=2/sprites=2. View each eye separately through suitable
passive glasses, then both together. Compare the background, ground, character
and HUD depth. Adjust the depth sign/magnitude and eye order. Parallax bands
within a single plane share the same assigned depth.

## Implementation and limitations

- Only `SYSTEM_MD` is affected; SMS, Game Gear, SG-1000, Pico, Sega CD and PBC
  retain their existing behavior.
- The ordinary renderer runs once per scanline and preserves VDP side effects.
  Each eye receives separate Plane A/B samples from VRAM/cache, Window and
  the same parsed sprite list before composition. Machine time, SAT parsing,
  IRQs, collisions and overflow/masking are not processed again for each eye.
- Priorities, transparency and shadow/highlight use the existing LUTs.
  Horizontal scroll is read for each scanline. Ordinary two-cell-column
  vertical scrolling and interlace mode 2 are supported, including the
  existing fetch behavior at the Window boundary and partial leftmost column.
- Tiles are sampled beyond the horizontal viewport edges; the sprite buffer
  has room for the maximum displacement. Revealed areas show lower layers
  or the backdrop color, rather than copies of the final image's edge pixels.
  Window replaces Plane A as in the ordinary renderer; transparent Window
  pixels reveal Plane B.
- **Enhanced per-tile vertical scroll** and MD display modes other than
  Mode 5 produce identical ordinary scanline images for both eyes, without
  depth. This is an explicit fallback, not stereo support for those modes.
- Stereo 3D bypasses Blargg NTSC and LCD persistence. Their settings are
  retained for ordinary output. The lightgun cursor drawn by the frontend
  wrapper is not displayed in SBS. This mode primarily targets ordinary
  games played with a gamepad.
- Full SBS is 512/640 pixels wide without horizontal overscan, or up to 688
  with it. Heights follow the existing viewport, including interlace doubling.
  The buffer is 688×576 with a 1376-byte RGB565 pitch. Geometry and maximum
  dimensions are updated through libretro on enable/disable and viewport changes.
- When disabled, the original framebuffer and rendering path are used, with
  only inexpensive conditional checks added. Enabling stereo is significantly
  more expensive: two eye images are sampled and composed in addition to the
  ordinary rendering pass.

## Validation performed

- macOS arm64 libretro build; existing compiler warnings remain.
- 512 zero-depth comparisons against the ordinary renderer using synthetic
  VDP data: H32/H40, Window, scrolling, sprites, priorities, shadow/highlight
  and interlace mode 2. Repeated for both ordinary and `ALT_RENDERER` paths.
- A further 256 checks per path covering extreme offsets of ±16, Swap Eyes,
  interlace, borders and blanking with AddressSanitizer; preservation of
  status, spr_col and spr_ovr during eye composition.
- Explicit checks that Plane B is revealed after shifting Plane A/a sprite,
  and that shadow/highlight operator sprites move with their assigned depth.
- 80 synthetic geometry checks: enable/disable, 256/320 widths, heights from
  224 to 288, horizontal borders, interlace and the NTSC setting.
- Renderer C89 syntax check (existing dependency `inline` mapped to a compiler
  extension), GLSL 120/150 validation with glslangValidator, and a diff check
  accounting for preserved CRLF line endings.

VDP and geometry checks used temporary harnesses without a ROM or gameplay.
RetroArch.app is installed, but gameplay, GPU shader rendering and the effect
on the monitor are **unverified**. Successful compilation and synthetic checks
do not establish the panel's compatibility with 3D.
