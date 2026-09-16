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
| 5 | Passthrough Camera API board reading (computer vision) | Yes, expected to be dropped |

The cut line sits after stage 2. A build that stops there is still a complete
mixed reality application. Stage 3 degrades rather than drops: if the engine
integration stalls, a weak C# evaluator still produces an opponent.

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

Two routes, both laid out below under "Engine integration". Decision deferred
until stage 2 lands.

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

Physical set note: Breakthrough needs 16 pieces per side. A standard checkers
or draughts set has 12 per side, so one set cannot represent a Breakthrough
opening position. Resolve this before stage 4 by acquiring a second set, using
a different piece type, or defining a reduced-piece variant and rating it
separately. This constraint applies to stage 5 equally.

### Stage 5 - Camera board reading (stretch, expected to be dropped)

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

### Recommended sequencing

Stand up Route B first so stage 3 never blocks on a toolchain problem, then
attempt Route A once the game itself is complete. If Route A lands, the server
becomes a development convenience. If it does not, the demo still works.

Decision criteria, evaluated after stage 2:
- If schedule pressure is high at that point, ship Route B and stop.
- If the NDK build produces a working `.so` within one focused attempt, take
  Route A.
- If the demo must run without a PC present, Route A is mandatory and should
  be started immediately rather than deferred.

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
project. The Depth API for real-object occlusion is a cheap add at any point
after stage 1 and is a strong visual upgrade for a demo, so it is a good
candidate to insert if the schedule allows.

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
| Engine will not cross-compile to ARM64 | Medium | Route B exists precisely for this. The Emscripten build is evidence against it |
| On-device search too slow at desktop budgets | Medium | Wall-clock budget instead of node budget, measured on-device |
| Stage 5 consumes the schedule | High | Stage 5 is explicitly expected to be dropped, and stage 4 delivers its user-facing value without it |
| Physical set has 12 pieces per side, not 16 | Blocking for stages 4 and 5 | Resolve before stage 4 starts |

## Open questions

- The course due date, needed to place the cut line on the stage table.
- Whether the demo must run standalone with no PC present, which forces the
  native plugin route.
- Whether a physical board and 32 pieces can be obtained, which gates stages 4
  and 5.
- Repository layout, per the section above.

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
