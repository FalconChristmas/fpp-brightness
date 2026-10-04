# fpp-brightness

Support for dynamically changing the brightness of channel data during a running show



## FPP commands

| Command | Arguments | What it does |
| --- | --- | --- |
| `Brightness` | brightness 0-200 | Set the brightness (100 = unchanged) |
| `Brightness Adjust` | -100 to 100 | Raise or lower the brightness by that much |
| `Brightness Fade` | brightness, seconds | Fade to a brightness over a duration |
| `Brightness Exclude Set` | channels | Replace the excluded channels. Blank clears the list |
| `Brightness Exclude Add` | channels | Stop applying brightness to these channels |
| `Brightness Exclude Remove` | channels | Apply brightness to these channels again |
| `Brightness Exclude Reset` | none | Go back to the exclude ranges saved on the plugin page |

Channels are 1-based and comma separated, for example `1-512,1000,2000-2099`.

The exclude commands change the running list only. The saved exclude ranges on the
plugin page are what fppd starts with. Saving that setting replaces whatever the
commands had set.
