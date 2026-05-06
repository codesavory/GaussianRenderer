# GaussianRenderer
A CPU software rasterizer that renders 2D Gaussian splats to a PPM image.

<figure style="display: flex; justify-content: center; gap: 2%; align-items: center;">
  <div style="flex: 1; text-align: center;">
    <img
      src="./images/saturatedGaussian.png"
      alt="Saturated Gaussian — α=25, σ=80"
      style="width: 48%; border-radius: 8px;" />
    <p style="font-size: 0.85em; color: #888; margin-top: 6px;">Saturated — α=25, σ=80<br>HDR alpha drives white core, red halo</p>
  </div>
  <div style="flex: 1; text-align: center;">
    <img
      src="./images/smoothGaussian.png"
      alt="Smooth Gaussian — α=0.5, σ=100"
      style="width: 48%; border-radius: 8px;" />
    <p style="font-size: 0.85em; color: #888; margin-top: 6px;">Smooth — α=0.5, σ=100<br>Correct straight-colour composite</p>
  </div>
</figure>

## Bug fixes (Phase 1)
- **Premultiplied write** — `splat()` no longer bakes alpha into colour. Straight colour is stored; compositing happens explicitly in `writePPM`.
- **Index typo** — `accumAlpha[i*width + i]` → `accumAlpha[j*width + i]`.
- **Separate alpha buffer** — `renderGaussian` now returns `pair<color_buf, alpha_buf>` so both are available at write time.
- **HDR alpha clamp** — Removed premature `clamp(alpha, 0, 1)` before the colour multiply. Alpha passes through unbounded; the clamp to display range happens at the `unsigned char` cast.
- **File error check** — `writePPM` now reports to `stderr` if the output path cannot be opened.

## Timeline
1. Software Rasterizer: Render a single Gaussian via CPU. ✅
2. Extended Software Rasterizer: Render a scene with multiple Gaussians via CPU (over-composite operator).
3. GPU Rasterizer: Gaussian splatting via shaders.
4. WebGPU Rasterizer: Real-time rendering in the browser via WebGPU.
5. Bonus: Radiance field rendering via CPU.
