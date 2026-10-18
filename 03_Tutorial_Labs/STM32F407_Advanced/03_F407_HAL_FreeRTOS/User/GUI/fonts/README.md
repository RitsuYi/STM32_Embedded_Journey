Measurement UI Subset 是从本机合法安装的 Noto Sans SC Regular/Bold 生成的字体子集，依据 SIL OFL 1.1 分发，许可证及源字体的版权声明见 OFL.txt。

源字体：Noto Sans SC (TrueType).otf、Noto Sans SC Bold (TrueType).otf；内部版权字段为 © 2014-2020 Adobe。上游许可：https://github.com/notofonts/noto-cjk/blob/main/Sans/LICENSE 。子集名称使用 Measurement UI Subset。

五份只读字体采用无压缩 4bpp 格式。12 px 用于状态和详情，14 px 用于尺寸标签、导航与操作按钮，16 px 加粗用于仪器和页面标题，24/28 px 加粗用于数字。生成器从测量与仪器页面源码提取所需汉字，并补齐 ASCII、单位、箭头等符号。没有运行时字体加载或完整中文字库；各文件开头列出字形数量和位图字节数。

构建直接编译已生成的 C 文件，无需安装字体、Python 或转换工具。以上五份是历史字体；当前屏幕字体的生成方法见下文。


2026-10-18历史：Pillow生成过`ui_font_9/11/13_bold`、16px统计和26px数字，部分数字横向缩为84%。旧字体和`tools/generate_dashboard_fonts.py`保留追溯，当前屏幕字体按下面方法重建。

2026-10-19当前：`tools/generate_screen_fonts.py`使用LVGL官方`lv_font_conv`1.5.3直接生成原尺寸4bpp字形，开启强网格微调，不启用LCD子像素排列、不压缩数字宽度。12px正文和14px标题各345字形，16px主标题/统计107字形，26px数字13字形。`ui_icons_16/20`各18个完整图标，独立指定字体，避免中文fallback造成空框或缺图。Wi-Fi连接和断开使用不同完整字形；后者离线合成，运行时不叠细线。图标来源/许可见`source/README.md`。

重建（仅需要生成字体时执行；常规ARM编译不依赖工具）：

```powershell
npm install --prefix build/FontTools --cache build/npm-cache --registry https://registry.npmjs.org --no-audit --no-fund --ignore-scripts --save-exact lv_font_conv@1.5.3
python tools/generate_screen_fonts.py
```

Python在当前步骤只使用标准库调用转换器；`--regular/--bold/--node/--converter`可指定本机路径。字形覆盖、数字宽度、图标边界、Wi-Fi状态及1/7/20行重绘一致性由`tests/ui_dashboard`核查。主机清晰度不等于LCD实物已验收。
