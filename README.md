# snacky

Complete Snake Game in C++ (console/terminal, single file).

## File

- `/home/runner/work/snacky/snacky/snake_game.cpp`

## 1) Game logic

- The board is a configurable rectangle with walls (`#`).
- Snake starts at the center and moves every tick.
- Food (`*`) spawns at random free cells.
- Eating food increases score and grows snake by one segment.
- Game ends when snake hits wall or itself.
- After game over, player can restart (`R`) or quit (`Q`).

## 2) Main classes/functions

- `Point`: stores x/y coordinates.
- `Snake`: stores body segments, movement direction, and self-hit checks.
- `Console`: cursor movement (`gotoxy`), clear screen, cursor visibility.
- `Input`: non-blocking keyboard input (`_kbhit/_getch` on Windows).
- `Game`: main loop, input handling, update, rendering, food spawn, score/high-score, restart flow.

## 3) How movement works

- Controls:
  - `W` = up
  - `S` = down
  - `A` = left
  - `D` = right
  - `P` = pause
- Every frame, snake computes next head position from current direction.
- Opposite-direction instant reversal is blocked to keep movement valid.

## 4) Collision detection

- **Wall collision:** if next head cell is on/through border, game over.
- **Self collision:** after movement, if head matches any body segment, game over.
- **Food collision:** if next head equals food position, snake grows and score increments.

## 5) Compile and run

### g++ (Windows MinGW / Linux / WSL)

```bash
g++ -std=c++17 -O2 snake_game.cpp -o snake_game
./snake_game
```

### Code::Blocks / Visual Studio

- Create a C++ console project.
- Add `snake_game.cpp`.
- Build and run.

## Extra features included

- Difficulty levels: slow/medium/fast
- Pause/resume (`P`)
- High score saving (`highscore.txt`)
- Smooth cursor-based rendering to reduce flicker
