# Quest 3 Mixed Reality - Planning Conversation Transcript (velvet-marmot)

Session date: 2026-09-16. Branch: `vr`.
Companion to `plans/quest3-mr-plan-1-velvet-marmot.md`.

This is the discussion that produced the plan. The developer's messages and the
assistant's chat messages are reproduced as written. Tool activity (repository
inspection commands and their findings) is summarized rather than pasted, and
marked as such. Multiple-choice prompts are shown with the options offered and
the answer given.

---

## Developer

> I'd like to make a Mixed Reality project for my Meta Quest 3. I have a Unity
> student account. My idea for a project is to recognize a flat surface (table)
> and render a Breakthrough board onto it. Or possibly recognize a checkerboard
> game with black and white checker disks and read the board state and render
> recommended moves. I'd also like to add some interactable digital elements, if
> at least a UI element that I can press, but ideally allow me to interact with
> digital game pieces.
>
> Make a VR git branch.
>
> Discuss with me the viability of this project. I have not done development
> with VR or Unity in the past. Lets just plan and discuss for now, no coding
> work yet.

---

## Repository survey (tool activity, summarized)

Before answering, the assistant inspected the repository to ground the
integration discussion in what the engine actually is. Commands run and what
they established:

- `git status`, `git branch -a` - clean tree on `main`, no other branches.
- `ls -la src/`, line counts - roughly 22k lines across the engine sources,
  with `ranking.cpp` (406 KB) and `ml_train.cpp` (135 KB) dominating.
- `grep` for `windows.h`, `_MSC_VER`, `immintrin.h`, `__declspec`, SIMD
  intrinsics - the only Windows-specific code is in `ml_train.cpp`,
  `ranking.cpp`, and `train_budget.cpp`. `immintrin.h` appears in
  `src/ml_eval.cpp` and `src/ml_model.cpp`, both wrapped in
  `#if defined(__AVX2__)` with scalar fallbacks (7 and 10 intrinsic uses
  respectively).
- `cat src/agents.h` - the `AgentSpec` composition surface, `agentChooseMove`,
  the budget and toggle fields.
- `cat build_web.sh` - the Emscripten build, establishing that the play-side
  source list already compiles under clang for a non-x86 target.
- `head -70 gui/gui_engine.h` - the GUI's engine service contract: engine state
  lives in process globals, exactly one thread may touch them, every
  computation is a job carrying a copy of the position, results are published
  for the caller to poll. Plus the pure rule helpers `guiIsLegal`,
  `guiLegalMoves`, `guiApplyMove`, `guiWinner` over a `GuiPos` struct.
- `ls -S models/*.txt`, `cat gui/presets.txt` - the 2 MB files are
  `dist_mlp_nnue_*`, but the `hard` preset is
  `ab(deep=6,tt,ord,nodes=200k)@3.learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1`,
  a 129-weight linear model on the order of a couple of KB.
- `cat boards/board1.txt` with piece counts - 16 White and 16 Black, two full
  home rows each.
- `git checkout -b vr` - branch created.

---

## Assistant

I created the branch and dug through the repo to ground this in what your engine
actually is rather than guessing.

```
Switched to a new branch 'vr'
```

### Verdict up front

Your idea A (detect a table, render a Breakthrough board on it, touch the
pieces) is **solidly viable as a first Unity/VR project.** Most of what sounds
hard about it is a solved, packaged feature on Quest 3.

Your idea B (look at a physical checkerboard, read the position, overlay
recommended moves) is a **different class of project.** It is viable in
principle, but it is a computer vision research task wearing an MR costume, and
it has a specific blocker I will get to that you probably have not considered.

My recommendation is to build A as the real project and treat B as a stage 4
stretch goal that you are allowed to abandon.

### What is genuinely easy here

You are lucky in what you picked. "Recognize a flat table" is close to free on
Quest 3:

| Thing you want | How you get it | Difficulty |
|---|---|---|
| Find the table surface | MRUK (Mixed Reality Utility Kit, part of the Meta XR SDK) exposes the room model from the headset's Space Setup scan as labeled anchors. `TABLE` is a built-in semantic label, alongside `FLOOR`, `WALL_FACE`, `COUCH` | Low. This is a handful of lines |
| Passthrough background | One Building Block in the Meta XR SDK | Trivial |
| A UI panel you can poke | Interaction SDK poke interactor plus a curved canvas. There is a Building Block for this | Low |
| Grab and move a virtual piece | Interaction SDK `Grabbable` plus `HandGrabInteractable`. Also a Building Block | Low to medium |
| Hands or controllers | Both work, hand tracking is on by default on Quest 3 | Low |
| Real objects occluding virtual ones | Depth API, Quest 3 only | Medium, and optional |

Meta's "Building Blocks" window is the thing that makes this tractable for a
first-timer. You drag a block into the scene and get a working camera rig,
passthrough, hand tracking, or a grab interaction, pre-wired. The days when you
hand-assembled an `OVRCameraRig` are over.

The honest caveat on table detection: MRUK reads the room model the **user**
captured during Space Setup. It is not live plane detection. If you have not
scanned that table into your room setup, there is no table anchor. So every app
like this ships a fallback: "point and pinch to place the board." Build the
fallback first, add the automatic table snap second. That ordering also means
you can develop without the headset constantly re-scanning.

### Where the actual difficulty is

It is not the MR. For a first Unity project the pain is concentrated in three
unglamorous places:

1. **Unity itself.** The editor, prefabs, the component model, C# scripting
   lifecycle, why your script is null on frame one. This is the bulk of your
   learning curve and none of it is VR-specific.
2. **The Android build pipeline.** Meta developer account, organization
   verification, Developer Mode via the Horizon mobile app, Android Build
   Support plus the NDK/SDK modules in Unity Hub, keystore, `adb` over USB or
   Wi-Fi. Expect day one to be entirely this and nothing else. Meta Quest
   Developer Hub (MQDH) is worth installing for device management and casting.
3. **Iteration speed.** Deploy-to-device is slow. Mitigate with Quest Link (run
   in the Editor, headset as display) and the Meta XR Simulator (fakes a headset
   and a synthetic room with furniture, no hardware needed). Get one of these
   working early. It is the difference between a fun project and a miserable
   one.

Your Unity student account is fine. Unity Personal would also be fine. The
licensing is not a gate here. The Meta developer account is the gate, and it is
free.

### The checkerboard CV idea, honestly

Four separate problems stack up:

**1. Passthrough camera access exists now, but with strings.** Meta opened the
Passthrough Camera API in Horizon OS v74 (2025) for Quest 3 and 3S. Unity gets
frames as a `WebCamTexture` plus the camera pose and intrinsics, which is what
you need for pose estimation. Requires the `horizonos.permission.HEADSET_CAMERA`
permission and a user consent prompt. Critically, **it does not work in the
Editor over Link.** Every CV iteration is a full build and deploy. Verify the
current state of this API before committing, since Meta has been changing it
fast and my information has a cutoff.

**2. Your game does not fit on a checkers set.** This is the blocker. I checked
`boards/board1.txt`:

```
BBBBBBBB
BBBBBBBB
........
........
........
........
WWWWWWWW
WWWWWWWW
```

Breakthrough starts with **16 pieces per side**, filling two home rows. A
standard checkers or draughts set has **12 per side**. You cannot set up a
Breakthrough opening position with one checkers set. You would need two sets, or
a different piece type, or you accept a non-standard variant. Also, checkers
pieces are placed only on dark squares, so a Breakthrough position on a real
board puts pieces on light squares too, which is fine mechanically but breaks
any detector that assumes the checkers convention.

**3. The contrast problem is worse than it looks.** A black disk on a black
square. The classic `findChessboardCorners` from OpenCV is built for finding
inner corners of an *empty* calibration board and degrades badly when the board
is covered in pieces. You would end up doing: detect the board quadrilateral,
compute a homography to a canonical top-down 8x8 grid, then classify each of 64
cells as empty / white / black. Cell classification under passthrough camera
quality, at an angle, with your own hands moving through frame, in whatever
lighting your room has, is the part that eats weeks. Mitigations exist (ArUco
markers taped at the board corners for robust pose, a board with non-black dark
squares, a small trained classifier via Unity Sentis instead of thresholding),
but each is its own sub-project.

**4. You would be debugging CV inside a headset.** No breakpoints on a frame you
can inspect. You will want to dump frames to disk and analyze them on the PC.

**There is a middle path that gets you 80% of the payoff for 5% of the risk.**
Skip the CV entirely. Let the user align a virtual overlay to their physical
board by grabbing two corner handles, then tap squares to enter the physical
position by hand, or just play the whole game virtually on the real table. You
still get "recommended move arrows floating over a real table," which is the
actual experience you are describing. Add CV later only if the manual version is
finished and you still want to.

### Getting your engine onto the headset

This is where the repo pays off, and I found better news than I expected.

Your engine is in unusually good shape for this port:

- **The x86 SIMD is already guarded.** `src/ml_eval.cpp` and `src/ml_model.cpp`
  wrap `immintrin.h` in `#if defined(__AVX2__)` with scalar fallbacks. ARM64
  takes the scalar path and compiles clean.
- **You already build it with clang for a non-x86 target.** `build_web.sh`
  compiles the play-side engine under Emscripten. That is strong evidence an
  Android NDK clang build of the same file list will work with little fuss.
- **`windows.h` is confined to the parts you do not need.** The only
  Windows-specific code is in `ml_train.cpp`, `ranking.cpp`, and
  `train_budget.cpp`, which are trainer and ranker concerns. The play-only link
  set avoids them. (`ranking.cpp` is in the web link set, so check what the GUI
  pulls from it before assuming it drops out.)
- **`gui/gui_engine.h` is already the exact contract a Unity plugin needs.** It
  documents the "engine state lives in process globals, exactly one thread may
  touch it, every computation is a job carrying a copy of the position" design,
  with `GuiPos` as a pure UI-side position and pure rule helpers (`guiIsLegal`,
  `guiLegalMoves`, `guiApplyMove`). A Unity native plugin is that same file with
  raylib removed and a flat C API bolted on. You already solved the hard
  architectural question.
- **Your champion model is tiny.** The `hard` preset is
  `learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)`, a 129-weight linear
  model, on the order of a couple KB. There are 2 MB NNUE models in `models/`
  but the current strongest rated agent is not one of them. APK size is a
  non-issue.

Four options:

| Option | What it is | Pro | Con |
|---|---|---|---|
| **Native plugin (recommended)** | Build the play-side sources as an ARM64 `.so` with the Android NDK, expose a flat C API, P/Invoke from C# | Real engine, full strength, offline, standalone | You must learn NDK builds and marshalling. Model files need `StreamingAssets` handling, since Android assets sit inside the APK and cannot be `fopen`ed |
| **LAN server** | `rank.exe`-style process on your PC, WebSocket or HTTP, Quest is a thin client | Zero porting. Fastest path to a playable AI opponent. Full desktop compute | Needs your PC on. Not a shippable app. Latency |
| **Reimplement rules in C#** | Port only move generation and win detection, plus a weak eval | No native build at all. Fine for stage 2 | Throws away the engine and all your Elo work. Only sane as a temporary stand-in |
| **WebXR in Quest browser** | Extend the existing Emscripten build | Reuses the web build | Your web build is a raylib 2D canvas, not WebXR. No MRUK, no scene understanding. Wrong tool |

**Do the LAN server or the C# stand-in first so you have something playable,
then swap in the native plugin.** That way stage 3 does not block stage 2, and
you are never stuck.

The gotchas to expect on the native path: model and board file loading
(`StreamingAssets` on Android must be read via `UnityWebRequest` or unpacked to
`persistentDataPath` on first launch, plain `ifstream` will fail), and threading
(a search must never run on Unity's main thread or you drop frames and the
compositor will punish you visually).

**Performance reality.** Quest 3 is a Snapdragon XR2 Gen 2, an Android phone
chip with thermal limits, no AVX, and a render loop already spending budget on
passthrough. Your `nodes=200k` budget is calibrated on a desktop core. Expect
the per-node cost to be meaningfully higher and the sustained throughput lower
under thermal load. Your existing `time=150ms` compute-normalization track is
exactly the right knob here. Measure us/node on-device early and pick a budget
from the measurement rather than reusing the desktop one. This is also a
genuinely interesting datapoint for your actual research project.

### Where the Unity project should live

Worth deciding now. Unity projects generate a large `Library/` folder and a lot
of binary assets. This repo already carries big match stores and a 95 MiB
pre-commit guard. Two sane choices: a `xr/` subfolder here with a proper Unity
`.gitignore` layered on, or a separate repo that consumes the engine (submodule
or just a copied `src/`). The branch is made either way, so this is reversible.

### Suggested stages

```
0. Toolchain: Unity 6 LTS + Meta XR SDK, build "hello cube" to the device.
   Get XR Simulator or Link working. Nothing else until this is done.
1. Passthrough + place a board: pinch-to-place a virtual 8x8 board, then
   add the MRUK TABLE anchor snap on top.
2. Rules + interaction: 16 grabbable pieces per side, legal move highlights,
   turn logic, win detection. Human vs human, hot seat.
3. Opponent: LAN server or C# stand-in, then the native plugin. Add the UI
   panel (difficulty, new game, hint) as the pressable element.
4. Stretch: manual alignment to a physical board and tap-to-enter position,
   with move arrows overlaid. CV only after that works.
```

Stage 2 is your real MVP. If you ship only through stage 2, you have a credible
MR project.

Three things change the plan materially depending on your answer, so let me ask
rather than assume.

---

## Prompt 1 and answers

**Which track do you want to plan against?**

- Track A, virtual board only (Recommended)
- Track A, with B as stage 4 stretch
- Track B first, read a physical board
- Middle path, manual alignment

> Answer: Track A, with manual alignment as a stage 4 stretch, and Track B as a
> stage 5 stretch

**How should the engine reach the headset?**

- LAN server now, native plugin later (Recommended)
- Native Android plugin from the start
- C# reimplementation of the rules
- Undecided, lay out both in the plan

> Answer: Undecided, lay out both in the plan

**What is this project for, and is there a deadline?**

- Course project with a due date
- Personal project, open ended
- Portfolio or demo piece
- Learning exercise, VR and Unity

> Answer: It's a course project with a due date, but I also want it to be a
> learning experience. Recommend breadth across Unity and Meta XR features, but
> keep in mind I may drop them for the deadline.

---

## Assistant

*(Wrote `plans/quest3-mr-plan-1-velvet-marmot.md` and committed it as "Add the
Quest 3 mixed reality plan, staged with a cut line after the MVP".)*

Plan written and committed to the `vr` branch:
`plans/quest3-mr-plan-1-velvet-marmot.md`.

### What I locked in from your answers

Six stages with an explicit **cut line after stage 2**, so a deadline crunch
amputates cleanly instead of leaving you with a half-finished build:

| Stage | Deliverable | Droppable |
|---|---|---|
| 0 | Toolchain, hello-world on device | No |
| 1 | Passthrough, pinch-to-place board, MRUK table snap | No |
| 2 | Rules, grabbable pieces, hot-seat play, pressable UI | **No, this is the MVP** |
| 3 | Engine opponent + hints | Degradable, not droppable |
| 4 | Manual alignment to a physical board, move arrows | Yes |
| 5 | Camera board reading (CV) | Yes, expected to drop |

Stage 3 is marked *degradable* rather than droppable. Even if the engine
integration stalls entirely, a weak C# evaluator still gives you an opponent, so
you never demo a game with no AI in it.

The engine section lays out both routes with a **decision point after stage 2**
rather than now, and gives you criteria to decide with. I also recorded the
survey evidence behind it, because it changed my confidence a lot: the AVX2 code
is already `#if defined(__AVX2__)` guarded, `build_web.sh` proves your play-side
sources compile under clang for a non-x86 target, `windows.h` is confined to the
trainer and ranker, and `gui/gui_engine.h` is already the exact single-threaded
job-based contract a Unity plugin needs. The native port is a much better bet
than I would have guessed before looking.

### On breadth versus the deadline

You asked for breadth but reserved the right to cut. I added a **learning
breadth map** to the plan, a table of which Unity and Meta XR concepts each
stage teaches, so when you cut you are trading against a known quantity rather
than guessing.

Two things worth flagging from it:

- **The Depth API** (real objects occluding virtual ones, so your hand passes in
  front of a piece correctly) is a cheap insert at any point after stage 1 and
  is the single biggest visual upgrade per unit of effort for a demo. Good
  candidate if you have slack.
- **The native plugin route in stage 3** is the highest learning value outside
  stage 2, because native interop generalizes far beyond this project. If
  breadth matters to you and the schedule holds, that is where to spend it, not
  on stage 5.

The one place I would push back on breadth: **stage 0 is a hard gate and
deserves no creativity at all.** Get a cube rendering over passthrough on the
device, and get either Quest Link or the Meta XR Simulator working, before you
write a line of app logic. Iteration speed compounds through every later stage.

### Judgment calls I made

Repo layout: I am planning for an `xr/` subfolder on this branch with a Unity
`.gitignore` layered in, rather than a separate repo. It keeps the engine and
client in one history, which matters if you take the native plugin route. Easy
to reverse before any Unity files exist, so say the word if you would rather
split it.

### The one thing that is genuinely blocking

Stages 4 and 5 both need a physical board with **16 pieces per side**. I
confirmed against `boards/board1.txt` that Breakthrough fills two home rows. A
standard checkers set has 12 per side, so one set cannot represent an opening
position. This needs a physical answer (a second set, different pieces, or a
defined reduced-piece variant) before stage 4 starts, and it is in the risk
register as blocking.

Three open questions remain, and the due date is the one I need to place the cut
line.

---

## Prompt 2 and answers

**When is the course deadline?**

- Under 3 weeks
- 3 to 6 weeks
- 6 to 12 weeks
- More than 12 weeks, or a semester

> Answer: 6 to 12 weeks

**Must the demo run on the headset alone, with no PC present?**

- Yes, standalone required
- No, a PC on the LAN is fine
- Unsure, plan for the safer option

> Answer: No, a PC on the LAN is fine

**Can you get a physical board with 32 pieces?**

- Yes, I can get one
- I have a standard checkers set only
- No physical board
- Not sure yet

> Answer: Yes, I can get one

---

## Assistant

*(Revised the plan document and committed it as "Update the Quest 3 plan with
the three resolved scope decisions".)*

Plan updated and committed. Branch `vr`, two commits, no push.

### The consequential answer was the LAN one

Allowing a PC on the network removed the riskiest engineering task from the
project. Cross-compiling your C++ engine to ARM64 with the Android NDK was the
one item that could have eaten weeks with nothing to show, and it is now **off
the critical path entirely**, reclassified as an optional learning exercise
after stage 4.

There is a second benefit I did not anticipate until I wrote it out. Route B
serves moves from the *exact* engine binary your Elo ladder rated. That means a
claim like "the Hard preset is the strongest rated agent, 1227 in the 2026-09-06
fit" transfers to the demo with no qualification. A recompiled ARM64 build would
need re-measuring before you could repeat that, since a different compiler and
instruction set can change what a search does inside a node budget. For a course
demo where you will be explaining what the opponent is, that matters.

Retired two risk rows as a result. Added one: **campus and venue Wi-Fi commonly
blocks peer traffic between clients.** Test on the actual demo network well
before the deadline and have a travel router or phone hotspot as backup. This is
the new way the demo can fail on the day.

### With 6 to 12 weeks

Stages 0 through 4 are the deliverable. Stage 5 is the designated cut. Since you
asked for breadth, I ranked the detours rather than listing them:

1. **Depth API occlusion** after stage 1. Largest visual upgrade per unit of
   effort, and it makes the demo look convincing in a way nothing else does.
2. **The native plugin** after stage 4, now that it is optional. Native interop
   is the most transferable skill in the whole project.
3. Anything in stage 5, last.

### On the physical board

Since you can get one, my recommendation is to **print it rather than buy it.**
A large-format matte print of an 8x8 grid with ArUco fiducial markers at the
four corners, dark squares in green or deep red rather than black, and 32 matte
plastic discs. That costs almost nothing and eliminates most of stage 5's
difficulty before it starts: the fiducials give you a robust pose without
detecting the board outline, and non-black dark squares remove the
black-disk-on-black-square contrast problem. A bought checkers set is fine for
stage 4, which uses no vision at all, but it is worse on every axis for stage 5.

### Open items, all minor

Repo layout is provisionally an `xr/` subfolder on this branch. The Route B wire
protocol needs specifying before stage 3, and I would make it carry the engine's
own board text from `boards/` plus side to move, with the agent chosen by
canonical ID string so your Easy/Medium/Hard are literally the
`gui/presets.txt` lines.

The plan is settled enough to act on. When you are ready, stage 0 is the next
move, and it is entirely account setup, Unity Hub modules, and getting a cube on
the headset. I can walk you through that, draft the Route B protocol, or sketch
the C# position and rules types ported from `gui_engine.h`, whichever you want
first.

---

## Developer

> Also commit this conversation to a text file.

---

## Commits on this branch

```
09ebe89  Add the Quest 3 mixed reality plan, staged with a cut line after the MVP
8089ed4  Update the Quest 3 plan with the three resolved scope decisions
```

Nothing has been pushed.

---

## Caveats on this transcript

- Assistant chat messages are reproduced as written. Tool calls and their
  outputs are summarized in the "Repository survey" section rather than pasted
  verbatim.
- External-platform claims in this conversation (Meta XR SDK feature names,
  MRUK semantic labels, the Horizon OS v74 Passthrough Camera API and its
  permission string, Unity 6 LTS as the target) were stated from the assistant's
  training knowledge, not verified against live documentation in this session.
  Verify them before relying on any specific API name or version number, since
  the Meta XR SDK changes quickly.
- Claims about this repository (the `__AVX2__` guards, the `windows.h`
  distribution, the `gui_engine.h` contract, the 16-per-side starting position,
  the linear model behind the `hard` preset) were verified against the working
  tree on 2026-09-16 by the commands listed in the survey section.
