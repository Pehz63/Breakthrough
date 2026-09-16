# Quest 3 Mixed Reality Breakthrough - Plan 1 (velvet-marmot)

Status: planning only, no implementation started. Branch: `vr`.

## Goal

A Meta Quest 3 mixed reality application that anchors a Breakthrough board to a
real table in the user's room, lets the user move pieces with their hands, and
plays against the project's existing C++ engine.

## Context and constraints

- Developer has no prior Unity or VR experience. This is the dominant risk.
- Course project with a due date, so every stage must be independently
  demoable and the later stages must be droppable without leaving the build
  broken.
- Secondary goal is breadth of exposure across Unity and the Meta XR feature
  set, traded away for the deadline when the two conflict.
- Target hardware: Meta Quest 3 (Snapdragon XR2 Gen 2, Horizon OS). Quest 3 is
  required, not incidental: the color passthrough, the Depth API, and the
  Passthrough Camera API used in the later stages are Quest 3 / 3S only.

## Scope decision (2026-09-16)

Chosen: Track A as the project, with two stretch stages appended.

| Stage | Deliverable | Droppable |
|---|---|---|
| 0 | Toolchain, hello-world build running on the device | No |
| 1 | Passthrough, pinch-to-place a virtual board, MRUK table snap | No |
| 2 | Full rules, grabbable pieces, hot-seat human vs human, pressable UI | No, this is the MVP |
| 3 | AI opponent from the project engine, plus hint display | Degradable, not droppable |
| 4 | Manual alignment to a physical board, tap-to-enter position, move arrows | Yes |
| 5 | Passthrough Camera API board reading (computer vision) | Yes |

The cut line sits after stage 2. A build that stops there is still a complete
mixed reality application. Stage 3 degrades rather than drops: if the engine
integration stalls, a weak C# evaluator still produces an opponent.

## Decisions resolved (2026-09-16)

Three questions were open when the stage table above was drafted. All three are
now answered, and the answers change the plan materially.

**Schedule: 6 to 12 weeks.** Stages 0 through 4 are the planned deliverable.
Stage 5 is attempted only if 0 through 4 are complete with time remaining, and
is still expected to be the stage that gets cut. Breadth detours are budgeted
deliberately rather than taken as slack fillers, with the Depth API insert
after stage 1 as the first one.

**The demo may rely on a PC on the LAN.** This is the single largest risk
reduction available to the project. Route B (engine as a LAN server) becomes
the SHIPPED answer for stage 3 rather than a fallback, which means the native
Android plugin is no longer on the critical path at all. It is reclassified
from a required port to an optional learning exercise, attempted after stage 4
if the schedule allows. A second benefit: the engine binary serving the demo is
byte-identical to the one the Elo ladder rated, so any strength claim made
about the opponent transfers without qualification.

**A physical board with 32 pieces is obtainable.** Stages 4 and 5 stay in
scope and the piece-count row drops out of the blocking risks. See "Physical
set" below for what to actually acquire, since the choice has direct
consequences for stage 5's difficulty.

Net effect: the riskiest engineering task (cross-compiling the engine to ARM64)
left the critical path, and the two stretch stages both survived. Effort freed
from the port should go to stage 2 polish and stage 4, not to starting stage 5
early.

## Physical set

Stages 4 and 5 both need an 8x8 board and 16 pieces per side. The cheapest
option that is also the best option for stage 5 is a printed board rather than
a bought one:

- A large-format matte print of an 8x8 grid, with ArUco fiducial markers at the
  four corners. Matte matters, since a glossy board produces specular
  highlights under room lighting that break per-cell classification. The
  fiducials give stage 5 a robust pose without depending on detecting the board
  outline, which is the single largest source of CV risk.
- Dark squares that are NOT black. Green, deep red, or mid brown all preserve
  contrast against a black piece. A black disk on a black square is the
  low-contrast case that makes stage 5 hard, and it is avoidable for free at
  this step.
- Large squares, on the order of 2 inches or more, so each cell occupies more
  camera pixels at a natural seated viewing distance.
- 32 plain plastic discs or poker chips in two colors, with matte tops.

A bought checkers set works for stage 4 (which needs no vision at all) as long
as two sets are combined to reach 16 per side, but it is a worse substrate for
stage 5 on every axis above.

## Stage detail

### Stage 0 - Toolchain

Nothing else starts until a trivial scene builds and runs on the headset.

- Unity 6 LTS via Unity Hub, with the Android Build Support module including
  the embedded SDK and NDK. Pin the exact Unity version and the exact Meta XR
  SDK version in the repo and do not float them mid-project. Meta SDK version
  churn is a known source of broken tutorials.
- Meta XR All-in-One SDK (Meta XR Core, Interaction SDK, MRUK).
- Meta Horizon developer account with organization verification, Developer
  Mode enabled through the Horizon mobile app, device authorized over `adb`.
- Meta Quest Developer Hub (MQDH) for device management, casting, and log
  capture.
- Set up ONE fast iteration path before writing app code, either Quest Link
  (run in the Editor with the headset as display) or the Meta XR Simulator
  (synthetic headset and synthetic room, no hardware needed). This single
  decision has more effect on total project time than any other in stage 0.

Exit criteria: a cube renders over passthrough on the device, and the same
scene runs in the Editor through Link or the Simulator.

### Stage 1 - Surface and placement

- Passthrough enabled via the Building Blocks window.
- Pinch-to-place: raycast from the hand, drop the board anchor where the ray
  meets a surface. Build this FIRST. It works without any room scan and is the
  permanent fallback.
- MRUK table snap layered on top: query the room model for an anchor with the
  `TABLE` semantic label, align the board to its plane, size the board to fit
  within the plane bounds.
- Board scale and rotation handles so the user can adjust after placement.

Design note: MRUK reads the room model captured during the user's Space Setup
scan. It is not live plane detection. If the table was not captured, there is
no table anchor, which is exactly why the pinch-to-place fallback is built
first rather than last.

Exit criteria: a board sits flat on a real table, stays put when the user
walks around it, and can be placed manually in a room with no scan.

### Stage 2 - Rules and interaction (the MVP)

- 32 piece objects, 16 per side, matching `boards/board1.txt`.
- Grab and release with the Interaction SDK. Snap to the nearest legal square
  on release, with an illegal release animating back to the origin square.
- Legal move highlighting on grab.
- Turn enforcement, capture handling, win detection (reach the far row, or
  capture every enemy piece).
- A pressable UI panel: new game, undo, difficulty, and a hint button. This is
  the interactable UI element the developer wanted, via the Interaction SDK
  poke interactor on a world-space canvas.

Rules logic at this stage is C# regardless of the stage 3 route, because the
UI needs synchronous legality checks every frame. `gui/gui_engine.h`'s
`guiIsLegal` / `guiLegalMoves` / `guiApplyMove` are the reference semantics to
port. They are pure functions over a position struct and are small.

Exit criteria: two people can play a complete game of Breakthrough on a real
table through the headset.

### Stage 3 - The engine opponent

Route B (engine as a LAN server) is the shipped answer, settled 2026-09-16.
See "Engine integration" below for both routes and why the native port left the
critical path.

- Agent selection from a small curated list, mirroring `gui/presets.txt`
  (Easy / Medium / Hard).
- Search runs off the Unity main thread in all cases. A search on the main
  thread drops frames, and dropped frames in a headset are far more punishing
  than on a monitor.
- Hint display: ask the engine for its preferred move in the human's position
  and render it as an arc or arrow between squares.

Exit criteria: a human can lose to the Hard preset without the frame rate
dipping.

### Stage 4 - Manual alignment to a physical board (stretch)

No computer vision. The user grabs two corner handles and drags them onto the
two opposite corners of their real board, which defines the mapping by hand.
Then they tap squares to enter the physical position, and the engine's
recommended move is drawn over the real board.

This delivers the experience the developer described in the original second
idea while carrying none of its risk.

Physical set note: Breakthrough needs 16 pieces per side, confirmed against
`boards/board1.txt`, while a standard checkers or draughts set has 12. See the
"Physical set" section above for what to acquire and why the choice matters
more to stage 5 than to this stage.

### Stage 5 - Camera board reading (stretch)

Passthrough Camera API, available from Horizon OS v74 on Quest 3 and 3S,
surfaces camera frames to Unity as a `WebCamTexture` with pose and intrinsics.
Requires the `horizonos.permission.HEADSET_CAMERA` permission and user
consent. Verify the current API state before starting, since it has changed
repeatedly since introduction.

Pipeline: detect the board quadrilateral, compute a homography to a canonical
top-down 8x8 grid, classify each of the 64 cells as empty / white / black.

Known hazards, each of which is its own sub-project:
- The API does not work in the Editor over Link. Every iteration is a full
  build and deploy, which makes this the slowest stage by a wide margin.
- `findChessboardCorners` targets an empty calibration pattern and degrades
  badly on a board covered in pieces. Plan for ArUco markers at the board
  corners instead, which also gives a robust pose.
- A black disk on a black square is a low-contrast classification problem, and
  passthrough camera imagery is not high quality. Consider a board whose dark
  squares are not black, or a small learned classifier via Unity Sentis rather
  than luminance thresholding.
- Debugging vision inside a headset is painful. Build a frame-dump path to
  `persistentDataPath` and analyze on the PC.

## Engine integration

The repo is in better shape for this port than expected. Findings from the
2026-09-16 survey:

- The x86 SIMD paths in `src/ml_eval.cpp` and `src/ml_model.cpp` are guarded
  by `#if defined(__AVX2__)` with scalar fallbacks, so an ARM64 target compiles
  without touching them.
- `build_web.sh` already compiles the play-side engine under Emscripten clang
  for a non-x86 target. This is strong evidence that an Android NDK clang build
  of the same file list will work.
- `windows.h` appears only in `ml_train.cpp`, `ranking.cpp`, and
  `train_budget.cpp`. The play-only link set avoids the first and third.
  `ranking.cpp` is in the web link set, so audit what the GUI actually pulls
  from it before assuming it drops out of an Android build.
- `gui/gui_engine.h` already documents and implements the exact contract a
  Unity native plugin needs: engine state lives in process globals, exactly one
  thread may touch them, every computation is a job carrying a copy of the
  position, results are published for the caller to poll. A Unity plugin is
  that design with raylib removed and a flat C API added.
- The strongest curated preset is
  `ab(deep=6,tt,ord,nodes=200k)@3.learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1`,
  a 129-weight linear model on the order of a couple of KB. The 2 MB NNUE
  models in `models/` are not needed for the shipping opponent, so APK size is
  not a constraint.

### Route A - Native Android plugin

Build the play-side sources as an ARM64 `.so` with the Android NDK, expose a
flat `extern "C"` API, and P/Invoke it from C#.

Pro: full engine strength, offline, standalone, shippable.

Cost and gotchas:
- NDK build setup and P/Invoke marshalling are both new skills.
- Android `StreamingAssets` live inside the APK and cannot be opened with
  `ifstream`. Model and board files must be read through `UnityWebRequest` or
  unpacked to `persistentDataPath` on first launch.
- A stale `.so` does not error at build time, matching the same hazard the root
  `CLAUDE.md` already documents for the four MSVC link targets.
- Threading discipline must be preserved across the plugin boundary.

### Route B - LAN server

Run the engine as a process on the PC and have the headset talk to it over
WebSocket or HTTP on the local network.

Pro: no porting at all, fastest path to a playable opponent, full desktop
compute, and the engine binary stays exactly the one the Elo ladder rated.

Con: requires the PC to be on and on the same network, is not a shippable
standalone app, and adds latency. Acceptable for a course demo where the
developer controls the demo environment, not acceptable for distribution.

### Chosen sequencing (settled 2026-09-16)

Route B is the shipped answer. The demo is permitted to rely on a PC on the
LAN, which removes the only requirement that would have forced the native port.

Route A is reclassified as an optional learning exercise, attempted after stage
4 and only if the schedule allows. If it lands, the server becomes a
development convenience and the app gains the ability to run standalone. If it
never happens, nothing in the deliverable is missing.

This ordering is not a compromise. Route B keeps the demo running the exact
engine binary the Elo ladder rated, so a claim like "the Hard preset is the
strongest rated agent at 1227 in the 2026-09-06 fit" transfers to the demo
without needing to re-rate a recompiled ARM64 build. A native port would
require re-measuring before any such claim could be repeated, since a different
compiler and instruction set can change search behavior at a node budget.

Practical requirements for Route B:
- A defined wire protocol carrying a position and returning a move. The
  position format should be the engine's own board text (see `boards/`) plus
  side to move, so the server side is thin.
- The agent is selected by canonical ID string, so the Easy / Medium / Hard
  presets are exactly the `gui/presets.txt` lines rather than a reimplementation
  of them.
- The headset must behave when the server is unreachable. A visible connection
  state in the UI panel, and a fallback to the stage 2 C# evaluator, so the
  demo degrades instead of hanging.
- Demo-day hazard: campus and venue Wi-Fi frequently blocks peer-to-peer
  traffic between clients. Test on the actual demo network well before the
  deadline, and carry a travel router or a phone hotspot as a backup.

### On-device performance

Quest 3 is a mobile SoC with thermal limits, no AVX, and a render loop already
paying for passthrough. The `nodes=200k` budget in the curated presets is
calibrated against a desktop core and should not be assumed to transfer.
Measure us/node on-device early and set the budget from that measurement. The
project's existing `time=150ms` compute-normalization track is the appropriate
knob, since a wall-clock budget self-adjusts to whatever the hardware delivers
while a node budget does not.

The on-device us/node figure is worth recording as a measurement in its own
right, since it extends the project's existing cross-platform speed work to a
new architecture.

## Learning breadth map

Which Unity and Meta XR concepts each stage exercises, so that stages dropped
for the deadline can be traded against learning value rather than guessed at.

| Stage | Unity concepts | Meta XR concepts |
|---|---|---|
| 0 | Editor, project settings, Android build pipeline | SDK install, Building Blocks, device deploy, XR Simulator |
| 1 | Transforms, raycasting, prefabs | Passthrough, MRUK scene anchors, semantic labels, spatial anchors |
| 2 | GameObject lifecycle, physics or snapping, world-space UI, C# game logic | Interaction SDK: hand grab, poke interactor, ray interactor, hand tracking |
| 3 | Threading, async, native interop or networking, `StreamingAssets` | Frame budget and performance discipline in XR |
| 4 | Coordinate frames, plane mapping, user calibration UX | Anchor persistence across sessions |
| 5 | Texture processing, Sentis or OpenCV interop, permissions | Passthrough Camera API, camera intrinsics and pose |

Stage 3's native plugin route is the highest learning value per unit of time
outside of stage 2, because native interop generalizes well beyond this
project. It is now optional rather than required, which makes it the natural
place to spend schedule slack once stage 4 is done.

The Depth API for real-object occlusion is a cheap add at any point after stage
1 and is the largest visual upgrade per unit of effort available for a demo. It
is the first breadth detour to take, ahead of anything in stage 5.

## Repository layout question (open)

The Unity project generates a large `Library/` directory and many binary
assets. This repo already carries large match stores and a 95 MiB pre-commit
size guard. Two options, not yet decided:

1. A `xr/` subfolder on this branch with a Unity `.gitignore` layered in.
2. A separate repository that consumes the engine as a submodule or a copy of
   `src/`.

Option 1 keeps the engine and the client in one history, which matters if the
native plugin route is taken. Option 2 keeps this repo's history clean for the
research work.

## Risk register

| Risk | Severity | Mitigation |
|---|---|---|
| First Unity project, learning curve dominates | High | Stage 0 is a hard gate. Fast iteration path before any app code |
| Android build pipeline friction | High | Budget the whole first session for it and nothing else |
| Meta SDK version churn breaks tutorials | Medium | Pin Unity and Meta XR SDK versions, record them in the repo |
| Table not in the user's room scan | Medium | Pinch-to-place fallback built before the MRUK path |
| Demo-network Wi-Fi blocks headset-to-PC traffic | Medium | Test on the real demo network early, carry a travel router or hotspot |
| Stage 5 consumes the schedule | High | Stage 5 is the designated cut, and stage 4 delivers its user-facing value without it |

Retired risks, kept for the record:

- *Engine will not cross-compile to ARM64.* Off the critical path as of
  2026-09-16, since Route B is the shipped answer and the native port is
  optional.
- *On-device search too slow at desktop budgets.* Same reason. It returns if
  Route A is attempted, and the mitigation is a wall-clock budget rather than a
  node budget, set from an on-device measurement.
- *Physical set has 12 pieces per side, not 16.* Resolved, a set can be
  obtained. See "Physical set".

## Open questions

All three scope questions were resolved on 2026-09-16, see "Decisions
resolved". Remaining:

- Repository layout. Provisionally option 1, an `xr/` subfolder on this branch
  with a Unity `.gitignore` layered in, on the grounds that it keeps the engine
  and the client in one history. Cheap to reverse until Unity files exist.
- The wire protocol for Route B, to be specified before stage 3 starts.
- Whether the on-device us/node measurement is worth taking even though Route A
  is optional. It is a real datapoint for the research side and needs only a
  throwaway NDK build rather than a full plugin, so it may be worth doing
  independently of whether the plugin ever ships.

## Ideas this inspired

- The on-device us/node measurement extends the project's speed work to ARM64
  and is a legitimate datapoint for the research side, not just a port detail.
- A native plugin build would give the project a third non-MSVC target
  alongside Emscripten, which would catch portability regressions earlier.
- Physical-board move suggestion is a different interaction model from the
  existing GUI and might surface evaluator explanations that a 2D board does
  not, for example rendering the eval bar as a volume above the table.
- If anchor persistence works well, a board left on a table across sessions is
  a stronger demo than one placed fresh each launch.
