# Roblox Camera Flip

Native Linux GTK3 GUI for the Roblox camera flip macro.

The macro keeps the original low-latency path:

- evdev reads the physical keyboard.
- X11/XTest injects relative mouse movement.
- Camera movement is horizontal only: `dy` is always `0`.
- No xdotool, Python, shell scripts, Wine user32 calls, absolute warps, or keyboard-arrow simulation.

## Build on CachyOS

Fish shell:

```fish
sudo pacman -S --needed base-devel gtk3 libxtst libx11
mkdir -p ~/.config/roblox-camera-flip
make clean
make
./camera_flip
```

## Install

Normal user install with a KDE launcher entry and desktop shortcut:

```fish
make install-user
```

System install:

```fish
sudo make install
camera_flip
```

## Permissions

The app should run as your normal user. On this system the input devices are owned by the `input` group.

Check groups and device permissions:

```fish
id -nG
ls -l /dev/input/event* /dev/uinput
getent group input
```

Install the included udev rule if your user cannot read `/dev/input/event*` or `/dev/uinput`:

```fish
sudo install -Dm644 config/99-roblox-camera-flip.rules /etc/udev/rules.d/99-roblox-camera-flip.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
sudo usermod -aG input $USER
```

Log out and back in, then verify:

```fish
id -nG
test -r /dev/input/event10; and echo evdev-readable; or echo evdev-not-readable
test -w /dev/uinput; and echo uinput-writable; or echo uinput-not-writable
```

## Settings

Settings are saved to:

```text
~/.config/roblox-camera-flip/config.ini
```

Defaults:

```ini
[camera]
calibration=2500
steps=32
step_delay_us=500

[click]
enabled=true
delay_ms=0

[keyboard]
device=auto
key=46

[performance]
mode=maximum
```

Change calibration, step count, step delay, click enable, click delay, and keyboard device in the GUI, then press **Save Settings**. **Reset Defaults** restores the values above.

## Testing

Use **Refresh Devices** to test evdev detection. The GUI should show the keyboard name and `/dev/input/event*` path.

Use **Test Flip** to inject one forward 180 degree turn and one reverse 180 degree turn without clicking.

Use **Test Click** to inject exactly one left click.

Use the diagnostics labels for X11, XTest, evdev, uinput, keyboard, and macro state.

If Roblox receives movement but does not rotate, check:

- Roblox is focused and running under the X11 session, not a Wayland session.
- Vinegar/Wine is receiving XTest input.
- The camera is in a mode that accepts relative mouse look.
- Roblox camera sensitivity/FOV needs a different calibration.
- Step count or step delay is too aggressive for the Wine/Roblox input path.

Try increasing `steps`, increasing `step_delay_us`, or adjusting `calibration`.
