## Swindle

> "It's so bad it's not even funny!" -kantiankant, 2026

Swindle started as a fork of [dwl](https://codeberg.org/dwl/dwl) with a
simple idea: make something that feels like Hyprland for people who don't
want to run Hyprland. Honest status report:

* NO cool vfx on main (yet)
* only ONE tiling layout (dwindle)
* Lua config that actually hot-reloads
* dwl IPC plus ext-workspace support
* eye candy (corners, shadows, fade-in) on the `scenefx` branch

> Fun fact: the ext-workspace bits were borrowed from
> [MangoWM](https://github.com/mangowm/mango), which is why
> `licenses/LICENSE.mangowm` exists.

## What you get

* **Dwindle tiling.** A binary-split tree in the Hyprland style, with gaps
  and optional smart gaps.
* **Lua config with hot-reload.** Save `config.lua` and appearance,
  keybinds, keyboard layout and input pick it up without a restart.
  (Only `autostart` runs once at startup, as the name suggests.)
* **Touchpad and mouse stay separate.** Different scroll methods and
  natural scrolling per device class, so your trackpad and your mouse can
  finally disagree in peace.
* **Keyboard layout in the config.** `layout`, `variant`, `options`,
  `model`, applied live.
* **Sane config lookup.** `-c` flag first, then your user config, then a
  system fallback. More on that below.
* **An IPC client called `smsg`** (see `smsg/README.md`) for status bars
  and little scripts.
* XWayland support, on by default, removable at compile time.

## Dependencies

| Library | Notes |
|---|---|
| `wlroots-0.20` | needs the libinput backend |
| `wayland` | also ships `wayland-scanner` for the build |
| `wayland-protocols` | build time only |
| `libinput` | |
| `libxkbcommon` | |
| `lua5.4` | the config engine |
| `libxcb`, `xcb-util-wm` | only for XWayland |
| `pkg-config` | build time only |
| `scenefx-0.5` | only for `SCENEFX=1` builds (eye candy) |

If your distro ships separate `-dev`/`-devel` packages, grab those too.

**CachyOS / Arch** (this is tested and works):

```
sudo pacman -S base-devel wlroots0.20 wayland wayland-protocols \
  libinput libxkbcommon lua54 libxcb xcb-util-wm pixman libdrm seatd
```

Eye candy needs one more package (`SCENEFX=1` builds only):

```
sudo pacman -S scenefx0.5
```

**Nix** (there is a flake in the repo):

```
nix build
nix run
```

## Building & installing

```
make                        # builds ./swindle and ./smsg/smsg
sudo make install           # puts them in /usr/local/bin, example config in /etc/swindle
sudo make uninstall         # removes both again
```

`PREFIX` and `DESTDIR` in `config.mk` are respected, so packaging or a
user-local install works fine:

```
make PREFIX="$HOME/.local"  # then copy the binaries into ~/.local/bin yourself
```

Heads up: `make install` copies `example/*` to `/etc/swindle/`. Your own
`~/.config/swindle/config.lua` always wins over that file (details below).

## Running

```
swindle [-v] [-d] [-s startup command] [-c config file]
```

| Flag | What it does |
|---|---|
| `-c PATH` | use this config file, skip the lookup chain |
| `-s CMD` | run one extra startup command |
| `-d` | debug logging, overrides whatever the config says |
| `-v` | print the version and exit |

You need `XDG_RUNTIME_DIR` set. Systemd usually handles that for you.

* **From a login manager,** drop in a session file that points at your
  binary, for example `~/.local/share/wayland-sessions/swindle.desktop`:

  ```
  [Desktop Entry]
  Name=Swindle
  Exec=/home/you/.local/bin/swindle
  Type=Application
  DesktopNames=swindle;wlroots;
  ```

* **From a TTY,** just run `swindle`. On systemd machines logind takes
  care of seats. Without logind you want `seatd` running and your user in
  the `seat`, `video` and `input` groups.

## Configuration

Copy `example/config.lua` somewhere and make it yours. It documents every
option. When swindle starts it looks for a config in this order and takes
the first readable file it finds:

1. `swindle -c /path/to/config.lua`
2. `$XDG_CONFIG_HOME/swindle/config.lua`, falling back to
   `~/.config/swindle/config.lua`
3. `/etc/swindle/config.lua` (the system fallback)

If none of those exist, swindle tells you so and quits. Everything below
hot-reloads when you save, except `autostart`, which only runs at startup.

### appearance

```lua
appearance = {
    border_px    = 2,          -- shorthand for inner+outer border width
    gaps         = 10,         -- 0 means no gaps
    smart_gaps   = false,      -- no gaps when a single window is visible
    root_color   = 0x222222ff, -- colors are 0xRRGGBBAA
    border_color = 0x00000000, -- unfocused (shorthand for inner+outer)
    focus_color  = 0xe8e8e8ff, -- focused (shorthand for inner+outer)
    urgent_color = 0xff0000ff, -- urgent (shorthand for inner+outer)
    fullscreen_bg = 0x000000ff,
}
```

If you want finer control, the explicit `inner_border_px`,
`outer_border_px` and `inner_*_color` / `outer_*_color` keys beat the
shorthands.

### animations (needs a scenefx build)

```lua
animations = {
    enabled           = true,
    corner_radius     = 12,
    fade_in_ms        = 150,
    shadow_enabled    = true,
    shadow_blur_sigma = 20.0,
    shadow_color      = 0x000000a0,
}
```

Rounded corners, drop shadows and a short fade-in on map, through
[scenefx](https://github.com/wlrfx/scenefx) 0.5. Build with `SCENEFX=1`
in `config.mk` (this branch default) and install the `scenefx-0.5`
package, e.g. `scenefx0.5` on CachyOS/Arch. Plain `SCENEFX=0` builds
parse the table but ignore it. Window blur is still to come.

### input

```lua
input = {
    repeat_rate = 50, repeat_delay = 150,
    tap_to_click = true, tap_and_drag = true, drag_lock = true,
    natural_scrolling = false,          -- fallback for both device classes
    natural_scrolling_touchpad = nil,   -- true/false, or nil for fallback
    natural_scrolling_mouse    = nil,
    disable_while_typing = true,
    left_handed = false, middle_button_emulation = false,
    scroll_method = "2fg",              -- fallback: "2fg", "edge", "button", "none"
    scroll_method_touchpad = nil,       -- per-class override, nil for fallback
    scroll_method_mouse    = nil,
    click_method  = "button_areas",     -- or "clickfinger"
    accel_profile = "adaptive",         -- or "flat"
    accel_speed   = 0.0,
    send_events_mode = "enabled",       -- "enabled", "disabled", "disabled_on_external"
    tap_button_map  = "lrm",            -- or "lmr"
}
```

Touchpads are spotted by their tap capability. If you configure a scroll
method a device does not support, swindle logs a warning and leaves the
device default alone instead of blowing up. Device settings kick in for
newly plugged hardware, while keyboard repeat and layout apply instantly.

### keyboard

```lua
keyboard = {
    layout  = "de",            -- "us", "us,de", whatever xkb knows
    variant = "",              -- e.g. "nodeadkeys"
    options = "caps:escape",   -- xkb options
    model   = "",              -- e.g. "pc105", empty means default
}
```

This one hot-reloads, so no restart dance for layout tweaks. If a layout
fails to compile, swindle keeps the old keymap and complains in the log.
(The older `input.xkb_layout` / `xkb_variant` / `xkb_options` /
`xkb_model` spellings still work.)

### rules

```lua
rules = {
    { app_id = "Gimp",    floating = true, monitor = -1 },
    { app_id = "firefox", tags = 1 << 8,   monitor = -1 },
}
```

`monitor = -1` means the current monitor. An empty `rules = {}` is
perfectly fine.

### monitors

```lua
monitors = {
    { name = nil, mfact = 0.55, nmaster = 1, scale = 1.0,
      layout = "dwindle", x = -1, y = -1 },
    -- { name = "eDP-1", scale = 2.0, layout = "dwindle", x = -1, y = -1 },
}
```

`name = nil` catches everything. `x`/`y = -1` means figure it out
automatically. If the table is missing or empty you get the catch-all
above.

### autostart

Commands run in order, once, at startup. Each gets about half a second;
daemons get noticed and skipped over:

```lua
autostart = {
    "dbus-update-activation-environment --systemd WAYLAND_DISPLAY XDG_SESSION_TYPE XDG_CURRENT_DESKTOP",
    "waybar",
}
```

Use `{}` for nothing. Saving the config never re-runs autostart.

### keybinds

```lua
keybinds = {
    { mods = {"logo"}, key = "q",     action = "spawn",   args = {"foot"} },
    { mods = {"logo"}, key = "space", action = "spawn",   args = {"wofi", "--show", "drun"} },
    { mods = {"logo"}, key = "w",     action = "killclient" },
    ...
}
for i = 1, 9 do ... end  -- tag keys, see the example config
```

Modifiers are `shift`, `ctrl`, `alt`, `logo`, `mod2`, `mod3`, `mod5`.
Keys are XKB keysym names like `"q"`, `"Return"` or `"XF86AudioMute"`.

| action | args | description |
|---|---|---|
| `spawn` | argv | run a program |
| `killclient` | | close the focused window |
| `togglefloating` | | float or unfloat the focused window |
| `togglefullscreen` | | fullscreen toggle |
| `togglegaps` | | gaps on or off (this monitor) |
| `togglesmartgaps` | | smart gaps on or off (global) |
| `setgaps` | `"+5"` / `"-5"` / `"10"` | relative or absolute gap size |
| `setmfact` | `"+0.05"` / `"-0.05"` / `"55"` | dwindle root split, relative delta or absolute percent |
| `zoom` | | swap the focused window with the first tiled one |
| `focusstack` | `"next"` / `"prev"` | cycle focus through tiled windows |
| `focusdir` / `swapdir` | `"left"`/`"right"`/`"up"`/`"down"` | directional focus / swap |
| `view` | mask or `"all"` | show tag(s), e.g. `tostring(1 << 0)` |
| `toggleview` | mask | toggle tag visibility |
| `tag` / `toggletag` | mask | (un)assign a window to tag(s) |
| `focusmon` / `tagmon` | `"left"` / `"right"` | focus or send to the adjacent monitor |
| `setlayout` | index | `0` is floating, `1` is dwindle |
| `moveresize` | `"move"` / `"resize"` | mouse drag action (see buttons) |
| `chvt` | number | switch virtual terminal |
| `quit` | | exit swindle |

### buttons

```lua
buttons = {
    { mods = {"logo"}, button = "left",   action = "moveresize", args = {"move"} },
    { mods = {"logo"}, button = "middle", action = "togglefloating" },
    { mods = {"logo"}, button = "right",  action = "moveresize", args = {"resize"} },
}
```

## IPC (`smsg`)

`make` also builds `smsg`, a little dwl-IPC client for bars and scripts:

```
smsg -w            # stream status, dwl-stdout style
smsg -w -c         # watch the focused appid/title
smsg -s -t 7       # select tag 8 on the current output
smsg -l '><>'      # switch to the floating layout
```

The full usage lives in [`smsg/README.md`](smsg/README.md).

## Troubleshooting

* **`failed to load config (tried '…' and '…')`**. No config anywhere in
  the lookup chain. Copy `example/config.lua` to
  `~/.config/swindle/config.lua` (or pass `-c`).
* **`XDG_RUNTIME_DIR must be set`**. Launch through systemd or a login
  manager, or export it yourself (`/run/user/$(id -u)`).
* **Wrong keyboard layout.** Set `keyboard.layout` (say `"de"`). It
  hot-reloads, no restart needed.
* **`Failed to bind socket @/tmp/.X11-unix/X0`**. Some other X server
  (probably your current desktop session) already owns it. Harmless
  unless you need XWayland inside swindle. Starting from a TTY avoids it.
* **No bar or launcher.** Swindle ships neither. Install `waybar` and
  `wofi` or `rofi`, then wire them into `autostart` and `keybinds`.
* **Sanity-check a config without starting:** `luac5.4 -p config.lua`
  catches syntax errors (logic errors are still on you).

## Roadmap

- [x] separate trackpad scroll from mouse scroll
- [x] configurable keymap
- [x] custom config path (`-c`)
- [x] `/etc/swindle/config.lua` fallback
- [x] animations via [scenefx](https://github.com/wlrfx/scenefx) 0.5
      (rounded corners, shadows, fade-in; lives on the `scenefx` branch,
      window blur still open)
- [ ] per-monitor wallpaper / output management helpers
- [ ] docs site (in planning)

## Contributing

Issues and PRs are welcome at <https://github.com/hakuxyz/swindle>.
A few ground rules:

* C11 and `-Wall -Wextra` clean. `make` should stay warning-free.
* Lua configs should pass `luac5.4 -p`.
* Keep the parser backwards compatible. New optional keys with `nil`
  meaning old behavior beat breaking renames.
* Small, reviewable commits, one concern per PR.

## Credits

* [dwl](https://codeberg.org/dwl/dwl), the upstream this fork started from
* [MangoWM](https://github.com/mangowm/mango) for the ext-workspace code
  (see `licenses/`)
* [dwlmsg](https://codeberg.org/notchoc/dwlmsg), the base of `smsg`

## License

GPLv3 for swindle itself (see `LICENSE`). Third-party bits keep their own
terms, collected under `licenses/` (`LICENSE.dwl`, `LICENSE.mangowm`, …).
