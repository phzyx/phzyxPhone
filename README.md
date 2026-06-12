# phzyxPhone

A testing-focused desktop SIP softphone for Linux, written in C++ on top of
[pjproject / PJSUA2](https://github.com/pjsip/pjproject) with a Qt GUI.

Unlike a consumer softphone, phzyxPhone is built to **poke at SIP/SDP/media
behaviour**: it exposes the full codec matrix, lets you inspect and rewrite the
SDP that goes on the wire, switches transports (UDP/TCP/TLS) and SRTP/ICE/STUN/
TURN modes, sends DTMF by RFC2833 or SIP INFO, and streams the raw PJSIP trace
log so you can see exactly what is being signalled.

Free and open source, licensed under **GPL-2.0-or-later** (see `LICENSE`).

## Features

- **Account & transport**: registrar/auth config; UDP, TCP or TLS transport;
  local port / bound address; per-account SRTP (disabled/optional/mandatory)
  and SRTP signalling requirement.
- **NAT**: STUN server, ICE enable, TURN (server/user/pass), SDP NAT rewrite.
- **Codec matrix**: enumerate every codec compiled into pjproject, enable/
  disable each, set priority (0-255), VAD, PLC, frames-per-packet (ptime) and
  average bitrate, then push to the live endpoint.
- **SDP inspection & override**: every locally-created offer/answer (and the
  remote offer) is captured and shown live. You can rewrite the outgoing SDP by
  whole-text replacement, ordered regex substitutions, or appended `a=`/`b=`
  lines - so you can force a direction (`sendonly`/`inactive`), strip an
  attribute, change `ptime`, inject malformed lines, etc.
- **Calls**: dial with custom INVITE headers, answer, hang up, hold/unhold,
  auto-answer with a configurable status code.
- **DTMF**: RFC2833 or SIP INFO, configurable signal duration.
- **Diagnostics**: live RTP/RTCP stream stats (packets, bytes, loss, jitter,
  RTT) and the full PJSIP trace log with export.

## Quick start

Everything you need is scripted. From a clean checkout:

```bash
git clone <your-fork-url> phzyxPhone
cd phzyxPhone
./bootstrap.sh
./build/phzyxphone
```

`bootstrap.sh` does three things:

1. installs the system packages (needs `sudo`),
2. downloads and builds pjproject into `third_party/pjproject-install`
   (self-contained, no system-wide install, with the extra codecs enabled),
3. configures and builds the app into `build/`.

The first run compiles pjproject from source, so expect a few minutes. Later
builds reuse it. If a script is not executable after cloning, run
`chmod +x bootstrap.sh scripts/*.sh` (or invoke with `bash <script>`).

## Setup scripts

Each step can also be run on its own (all live in `scripts/`, driven by
`bootstrap.sh`):

| Script | What it does |
| --- | --- |
| `scripts/install-deps.sh` | Installs the toolchain, CMake, Qt 5 Widgets, and the codec/TLS/ALSA dev libraries. Detects apt (Debian/Ubuntu), dnf (Fedora/RHEL) and pacman (Arch). `--dry-run` prints the command only. |
| `scripts/build-pjproject.sh` | Clones pjproject, writes a `config_site.h` enabling Opus/G.722/iLBC/GSM/Speex/L16, then `configure && make && make install` into `third_party/pjproject-install`. Options: `--ref <tag>` (default `2.17`), `--jobs N`, `--clean`. |
| `scripts/build.sh` | Configures and builds just the application (assumes pjproject + deps are present). Options: `--jobs N`, `--debug`. |

Useful flags on `bootstrap.sh`: `--skip-deps` (deps already installed) and
`--skip-pjproject` (reuse an existing pjproject build).

The app's `CMakeLists.txt` automatically detects the in-repo
`third_party/pjproject-install` prefix and bakes its library path into the
binary's RPATH, so `./build/phzyxphone` runs without setting
`LD_LIBRARY_PATH`.

## Manual build

If you prefer to wire things up yourself, you need a C++17 toolchain,
CMake >= 3.16, Qt 5 or 6 (Widgets), and pjproject built with the PJSUA2 C++
bindings.

```bash
# system deps (Debian/Ubuntu)
sudo apt-get install build-essential cmake pkg-config git \
    qtbase5-dev libasound2-dev libssl-dev \
    libopus-dev libgsm1-dev libspeex-dev libspeexdsp-dev

# pjproject into the in-repo prefix (what CMake looks for)
scripts/build-pjproject.sh           # or build/install pjproject yourself

# build the app
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
./build/phzyxphone
```

If you installed pjproject to a system prefix (e.g. `/usr/local`) instead of the
in-repo one, make sure its `lib/pkgconfig` is on `PKG_CONFIG_PATH` so
`pkg-config --exists libpjproject` succeeds.

> G.711 (PCMU/PCMA), G.722, GSM, iLBC, Speex, L16 and Opus are enabled by the
> build script. G.729 requires a separately-licensed codec library and is off
> by default.

To use Qt 6 instead of Qt 5, install `qt6-base-dev`; CMake prefers Qt 6
automatically when both are present.

## Usage

1. **Account / Transport** tab: choose **Simple** mode for the essentials
   (account, login/password, realm, transport + local port, and the remote
   server host + port) or **Advanced** mode for the full set of knobs (explicit
   ID/registrar URIs, bound address, NAT, SRTP, log level). Switching modes
   carries your values across. Use **Save profile... / Load profile...** to
   persist settings as YAML. Press **Start / Register**; the status line shows
   registration progress.
2. **Phone** tab: a traditional dialpad. Type or tap an extension/number (the
   host and port are filled in from the registrar) and press **Call**; the
   dialpad sends DTMF during a connected call. Speaker and microphone levels
   each have a slider and a mute toggle.
3. **Codecs** tab: press **Refresh from endpoint** to load the codec list, edit
   priorities/params, then **Apply all** before placing a call.
4. **SDP** tab: optionally configure an outgoing-SDP override and press
   **Apply SDP override settings**. Place a call and watch the captured
   offers/answers appear live.
5. **Call** tab: dial a URI (with an optional custom header), answer/hang up,
   send DTMF, and refresh RTP/RTCP stats during a call.
6. **Log** tab: the full PJSIP trace at the level chosen on the account tab;
   export it for sharing.

## Project layout

```
bootstrap.sh            - one-command setup (deps -> pjproject -> app)
scripts/                - install-deps.sh, build-pjproject.sh, build.sh
CMakeLists.txt          - Qt + pjproject (pkg-config) build
src/SipCore.{h,cpp}     - PJSUA2 wrapper: endpoint, account, calls, codecs,
                          SDP override, DTMF, stats, log capture
src/MainWindow.{h,cpp}  - Qt GUI (tabbed)
src/main.cpp            - entry point
third_party/            - pjproject source + install prefix (git-ignored)
```

## Notes & caveats

- The SDP override hook uses PJSUA2's `onCallSdpCreated()`. Modifying
  `wholeSdp` there is honoured by current pjproject (fixed upstream as #1911);
  the build script pins a recent release (default tag `2.17`).
- Whole-SDP replacement requires a *valid* SDP body or the offer/answer will be
  rejected; regex substitution and appended lines are safer for incremental
  fuzzing.
- TLS transport requires pjproject built with OpenSSL (`libssl-dev` present at
  build time) and appropriate certificates configured on your server.
- Audio uses the default ALSA capture/playback device. On headless test rigs
  you can build pjproject with the null audio device.

## Contributing

Contributions are welcome. The codebase is small and split cleanly between the
PJSUA2 wrapper (`SipCore`) and the Qt GUI (`MainWindow`); new testing knobs
generally mean adding a field to a settings struct, a control to a tab, and a
call into `SipCore`. Please keep changes building against the pinned pjproject
release and matching the existing GPL-2.0-or-later headers.

## License

phzyxPhone is licensed under the GNU General Public License, version 2 or
later (GPL-2.0-or-later). The full license text is in the
[`LICENSE`](LICENSE) file.
