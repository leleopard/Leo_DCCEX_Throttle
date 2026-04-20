# Leo DCC Throttle — Claude Instructions

## Flash target

**Always flash `st7796` unless the user explicitly asks for `ili9341`.**

```
~/.platformio/penv/Scripts/pio run -e st7796 -t upload
```

The hardware in use is the 4" 480×320 ST7796 display with XPT2046 touch.
`ili9341` is a secondary target for the 320×240 ILI9341 display and should
only be flashed when the user specifically requests it.
