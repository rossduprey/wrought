# ue/ — the wrought ↔ Unreal seam

This is the **#28 seam smoke-test** (see `STATUS.md`): the smallest thing that proves
wrought's header-only sim is a *portable field* — that it drops into Unreal and one
verb round-trips through a tiny adapter, before any art exists. If this builds and the
round-trip reads right, the "host = scenery only, sim/numbers/tools stay ours" thesis
is proven for real, not asserted.

## What's here

- `WroughtSim/WroughtSim.Build.cs` — module rules. **No library to build**: `core/` is
  header-only C++17, so this only puts `core/` on the include path.
- `WroughtSim/Public/WroughtSimSubsystem.h` — pure-Unreal header (no wrought type leaks
  to Unreal Header Tool). The `USTRUCT`s the panel reads and the `UWorldSubsystem` API.
- `WroughtSim/Private/WroughtSimSubsystem.cpp` — the entire adapter. All `#include` of
  `core/` is confined here.

## The contract (all of it)

- **In:** a world-space hit (a line trace onto the Landscape) → `WorldToPlace()` →
  `Place{x,y}` in meters. Unreal is centimeters; the sim is meters from an arbitrary
  origin; **z is dropped — depth is a discrete tier, not a coordinate.**
- **Out:** `win_bite` / `dig_column` → `FWroughtBite` (total mass, per-phase grade, and
  free-vs-locked mass) for the panel.
- **State the player drives:** `PlaneOrigin` (which world point is `Place(0,0)`),
  `SelectedTier` (SURFACE/MIDDLE/DEEP), `BlowEnergy` (the equipped tool). That's it.

## Drop-in

1. Copy `WroughtSim/` into your UE project's `Source/`, add `"WroughtSim"` to the
   project's `.uproject` modules and `.Target.cs`.
2. Point the build at the sim headers: set env `WROUGHT_CORE=<path>/wrought/core`, or
   keep this `ue/` dir alongside `core/` in the wrought checkout (the Build.cs falls
   back to `../../core`).
3. Build. **If UBT compiles it clean, the portability claim is proven.**

## The smoke-test in the editor

1. A flat Landscape (no art). Put `PlaneOrigin` at the copper hill's foot so
   `DEPOSITS[]` lines up: `copper-hill` is at `Place(0,0) r40`, `tin-creek` at
   `Place(300,120) r25`. Optionally drop debug spheres at those two spots.
2. On click: line-trace to the Landscape, call `BiteAt(Hit)` (or `DigColumnAt`), print
   `FWroughtBite` to a text panel.
3. Equip with `EquipBareHand()` / `EquipSaplingPick()`, toggle `SelectedTier`, and
   watch the seam behave:
   - Bare hand at the hill: **SURFACE** reads cuprite (grade 0.45); **DEEP** wins
     nothing — the tool skips off the fresh rock (`bWonRock == false`). That empty bite
     *is* the pick gate.
   - Sapling pick at the hill: every tier wins; **DEEP** reads chalcocite — the deep
     sulfide the hand could not reach, and it comes up **locked** (breaker-bound).
   - Either tool at the tin creek: every tier reads cassiterite, comes up **free** (no
     locked mass) — the placer needs no pick and no breaker.

## The smoke-test headless (unattended, no GUI)

`WroughtSim/Private/WroughtSeamTest.cpp` is a C++ automation spec that asserts the
same six findings against the **live subsystem** — no clicking. It opens a game world,
grabs `UWroughtSimSubsystem`, feeds it synthetic world coordinates for the copper hill
and tin creek, and checks: hand wins the cuprite cap, hand skips the deep root, pick
wins the chalcocite root **locked**, placer comes up cassiterite **free**. Run it with
the command-line editor (`-nullrhi` = no GPU, so it runs on a headless box or VM):

```
UnrealEditor-Cmd /path/YourProject.uproject \
  -ExecCmds="Automation RunTests Wrought.Seam" -unattended -nop4 -nullrhi -stdout
```

If this passes unattended, the portable-field thesis is proven end to end
(sim → subsystem → panel), and it's the loop a headless UE box makes worth standing up.

## Verifying without Unreal

The sim-side logic of every call the adapter makes is checked host-free — compile any
`core/` translation unit and the same `win_bite`/`dig_column`/`hand_bite`/`haft().bite()`
calls behave identically; the findings above are orderings, asserted, never magnitudes.

## Not here yet (deliberately)

No Megascans/CC0 art, no sculpted valley heightfield. `#28`'s layout numbers are still
placeholders; dressing an unproven field is decoration before foundation. Prove the
seam first.
