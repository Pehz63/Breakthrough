# Todo

- ~~Make unit tests~~
- Keep optimizing
- Improve board state evaluator with machine learning
- ~~Make a GUI~~ (raylib + raygui desktop GUI; web/WASM build supported, see INSTALL.md)
- Display the board state evaluation for each AI in the main board area
  - For tree search or other algorithms (like minimax), show both immediate evaluation and the AI's predicted downstream evaluation
- Display whose turn it is in the main board area
- ~~Board state evaluator selector for heuristic, NN, or other BSEFs~~
- Depth time budget for minimax (so I specify 10 seconds per move and it will stop calculating after going deep enough to do ~10 seconds)
- Parameter study for classic board state evaluator (for ~3, ~10, ~30 second budgets per move)
- Hyperparameter study for machine learning board state evaluator