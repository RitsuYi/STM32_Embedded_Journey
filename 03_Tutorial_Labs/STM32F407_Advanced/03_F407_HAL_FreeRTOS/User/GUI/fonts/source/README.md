# 屏幕字体源与许可

- 中文与数字：本机已有Noto Sans SC Regular/Bold，SIL OFL 1.1；上级`OFL.txt`保留版权与许可。
- 图标：LVGL官方字体转换页面指定的Font Awesome 5组合字库。官网master资源链接失效后，从官方LVGL v9.3.0标签取得同一源文件；这不改变本工程LVGL v9.6.0。
- 源地址：https://raw.githubusercontent.com/lvgl/lvgl/v9.3.0/scripts/built_in_font/FontAwesome5-Solid%2BBrands%2BRegular.woff
- WOFF SHA256：`F4E42F6CD69E5DBDCCCC0F2F5BE136CEBDE0E427641E45403BF9173A92DA95F4`，353228字节。
- Font Awesome许可原文保存在`FontAwesome_LICENSE.txt`，来源：https://raw.githubusercontent.com/FortAwesome/Font-Awesome/5.x/LICENSE.txt 。字体遵循SIL OFL 1.1。

`tools/build_ui_icon_font.cjs`只提取实际图标，并合成U+E001的Wi-Fi断开字形；派生字体命名Measurement UI Icons，避免沿用保留字体名称。它由`lv_font_conv`包内的opentype.js构建成临时TTF，再以官方转换器生成16/20px、4bpp的C文件。临时工具和TTF放在build，不参与固件编译。
