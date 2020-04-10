Simple instance timer for Guild Wars (c)

You can use any injector to inject the timer and similarly remove the library with
injectors supporting removing a DLL.

The timer always appear at the position (0, 0), the top left of the Guild Wars handle.
You can drag it with your mouse.

You can change the font style, the text color and the text size by modifying the
following registry keys:

1. `HKEY_CURRENT_USER\\Software\\GwTimer\\GwTimer\\FontStyle`
   The type is `REG_SZ` and that should be the name of the font. (e.g. Arial)
2. `HKEY_CURRENT_USER\\Software\\GwTimer\\GwTimer\\TextColor`
   The type is `REG_DWORD` and the format is 0xAARRGGBB.
3. `HKEY_CURRENT_USER\\Software\\GwTimer\\GwTimer\\TextSize`
   The type is `REG_DWORD` and you can play with the value to set the best size for you.
