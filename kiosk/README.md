# DOSBox Staging kiosk build

A fork of DOSBox Staging for a shared machine where people pick a game from a
launcher, play, and walk away. When a game is left alone for too long, DOSBox
restarts and the launcher comes back up.

## Running the example

```
cd kiosk-example
./run.sh
```

This starts RLoader with Doom (shareware). While a game runs, the image moves
to one side and a panel next to it shows the cover art, title and controls.

Leave it alone for 60 seconds and a 10 second "Back to menu" countdown appears
(the idle bar shows up 20 seconds before that). Any input cancels it. F12 goes
back to the menu straight away, and Ctrl+F9 quits.

## Settings

All in the `[kiosk]` section:

| Setting              | Default            |                                                     |
|----------------------|--------------------|-----------------------------------------------------|
| `kiosk_enabled`      | `off`              | Turn kiosk mode on                                  |
| `kiosk_menu_program` | `RLOADER UI`       | Programs that make up the launcher; never restarted |
| `kiosk_idle_timeout` | `60`               | Idle seconds before the countdown; `0` = never      |
| `kiosk_kick_warning` | `10`               | Length of the countdown                             |
| `kiosk_idle_display` | `20`               | Show the idle bar for this long before that         |
| `kiosk_overlay`      | `on`               | Show the side panel                                 |
| `kiosk_games_file`   | `kiosk-games.conf` | Per-game titles, images and controls                |
| `kiosk_menu_hint`    | `Press F12 ...`    | Text at the bottom of the panel                     |

F12 is the "Kiosk Menu" action in the key mapper (Ctrl+F1), where it can also
be bound to a gamepad button.

## Games file

```
[DOOM]
title    = Doom (shareware)
image    = images/doom.bmp
panel    = left
controls = Arrows: move / turn
controls = Ctrl: fire
controls = Alt+Arrows: strafe
```

Sections are program names without `.EXE`. `image` is a PNG or BMP relative to
the games file. `panel` is `left` (default) or `right`. In `controls`, the text
before the colon is drawn as keys: `+` separates them, and `Arrows`, `Up`,
`Down`, `Left` and `Right` become arrow keys.

Games need their own sound setup. The Doom in the example comes with a
`DEFAULT.CFG` for a Sound Blaster at port 220, IRQ 7, DMA 1 (the DOSBox
defaults).

## Building

Everything builds in Docker with the same toolchain as the official release:

```
kiosk/build/build.sh                 # -> build/release-linux/dosbox
kiosk/build/package.sh OFFICIAL EXAMPLE
kiosk/test/run.sh EXAMPLE screenshots
```

## Credits

- [DOSBox Staging](https://www.dosbox-staging.org/), GPL-2.0-or-later
- [RLoader](https://github.com/marco-sacchi/RLoader) by Marco Sacchi, MIT
- Doom shareware by id Software
