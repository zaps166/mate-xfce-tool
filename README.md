# mate-xfce-tool

## Description

This tool can be used only when your desktop configuration mixes Mate and Xfce4 elements:
 - Mate session manager,
 - Mate desktop,
 - Xfce4 window manager,
 - Xfce4 panel.

This tool will fix:
 - Mate desktop bounds after screen size or font dpi change,
 - Xfce4 panel after scaling factor or font dpi change,
 - Xfwm4 style after scaling factor change.

## Installation (Linux)

```sh
git clone https://github.com/zaps166/mate-xfce-tool.git
cd mate-xfce-tool
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
make
sudo make install/strip
```
