## Swindle

> "It's so bad it's not even funny!" -kantiankant, 2026

Swindle is a fork of [dwl](https://codeberg.org/dwl/dwl) with one goal:
to feel like the poor man's Hyprland. It has the following:

* NO cool vfx (yet)
* only ONE tiling layout (dwindle)
* Lua configuration with live hot-reload
* dwl IPC plus ext-workspace support

> Fun fact: parts of the ext-workspace implementation were taken from
> [MangoWM](https://github.com/mangowm/mango), hence `licenses/LICENSE.mangowm`.

## Features

* **Dwindle tiling** — binary-split tree, Hyprland-style, with gaps and
  smart gaps
* **Lua config** (`config.lua`) with **hot-reload on save** — appearance,
  keybinds, keyboard layout and input apply without restarting
  (`autostart` runs once at startup)
* **Split touchpad/mouse input** — separate scroll methods and natural
  scrolling per device class
* **Configurable keyboard layout** (`layout`, `variant`, `options`, `model`)
* **Config lookup chain** — `-c` flag, user config, `/etc` fallback
* **IPC client** (`smsg`, see `smsg/README.md`) for status bars and scripting
* XWayland support (compile-time toggle)

## Dependencies

| Library | Notes |
|---|---|
| `wlroots-0.20` | compiled with the libinput backend |
| `wayland` | also provides `wayland-scanner` (build) |
| `wayland-protocols` | build-time only |
| `libinput` | |
| `libxkbcommon` | |
| `lua5.4` | config engine |
| `libxcb`, `xcb-util-wm` | XWayland support |
| `pkg-config` | build-time only |

Install the matching `-dev`/`-devel` packages if your distro splits them.

**CachyOS / Arch** (verified):

```
sudo pacman -S base-devel wlroots0.20 wayland wayland-protocols \
  libinput libxkbcommon lua54 libxcb xcb-util-wm pixman libdrm seatd
```

**Nix** (flake in-tree):

```
nix build
nix run
```

## Building & installing

```
make                        # builds ./swindle and ./smsg/smsg
sudo make install           # installs to /usr/local/bin, example config to /etc/swindle
sudo make uninstall         # removes both again
```

`PREFIX` and `DESTDIR` are respected (`config.mk`), so packaging and
user-local installs work:

```
make PREFIX="$HOME/.local"           # then copy the binaries to ~/.local/bin
```

> Note: `make install` copies `example/*` to `/etc/swindle/`. Your
> personal `~/.config/swindle/config.lua` always takes precedence (see below).

## Running

```
swindle [-v] [-d] [-s startup command] [-c config file]
```

| Flag | Meaning |
|---|---|
| `-c PATH` | use this config file instead of the lookup chain |
| `-s CMD` | run one extra startup command |
| `-d` | debug log level (overrides config) |
| `-v` | print version and exit |

`XDG_RUNTIME_DIR` must be set (systemd normally does this for you).

* **From a login manager:** install a session file pointing at your binary,
  e.g. `~/.local/share/wayland-sessions/swindle.desktop`:

  ```
  [Desktop Entry]
  Name=Swindle
  Exec=/home/you/.local/bin/swindle
  Type=Application
  DesktopNames=swindle;wlroots;
  ```

* **From a TTY:** just run `swindle`. On systemd systems logind handles
  seats; without logind you need `seatd` running and your user in the
  `seat`/`video`/`input` groups.

## Configuration

Start from `example/config.lua` — it documents every option. The lookup
order (first readable file wins) is:

1. `swindle -c /path/to/config.lua`
2. `$XDG_CONFIG_HOME/swindle/config.lua` (else `~/.config/swindle/config.lua`)
3. `/etc/swindle/config.lua` (system fallback)

Without any of these, swindle refuses to start. Only `autostart` is
startup-only; everything else below hot-reloads when you save the file.

### appearance

```lua
appearance = {
    border_px    = 2,          -- shorthand for inner+outer border width
    gaps         = 10,         -- 0 = no gaps
    smart_gaps   = false,      -- no gaps when only one window is visible
    root_color   = 0x222222ff, -- colors are 0xRRGGBBAA
    border_color = 0x00000000, -- unfocused (shorthand for inner+outer)
    focus_color  = 0xe8e8e8ff, -- focused   (shorthand for inner+outer)
    urgent_color = 0xff0000ff, -- urgent    (shorthand for inner+outer)
    fullscreen_bg = 0x000000ff,
}
```

Explicit `inner_border_px` / `outer_border_px` and
`inner_*_color` / `outer_*_color` keys win over the shorthands.

### animations (stub)

```lua
animations = { enabled = false, corner_radius = 0, fade_ms = 0 }
```

Parsed for forward compatibility, but inert: real animations wait on an
[scenefx](https://github.com/wlrfx/scenefx) release compatible with
wlroots 0.20. Enabling it just logs a note.

### input

```lua
input = {
    repeat_rate = 50, repeat_delay = 150,
    tap_to_click = true, tap_and_drag = true, drag_lock = true,
    natural_scrolling = false,          -- fallback for both device classes
    natural_scrolling_touchpad = nil,   -- true/false, or nil = fallback
    natural_scrolling_mouse    = nil,
    disable_while_typing = true,
    left_handed = false, middle_button_emulation = false,
    scroll_method = "2fg",              -- fallback: "2fg", "edge", "button", "none"
    scroll_method_touchpad = nil,       -- per-class override, nil = fallback
    scroll_method_mouse    = nil,
    click_method  = "button_areas",     -- or "clickfinger"
    accel_profile = "adaptive",         -- or "flat"
    accel_speed   = 0.0,
    send_events_mode = "enabled",       -- "enabled", "disabled", "disabled_on_external"
    tap_button_map  = "lrm",            -- or "lmr"
}
```

Touchpads are detected by tap capability; a configured scroll method the
device doesn't support is skipped with a warning instead of failing.
Input-device settings apply to newly plugged devices; keyboard repeat
and layout apply immediately.

### keyboard

```lua
keyboard = {
    layout  = "de",            -- "us", "us,de", ...
    variant = "",              -- e.g. "nodeadkeys"
    options = "caps:escape",   -- xkb options
    model   = "",              -- e.g. "pc105", empty = default
}
```

Hot-reloads. An uncompilable layout keeps the old keymap and logs an error.
(Legacy `input.xkb_layout` / `xkb_variant` / `xkb_options` / `xkb_model`
are still honored.)

### rules

```lua
rules = {
    { app_id = "Gimp",    floating = true, monitor = -1 },
    { app_id = "firefox", tags = 1 << 8,   monitor = -1 },
}
```

`monitor = -1` means current monitor. Empty `rules = {}` is fine.

### monitors

```lua
monitors = {
    { name = nil, mfact = 0.55, nmaster = 1, scale = 1.0,
      layout = "dwindle", x = -1, y = -1 },
    -- { name = "eDP-1", scale = 2.0, layout = "dwindle", x = -1, y = -1 },
}
```

`name = nil` is the catch-all. `x`/`y = -1` means autoconfigure.
A missing or empty table falls back to the catch-all above.

### autostart

Commands run **sequentially** once at startup (each gets ~0.5s; daemons
are detected and skipped over):

```lua
autostart = {
    "dbus-update-activation-environment --systemd WAYLAND_DISPLAY XDG_SESSION_TYPE XDG_CURRENT_DESKTOP",
    "waybar",
}
```

Use `{}` for nothing. Hot-reload never re-runs autostart.

### keybinds

```lua
keybinds = {
    { mods = {"logo"}, key = "q",     action = "spawn",   args = {"foot"} },
    { mods = {"logo"}, key = "space", action = "spawn",   args = {"wofi", "--show", "drun"} },
    { mods = {"logo"}, key = "w",     action = "killclient" },
    ...
}
for i = 1, 9 do ... end  -- tag keys, see example config
```

Modifiers: `shift`, `ctrl`, `alt`, `logo`, `mod2`, `mod3`, `mod5`.
Keys are XKB keysym names (`"q"`, `"Return"`, `"XF86AudioMute"`…).

| action | args | description |
|---|---|---|
| `spawn` | argv | run a program |
| `killclient` | – | close focused window |
| `togglefloating` | – | float/unfloat focused window |
| `togglefullscreen` | – | fullscreen toggle |
| `togglegaps` | – | gaps on/off (this monitor) |
| `togglesmartgaps` | – | smart gaps on/off (global) |
| `setgaps` | `"+5"` / `"-5"` / `"10"` | relative or absolute gap size |
| `setmfact` | `"+0.05"` / `"-0.05"` / `"55"` | dwindle root split, relative delta or absolute percent |
| `zoom` | – | swap focused window with first tiled one |
| `focusstack` | `"next"` / `"prev"` | cycle focus through tiled windows |
| `focusdir` / `swapdir` | `"left"`/`"right"`/`"up"`/`"down"` | directional focus / swap |
| `view` | mask or `"all"` | show tag(s), e.g. `tostring(1 << 0)` |
| `toggleview` | mask | toggle tag visibility |
| `tag` / `toggletag` | mask | (un)assign window to tag(s) |
| `focusmon` / `tagmon` | `"left"` / `"right"` | focus / send to adjacent monitor |
| `setlayout` | index | `0` = floating, `1` = dwindle |
| `moveresize` | `"move"` / `"resize"` | mouse drag action (see buttons) |
| `chvt` | number | switch virtual terminal |
| `quit` | – | exit swindle |

### buttons

```lua
buttons = {
    { mods = {"logo"}, button = "left",   action = "moveresize", args = {"move"} },
    { mods = {"logo"}, button = "middle", action = "togglefloating" },
    { mods = {"logo"}, button = "right",  action = "moveresize", args = {"resize"} },
}
```

## IPC (`smsg`)

`make` also builds `smsg`, a dwl-IPC client for bars and scripts:

```
smsg -w            # stream status (dwl-stdout style)
smsg -w -c         # watch focused appid/title
smsg -s -t 7       # select tag 8 on current output
smsg -l '><>'      # switch to floating layout
```

Full usage in [`smsg/README.md`](smsg/README.md).

## Troubleshooting

* **`failed to load config (tried '…' and '…')`** — no config found anywhere
  in the lookup chain. Copy `example/config.lua` to
  `~/.config/swindle/config.lua` (or pass `-c`).
* **`XDG_RUNTIME_DIR must be set`** — launch via systemd / login manager,
  or export it manually (`/run/user/$(id -u)`).
* **Wrong keyboard layout** — set `keyboard.layout` (e.g. `"de"`); it
  hot-reloads, no restart needed.
* **`Failed to bind socket @/tmp/.X11-unix/X0`** — another X server
  (e.g. your current desktop session) already owns it. Harmless unless
  you need XWayland inside swindle; start from a TTY to avoid it.
* **No bar/launcher** — swindle ships none. Install `waybar` and
  `wofi`/`rofi` and add them to `autostart`/`keybinds`.
* **Validate a config without starting:** `luac5.4 -p config.lua`
  catches syntax errors (not logic errors).

## Roadmap

- [x] separate trackpad scroll from mouse scroll
- [x] configurable keymap
- [x] custom config path (`-c`)
- [x] `/etc/swindle/config.lua` fallback
- [ ] animations via [scenefx](https://github.com/wlrfx/scenefx) once a
      wlroots-0.20-compatible release drops (stub is in place)
- [ ] per-monitor wallpaper / output management helpers
- [ ] docs site (in planning)

## Contributing

Issues and PRs welcome at <https://github.com/hakuxyz/swindle>.

* C11, `-Wall -Wextra` clean — `make` must stay warning-free.
* Lua configs must pass `luac5.4 -p`.
* Keep the config parser backwards compatible: prefer new optional keys
  with `nil` = old behavior over breaking renames.
* Small, reviewable commits; one concern per PR.

## Credits

* [dwl](https://codeberg.org/dwl/dwl) — the upstream this fork started from
* [MangoWM](https://github.com/mangowm/mango) — ext-workspace code
  (see `licenses/`)
* [dwlmsg](https://codeberg.org/notchoc/dwlmsg) — the base of `smsg`

## License

GPLv3 for swindle itself (see `LICENSE`); third-party bits keep their
own terms — see `licenses/` (`LICENSE.dwl`, `LICENSE.mangowm`, …).
