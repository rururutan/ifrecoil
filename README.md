# Susie64 Recoil plug-in

This plug-in adapts the [Recoil Retro Computer Image Library](https://recoil.sourceforge.net/) to the Susie x64
plug-in interface. Recoil performs format recognition and decoding; the adapter converts its top-down RGB pixels to a Susie 24-bit DIB.
Because 4-bit and 8-bit images are forcibly converted to 24-bit color, this plug-in is not suitable for use as an image format conversion tool.

## Build

Build `src/ifrecoil.sln` with the `Release|x64` configuration.

## License

The adapter follows the project license. The bundled Recoil library is
licensed under the GNU General Public License version 2 or later; see
`src/recoil.h` and the upstream Recoil distribution for details.
