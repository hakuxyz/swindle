## Swindle

> "It's so bad it's not even funny!" -kantiankant, 2026


Swindle is a fork of [dwl](https://codeberg.org/dwl/dwl) that was designed from the start with one goal: to make it feel like the poor man's Hyprland. It has the following:

* NO cool vfx (yet)
* nearly NO documentation
* ZERO good original code
* only ONE tiling layout (dwindle)
 
> Fun fact: I actually took some parts of [MangoWM](https://github.com/mangowm/mango) (such as the ext-workspaces implementation). hence why the LICENSE.mangowm exists

## Dependencies

* libinput
* wayland
* wlroots-0.20 (compiled with the libinput backend)
* xkbcommon
* wayland-protocols (compile-time only)
* pkg-config (compile-time only)

Install these (and their `-devel` versions if your distro has separate
development packages) and run `make` followed by `doas/sudo make install`, if you wish to install it (installs to /usr/local/bin/ by default). 

## Things that are good to have

* [foot](https://codeberg.org/dnkl/foot)


## Known Issues

N/A

## Configuration

read example/config.lua. It should give you a basic idea of how configuring swindle works.

Config lookup order (first readable file wins):

1. `swindle -c /path/to/config.lua` (custom path)
2. `$XDG_CONFIG_HOME/swindle/config.lua` (or `~/.config/swindle/config.lua`)
3. `/etc/swindle/config.lua` (system fallback, installed by `make install`)

The config hot-reloads on save (inotify): appearance, keybinds,
keyboard layout and input (for newly plugged devices) apply without
restart. `autostart` runs only once at startup.

### Keymap

```lua
keyboard = { layout = "de", variant = "", options = "caps:escape", model = "" }
```

### Split touchpad/mouse scrolling

```lua
input = {
    scroll_method = "2fg",        -- fallback for both
    scroll_method_touchpad = "2fg", -- "2fg", "edge", "button", "none", nil
    scroll_method_mouse    = "edge",
    natural_scrolling = false,    -- fallback
    natural_scrolling_touchpad = true,
    natural_scrolling_mouse = false,
}
```

### New actions

`setmfact` (`{"+0.05"}`/`{"-0.05"}` relative, `{"55"}` absolute percent),
`zoom`, `focusstack` (`{"next"}`/`{"prev"}`), `setgaps`
(`{"+5"}`/`{"-5"}`/`{"10"}`), `togglesmartgaps`. See example/config.lua.

## Checklist

- [x] separate the trackpad scroll from the mouse scroll (`scroll_method_touchpad`/`scroll_method_mouse`, `natural_scrolling_touchpad`/`natural_scrolling_mouse`)
- [x] Add ability to change your keymap (`keyboard = {...}`, hot-reloads)
- [ ] Add animations via [scenefx](https://github.com/wlrfx/scenefx) when 0.5 drops (in another branch) — `animations` table is parsed as a stub until then
- [x] Add the ability for the compositor to read a custom path for configs (`swindle -c ...`, watched + documented)
- [x] Add /etc/swindle/config.lua as a fallback path if $HOME/.config/swindle/config.lua is not found


## License

GPL-v3, probably
