# Intel Iris Xe (renderer v2): investigation notes

Machine: i5-12500H, Iris Xe (80 EU, shared memory), 16 GB DDR4-3200 dual channel, Windows,
driver 101.7092. Game: CUSA00004 (EU) 1.07, whose executable is the US 1.07 one byte for byte.

## Two separate failures

1. **Native passes crash** (native passes on): a fault inside `igvk64.dll` on the pool thread
   recording a draw (`vkCmdDrawIndexed`) or on the recorder thread executing a recorded
   secondary command buffer (`vkCmdExecuteCommands`, `igvk64.dll+0xe167e`, read of 0x4).
   Gone with native passes off. Not investigated further yet.
2. **GPU hang** (native passes off): a readback still waiting after 2 s, then device lost. The GPU
   stops in the submission right after the last finished one. Happens 1 to 5 minutes in, not
   always; one run went 10 minutes clean.

## Runs (native passes off unless noted)

| Build | Change under test | Result |
|---|---|---|
| ced542f | native passes on | igvk64 fault (5 runs) |
| ced542f | native passes off | no crash in 50 s (closed by hand) |
| ced542f | RAM guard (reverted since) | hang, heavy texture eviction |
| 024204f | upstream texture rules | hang at ~2 and ~3 min |
| abd29b1 | compute subgroup pinned to 32 | hang at ~1.5 min |
| fb22646 | + LDS barriers for 64-thread groups | 10 min clean, then a hang at 4.5 min on the next run |
| 6a19dea | + multi-wave LDS / memory barriers | hang at ~55 s |
| 2ed61a6 | ISS_SUBMIT_EVERY=50 | hang |
| 2ed61a6 | multi-wave barriers off | hang at ~85 s |
| 5a9f528 | ISS_SUBMIT_EVERY_DISPATCH=20 | 9 min clean (one run) |
| 1316e1f | ISS_FULL_BARRIERS=1 | hang at ~40 s and ~30 s (two runs) |

The hung submission held hundreds of compute dispatches (421, 597, 1126) of the same ~50
shaders each time, mostly 1x1x1 groups. Indirect dispatches were not in those lists (fixed in
d703368).

## Open hypotheses for the hang

1. A missing synchronization between dispatches, or between dispatches and draws.
2. A submission running longer than the Windows TDR limit (~2 s).
3. A shader that fails only with some timing or overlap.
4. Chance: the hang is irregular, and one clean run proves little.

`ISS_FULL_BARRIERS=1` (full barrier before and after every dispatch, same submission sizes) hung
twice, sooner than without it, so hypothesis 1 is unlikely. The same 24 shaders are in the stuck
submission of every hang, but they also run every frame, so being there does not single one out.
`ISS_SKIP_DISPATCH` on a half of them at a time is the next test.

## Other crashes

- Start-up fault in `igvk64.dll` inside `ImGui::Vulkan::RemoveTexture` (Presenter::RecreateFrame,
  when the guest output size changes): the ImGui descriptor pool was used by the present thread
  and the GPU command processor without a lock. Locked since.
- Fault in `TextureCache::InvalidateMemoryFromGPU` (from `Rasterizer::CopyBuffer`) right after a
  hang: a copy to a nonsense address once the device is lost; a consequence of the hang.

## Switches (all off by default unless noted)

- `ISS_PIN_SUBGROUP=0`: leave the compute subgroup size to the driver (pinned by default).
- `ISS_LDS_BARRIERS=0`: LDS barriers by driver ID only, as upstream (on for Intel by default).
- `ISS_LDS_MULTI_WAVE=1`, `ISS_WAVE_MEMORY_BARRIERS=1`: the multi-wave barrier passes.
- `ISS_FULL_BARRIERS=1`: full barrier around every dispatch.
- `ISS_SUBMIT_EVERY_DISPATCH=N`: submit every N dispatches.
- `ISS_SKIP_DISPATCH=<hash>,...`: drop those compute shaders (direct and indirect paths).
- `ISS_HANG_TRACE=0`: no `HANG_DISPATCHES` report.

Set them in `gpu_v2\defaults.env` (the launcher reads it at every start); a variable set in a
terminal only reaches the game if the launcher is started from that terminal.

## Not bugs

- Blurry, blocky terrain: the game at quality level 14, forced by "Automatic quality: target
  frame rate".
- Smeared frames on fast turns: the game's motion blur, long at 10 to 16 FPS.
- Green flashes in the first session after a shader change: shaders being compiled.
