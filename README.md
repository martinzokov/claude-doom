# DOOM for Claude Code

Type `/doom` in Claude Code and play real DOOM in a pane, any time — while Claude works or not.

It runs [doomgeneric](https://github.com/ozkl/doomgeneric) (a portable build of the original engine) with [Freedoom](https://freedoom.github.io/) Phase 1 as the game data, drawn into the terminal with coloured half-block characters.

## Install

```sh
claude plugin marketplace add martinzokov/claude-doom
claude plugin install doom@zokov-mods
```

Then run `/reload-plugins` (or start a new session) and type `/doom`.

The first `/doom` builds the engine and downloads Freedoom, which takes a minute. You need a C compiler, `git`, `curl` and `unzip`. On a Mac, `xcode-select --install` gives you the compiler. Linux works; Windows needs WSL.

## Play

Click the line at the top of the pane so it takes your keys, then:

| Key | Action |
|---|---|
| WASD / arrows | move and turn |
| space | fire |
| f | use / open doors |
| q / e | strafe |
| 1–7 | weapons |
| m | menu |
| return | select |
| y | confirm |
| Esc | give the keyboard back to Claude Code |

Closing the pane quits the game. There's no sound.

## Tips

- The picture fills whatever room the pane gets. For a bigger, sharper picture, use Claude Code's fullscreen layout (the pane docks beside the transcript, floor to ceiling), drag the pane larger, or shrink your terminal's font.
- A terminal never says when a key is released, so a key lets go by itself shortly after you stop pressing it. Holding a key works, after a short stutter while your key repeat kicks in.

## How it works

- `engine/doomgeneric_term.c` is doomgeneric's platform layer: it scales each frame down to the pane's cells and prints it, and reads key presses from a small file.
- `hooks/register.tsx` registers `/doom`, runs the engine as a child process, and paints its frames into the pane.
- `hooks/keys.tsx` is the clickable row that captures your keys.
- `engine/build.sh` fetches and compiles doomgeneric and downloads Freedoom into `engine/bin/`.

## License

GPL-2.0, like doomgeneric and DOOM's source. Freedoom is under its own BSD-style license.
