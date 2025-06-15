# Human readable statistics generator service

# Installation

```
make install
```

- install binary to `${HOME}/.local/bin/yustat`
- install systemd service to `${HOME}/.config/systemd/user/yustat.service`
- enable and start the service

The service will update the statistics file at `${XDG_RUNTIME_DIR}/yustat.tmux` every second.

# Check service status

```
systemctl --user status yustat.service
```

# Usage

```
Usage: ./build/yustat [options]
Options:
  -h, --help              Show this help message and exit
  -o, --output FILE       Output to FILE (default: STDOUT)
  -i, --interval SECONDS  Update interval in seconds (default: one-shot)
  -f, --format FORMAT     Output format (tmux, console, json; default: tmux)
```

Edit `~/.tmux.conf`

example configuration for tmux:
```
set -g status-right "#{?client_prefix,#[reverse],}#(cat /run/user/1000/yustat.tmux || echo '%Y-%m-%d(%a) %H:%M:%S')"
```
