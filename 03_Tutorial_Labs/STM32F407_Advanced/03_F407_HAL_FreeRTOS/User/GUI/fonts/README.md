Measurement UI Subset 是从本机合法安装的 Noto Sans SC Regular/Bold 生成的字体子集，依据 SIL OFL 1.1 分发，许可证及源字体的版权声明见 OFL.txt。

源字体：Noto Sans SC (TrueType).otf、Noto Sans SC Bold (TrueType).otf；内部版权字段为 © 2014-2020 Adobe。上游许可：https://github.com/notofonts/noto-cjk/blob/main/Sans/LICENSE 。子集名称使用 Measurement UI Subset。

五份只读字体采用无压缩 4bpp 格式。12 px 用于状态和详情，14 px 用于尺寸标签、导航与操作按钮，16 px 加粗用于仪器和页面标题，24/28 px 加粗用于数字。生成器从测量与仪器页面源码提取所需汉字，并补齐 ASCII、单位、箭头等符号。没有运行时字体加载或完整中文字库；各文件开头列出字形数量和位图字节数。

构建直接编译已生成的 C 文件，无需安装字体、Python 或转换工具。增加文字时修改 tools/generate_measurement_fonts.py 的字符集合后重新生成。生成器只依赖 Pillow；参数 --regular 和 --bold 分别指定上述 OFL 字体路径。
