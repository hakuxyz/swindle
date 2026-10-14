sloppy_focus               = true
bypass_surface_visibility  = false
log_level                  = "error"  -- "silent", "error", "info", "debug"

appearance = {
    border_px    = 2,        -- legacy alias, maps to inner+outer (explicit inner_/outer_ win)
    gaps         = 10,       -- 0 = no gaps. simple as.
    smart_gaps   = false,

    root_color   = 0x222222ff,
    border_color = 0x00000000, -- legacy alias for inner+outer border
    focus_color  = 0xe8e8e8ff, -- legacy alias for inner+outer focus
    urgent_color = 0xff0000ff, -- legacy alias for inner+outer urgent

    fullscreen_bg = 0x000000ff,
}

-- Eye candy. Needs a SCENEFX=1 build (scenefx 0.5, wlroots-0.20).
-- Plain builds parse this table but ignore it.
animations = {
    enabled           = false,      -- master switch
    corner_radius     = 12,         -- rounded window corners in px
    fade_in_ms        = 150,        -- map fade-in, 0 = instant
    shadow_enabled    = true,       -- drop shadow behind windows
    shadow_blur_sigma = 20.0,       -- shadow softness
    shadow_color      = 0x000000a0, -- 0xRRGGBBAA
}


-- Note: the entire input section requires you to restart
-- the compositor once changed (except keymap, which hot-reloads)
input = {
    repeat_rate             = 50,
    repeat_delay            = 150,
    tap_to_click            = true,
    tap_and_drag            = true,
    drag_lock               = true,
    natural_scrolling       = false,      -- fallback for both devices
    natural_scrolling_touchpad = nil,     -- true/false or nil (= fallback)
    natural_scrolling_mouse    = nil,     -- true/false or nil (= fallback)
    disable_while_typing    = true,
    left_handed             = false,
    middle_button_emulation = false,
    scroll_method           = "2fg",      -- fallback: "2fg", "edge", "button", "none"
    scroll_method_touchpad  = nil,        -- override for touchpads (nil = fallback)
    scroll_method_mouse     = nil,        -- override for mice (nil = fallback)
    click_method            = "button_areas", -- "button_areas", "clickfinger"
    accel_profile           = "adaptive",   -- "adaptive", "flat"
    accel_speed             = 0.0,
}

-- Keyboard layout. Hot-reloads (no restart needed).
-- Examples: "us", "de", "us,de". variant e.g. "nodeadkeys",
-- options e.g. "caps:escape", model e.g. "pc105".
keyboard = {
    layout  = "us",
    variant = "",
    options = "",
    model   = "",
}

-- Cursor theme and size. Hot-reloads. Empty theme = system default.
-- Run `ls /usr/share/icons` to see installed themes.
cursor = {
    theme = "",
    size  = 24,
}

rules = {
--   { app_id = "Gimp",    floating = true,  monitor = -1 },
--   { app_id = "firefox", tags = 1 << 8,    floating = false, monitor = -1 },
}

monitors = {
    -- catch-all rule; name = nil means match anything
    { name = nil, mfact = 0.55, nmaster = 1, scale = 1.0,
      layout = "dwindle", x = -1, y = -1 },

    -- HiDPI laptop example:
    -- { name = "eDP-1", mfact = 0.5, nmaster = 1, scale = 2.0,
    --   layout = "dwindle", x = -1, y = -1 },
}

-- autostart: it starts stuff in sequence
-- (empty table = nothing started; hot-reload does NOT re-run it)
autostart = {
    "dbus-update-activation-environment --systemd WAYLAND_DISPLAY XDG_SESSION_TYPE XDG_CURRENT_DESKTOP",
    "systemctl --user import-environment WAYLAND_DISPLAY XDG_SESSION_TYPE XDG_CURRENT_DESKTOP",
    "waybar",
}


keybinds = {
    { mods = {"logo"},          key = "q",     action = "spawn",  args = {"alacritty"} },
    { mods = {"logo"},          key = "space", action = "spawn",  args = {"wofi", "--show", "drun"} },
    { mods = {"logo"},          key = "w",     action = "killclient" },
    { mods = {"logo"},          key = "v",     action = "togglefloating" },
    { mods = {"logo"},          key = "f",     action = "togglefullscreen" },
    { mods = {"logo"},          key = "g",     action = "togglegaps" },
    { mods = {"logo"},          key = "h",     action = "focusdir",  args = {"left"} },
    { mods = {"logo"},          key = "j",     action = "focusdir",  args = {"down"} },
    { mods = {"logo"},          key = "k",     action = "focusdir",  args = {"up"} },
    { mods = {"logo"},          key = "l",     action = "focusdir",  args = {"right"} },
    { mods = {"logo", "shift"}, key = "H",     action = "swapdir",   args = {"left"} },
    { mods = {"logo", "shift"}, key = "J",     action = "swapdir",   args = {"down"} },
    { mods = {"logo", "shift"}, key = "K",     action = "swapdir",   args = {"up"} },
    { mods = {"logo", "shift"}, key = "L",     action = "swapdir",   args = {"right"} },
    { mods = {"logo"},          key = "Tab",   action = "view" },
    { mods = {"logo"},          key = "0",     action = "view",      args = {"all"} },
    { mods = {"logo"},          key = "comma",  action = "focusmon", args = {"left"} },
    { mods = {"logo"},          key = "period", action = "focusmon", args = {"right"} },
    { mods = {"logo", "shift"}, key = "less",    action = "tagmon", args = {"left"} },
    { mods = {"logo", "shift"}, key = "greater", action = "tagmon", args = {"right"} },
    { mods = {"logo", "shift"}, key = "e",     action = "quit" },
    -- dwindle helpers: setmfact takes "+0.05"/"-0.05" (relative)
    -- or "55" (absolute percent); setgaps takes "+5"/"-5"/"10"
    { mods = {"logo"},          key = "i",     action = "setmfact",   args = {"+0.05"} },
    { mods = {"logo"},          key = "d",     action = "setmfact",   args = {"-0.05"} },
    { mods = {"logo", "shift"}, key = "m",     action = "zoom" },
    { mods = {"logo"},          key = "n",     action = "focusstack", args = {"next"} },
    { mods = {"logo"},          key = "p",     action = "focusstack", args = {"prev"} },
    { mods = {"logo", "shift"}, key = "g",     action = "togglesmartgaps" },
}

for i = 1, 9 do
    local key  = tostring(i)
    local mask = 1 << (i - 1)
    table.insert(keybinds, { mods = {"logo"},                    key = key, action = "view",      args = {tostring(mask)} })
    table.insert(keybinds, { mods = {"logo", "ctrl"},            key = key, action = "toggleview", args = {tostring(mask)} })
    table.insert(keybinds, { mods = {"logo", "shift"},           key = key, action = "tag",        args = {tostring(mask)} })
    table.insert(keybinds, { mods = {"logo", "ctrl", "shift"},   key = key, action = "toggletag",  args = {tostring(mask)} })
end

buttons = {
    { mods = {"logo"}, button = "left",   action = "moveresize",     args = {"move"} },
    { mods = {"logo"}, button = "middle", action = "togglefloating" },
    { mods = {"logo"}, button = "right",  action = "moveresize",     args = {"resize"} },
}
