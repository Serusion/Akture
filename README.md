# Akture

Akture is a lightweight X11 display manager for [AutumnBSD](https://github.com/Serusion/AutumnBSD) and NetBSD. It aims to show the login screen quickly on old hardware, with few dependencies, and keeps the part that runs as root small enough to be verified with Frama-C.

## Status

Early development. The first implementation is complete and its tests, fuzzers and formal verification pass, but it has not yet been run on a real NetBSD system. Do not use it as your only way to log in yet.

## Features

- Privilege separation: a small master runs as root, and everything that handles input from outside runs in an unprivileged greeter
- Authentication through PAM, including password expiry and other extra prompts
- Session selection, switching to another user, keyboard layout switching, shutdown and reboot from the login screen
- Multiple monitors and HiDPI scaling (1, 1.25, 1.5 or 2)
- Backgrounds in a prepared format (`.akbg`) that needs no decoding at login, and `akture-conv` to make them from PNG or JPEG
- Hooks run at login events, e.g. for AutumnBSD's aprefetch
- Optional autologin
- No CPU use while idle, apart from updating the clock once a minute

## Requirements

- NetBSD (FreeBSD support is in place but untested)
- BSD make and pkg-config
- OpenPAM
- libxcb with the shm, randr and xkb extensions, pixman, FreeType and libxkbcommon (with libxkbcommon-x11), for the greeter
- libpng and libjpeg, for `akture-conv`
- The fonts Outfit and Zen Kaku Gothic New, or any TrueType or OpenType fonts set in `akture.conf`. They are not included here; the package `autumn-fonts` of [Serusion/autumn-pkgsrc](https://github.com/Serusion/autumn-pkgsrc) installs them

## Building and installing

```sh
make
make install
```

This installs `akture` to `/usr/sbin`, `akture-greeter` to `/usr/libexec`, `akture-conv` to `/usr/bin`, the configuration to `/etc/akture`, the PAM services to `/etc/pam.d` and the rc.d script.

These are the paths of the base system. To install under another prefix, as the pkgsrc package does, set the `AKTURE_` variables of [Makefile.inc](Makefile.inc), for example:

```sh
make AKTURE_PREFIX=/usr/pkg AKTURE_ETCDIR=/usr/pkg/etc/akture
```

The pkgsrc package of Akture for AutumnBSD is in [Serusion/autumn-pkgsrc](https://github.com/Serusion/autumn-pkgsrc).

The greeter runs as the user `_akture`, which has to exist:

```sh
groupadd _akture
useradd -g _akture -d /var/db/akture -s /sbin/nologin -c "Akture greeter" _akture
```

Then enable and start it:

```sh
echo akture=YES >> /etc/rc.conf
/etc/rc.d/akture start
```

## Configuration

The settings are in `/etc/akture/akture.conf`; see [akture.conf(5)](akture/akture.conf.5). For example:

```
background = autumn
keyboard_layouts = jp,us
logo = **Autumn**BSD
```

Each file in `/etc/akture/sessions` adds a session to the menu:

```
name = Fluxbox
exec = /usr/pkg/bin/startfluxbox
```

Backgrounds are made once for each screen resolution and found by name:

```sh
akture-conv -s 1366x768 -d 35 -o /usr/share/akture/backgrounds/autumn-1366x768.akbg autumn.jpg
```

Programs in `/etc/akture/hooks.d` are run, in name order, at the events `greeter-shown`, `auth-ok`, `session-started` and `session-ended`; see [akture(8)](akture/akture.8).

## Verification

- `verify/`: Frama-C. WP proves the master's message parser free of run-time errors, that no session starts without authentication, the order of the privilege drop and the clearing of secrets. Eva shows the configuration reader and the `.akbg` header check free of run-time errors.
- `spec/`: a TLA+ model of the master, the greeter and the session, including crashes and a compromised greeter, checked with TLC.
- `tests/`: atf-c unit tests, and libFuzzer harnesses in `tests/fuzz`.

```sh
cd verify && make
java -jar tla2tools.jar -config spec/akture.cfg spec/akture.tla
cd tests/fuzz && make run
```

The tool versions used are listed in [verify/VERSIONS](verify/VERSIONS).

## Source layout

```
akture/            the master, akture(8)
akture-greeter/    the login screen, akture-greeter(8)
akture-conv/       the background converter, akture-conv(1)
common/            message protocol, options and helpers
libautuni/         drawing, .akbg format and parsers: a copy of
                   Serusion/libautuni, built but not installed
etc/               configuration, PAM services, sessions, rc.d script
autumnbsd/         the aprefetch hook shipped by AutumnBSD
tests/             unit tests and fuzzers
verify/            Frama-C setup
spec/              TLA+ model
```

## License

BSD-2-Clause; see [LICENSE](LICENSE).

Bug reports and pull requests are welcome on [GitHub](https://github.com/Serusion/Akture/issues).
