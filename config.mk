_VERSION = 0.1 
VERSION  = `git describe --tags --dirty 2>/dev/null || echo $(_VERSION)`

PKG_CONFIG = pkg-config

# paths
PREFIX = /usr/local
MANDIR = $(PREFIX)/share/man
DATDIR = $(PREFIX)/share

WLR_INCS = `$(PKG_CONFIG) --cflags wlroots-0.20`
WLR_LIBS = `$(PKG_CONFIG) --libs wlroots-0.20`

# Allow using an alternative wlroots installation
#WLR_INCS = -I/usr/include/pixman-1 -I/usr/include/elogind -I/usr/include/libdrm \
#	-I$(PWD)/wlroots/include
#WLR_LIBS = -Wl,-rpath,$(PWD)/wlroots/build -L$(PWD)/wlroots/build -lwlroots-0.19

#XWAYLAND =
#XLIBS =
# Uncomment to build XWayland support
XWAYLAND = -DXWAYLAND
XLIBS = xcb xcb-icccm

# scenefx eye candy (rounded corners, shadows, fade-in).
# Needs the scenefx-0.5 pkg-config package built against wlroots-0.20.
# On CachyOS/Arch: `sudo pacman -S scenefx0.5`. Keep 0 for a plain build
# without the extra dependency.
SCENEFX = 1

ifeq ($(SCENEFX),1)
SFX_PKG  = scenefx-0.5
SFX_INCS = `$(PKG_CONFIG) --cflags $(SFX_PKG)`
SFX_LIBS = `$(PKG_CONFIG) --libs $(SFX_PKG)`
SFX_DEF  = -DSCENEFX
else
SFX_INCS =
SFX_LIBS =
SFX_DEF  =
endif

# lua
LUA_PKG = lua5.4

PKGS      = wayland-server xkbcommon libinput $(LUA_PKG) $(XLIBS)

# Requires C11: _Alignas and anonymous unions in wlroots headers.
CC = cc
