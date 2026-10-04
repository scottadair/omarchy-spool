# Spool

Printer settings for Omarchy. One window to see your printers, add a network
printer, set the default, print a test page, pause or resume a queue, choose
paper size, duplex, color and quality, and watch or cancel jobs.

Spool replaces *Printer Settings* (`system-config-printer`). It supports
driverless (IPP Everywhere) printers, which covers most network printers made
since about 2015. Printers that need a vendor driver are not supported yet, and
the add dialog says so.

## How it works

- Reading (printers, options, jobs, discovery, test pages, canceling your own
  jobs) uses libcups and needs no admin rights.
- Changing a queue (add, remove, rename, default, pause, accept jobs, option
  defaults) goes through `cups-pk-helper` on the system bus. The Omarchy polkit
  agent shows the password prompt; the approval is cached for the session, so
  expect about one prompt. Cancelling or restarting someone else's job also
  goes through it.
- Options come from each printer's own supported values.

## Keyboard

| Key | Action |
| --- | --- |
| `↑` `↓` / `K` | Select printer |
| `A` | Add a printer (`F5` rescans, `Enter` continues, `Esc` goes back) |
| `T` | Print a test page |
| `D` | Make default |
| `P` | Pause / resume |
| `G` | Accept / reject jobs |
| `R` | Rename |
| `Delete` | Remove (asks first) |
| `O` | Edit defaults: paper, duplex, color, quality (`Esc` returns) |
| `J` | Jobs for this printer |
| `X` / `R` | In Jobs: cancel / restart |
| `F5` | Refresh |
| `?` | Shortcuts |
| `Q` | Quit |

## Build

```sh
bin/build      # build/spool
bin/test       # Qt Test, offscreen
bin/install    # makepkg -fsi, adds it to the launcher
```

Needs `qt6-base`, `qt6-declarative`, `libcups`, `cups-pk-helper` and
`cups-filters` (for `driverless` discovery).
