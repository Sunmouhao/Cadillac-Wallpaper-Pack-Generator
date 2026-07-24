# CT5 Wallpaper Packager

## Light/Dark image sources

- Light mode uses the main JPG/JPEG image and the light crop previews.
- Dark mode can use a separate JPG/JPEG image and independent dark crop previews.
- If the dark image is left empty, the tool keeps the old behavior and derives dark resources from the light image.
- Light resources are written to `light_*` PNG files and KZB light entries `072/073/074`.
- Dark resources are written to `dark_*` PNG files and KZB dark entries `069/070/071`.

## 当前版本

当前版本已经改成从 JPG/JPEG 图片生成左半屏 KZB 图像块，不再要求选择已有可运行 KZB 包作为来源。

- 主图：生成右半屏 PNG、预览图和屏保图。
- 左半屏图片：可单独选择 JPG/JPEG；留空时使用主图。
- 左半屏 KZB：工具会裁切为 `8960x1320`，按 KZB 解包的逆过程做垂直翻转，通过 `astcenc-native.exe` 编码为 ASTC 8x8，并写入 `.kzb` 中的 6 个 `Resource Files/Images/*wallpaper*.png` 图像块。
  - `069/072 *_blur*`：整图模糊后的深色/浅色版本。
  - `070/073 *_dic*`：左右屏分界附近虚化后的深色/浅色版本。
  - `071/074 *wallpaper*`：普通深色/浅色版本。
  - `067/068` mask 图保持模板原样。
- `astcenc-native.exe` 必须和发布版 `Ct5WallpaperPackager.exe` 放在同一目录；开发机上也会查找 `D:/Cadillac/kzb-tools/bin/astcenc-native.exe`。

Qt Widgets 工具，用于把 JPG/JPEG 图片制作成 CT5 壁纸包目录。

## 输入

- JPG/JPEG 图片
- 一个完整模板包目录，例如：

```text
E:\paper\BFA3A0F4596C4C57A6BCDC1EB3348932
```

## 输出

默认输出完整目录：

```text
E:\paper\output\BFA3A0F4596C4C57A6BCDC1EB3348932
```

工具会复制模板包，保留 `ipd\wallpaper\*.kzb`。其中：

- `vcd\wallpaper` 内的 `light_wallpaper_vcd.png`、`dark_wallpaper_vcd.png` 是右半屏深色/浅色壁纸。
- 左半屏壁纸在 `ipd\wallpaper\*.kzb` Kanzi 资源包内，当前工具会重写其中的 wallpaper 图像块。
- 当前工具只覆盖以下 PNG：

```text
light_preview_image.png              2198x367
dark_preview_image.png               2198x367
light_dim_background.png             3950x1320
dark_dim_background.png              3950x1320
vcd\wallpaper\light_wallpaper_vcd.png 3950x1320 右半屏浅色壁纸
vcd\wallpaper\dark_wallpaper_vcd.png  3950x1320 右半屏深色壁纸
rid\screenSaver\light_screenSaver_rid.png 1920x1080
rid\screenSaver\dark_screenSaver_rid.png  1920x1080
```

如果模板没有 `rid\screenSaver`，屏保文件会跳过。

## 操作流程

1. 选择 JPG/JPEG 图片。
2. 工具会先读取模板包内现有 PNG 的真实宽高，三个裁切预览框会保持和模板一致的形状：
   - 右半屏壁纸：读取 `vcd\wallpaper\light_wallpaper_vcd.png`
   - 预览图：读取 `light_preview_image.png`
   - 屏保图：读取 `rid\screenSaver\light_screenSaver_rid.png`
3. 每个预览框只显示最终裁切出的部分，多余部分不显示；在框内拖动图片即可单独调整位置。
4. 在任意预览框上使用鼠标滚轮，可以单独缩放该区域：
   - 向上滚动：放大
   - 向下滚动：缩小
5. 选择模板壁纸包和输出目录。
6. 点击“制作壁纸包”。

制作过程会在后台线程执行，界面不会卡死；进度条会显示当前步骤。右半屏壁纸、预览图、屏保图会分别按各自预览框的位置、缩放比例和模板原始形状输出。左半屏会按 `8960x1320` 裁切、垂直翻转、ASTC 8x8 编码后写入模板 kzb。

## 构建

用 Qt Creator 打开：

```text
D:\Cadillac\Ct5WallpaperPackager\Ct5WallpaperPackager.pro
```

选择 Qt 5.15.2 msvc2019_64 Kit 后构建运行。

也可以用 CMake 打开：

```text
D:\Cadillac\Ct5WallpaperPackager\CMakeLists.txt
```

当前机器已检测到 Qt：`D:\Qt\5.15.2\msvc2019_64`。如果命令行提示找不到 `cl.exe`，需要安装或启用 Visual Studio Build Tools 的 MSVC 编译环境。

## Visual Studio

也可以直接用 Visual Studio 打开：

```text
D:\Cadillac\Ct5WallpaperPackager\Ct5WallpaperPackager.sln
```

工程默认配置：

- 平台：`x64`
- 配置：`Debug` / `Release`
- Qt 路径：`D:\Qt\5.15.2\msvc2019_64`
- 平台工具集：`v142`

如果 Visual Studio 提示没有 `v142`，在项目属性中把 `平台工具集` 改成你本机安装的版本，或安装 Visual Studio 2019/2022 的 C++ 桌面开发工具。
## 左半屏 KZB 替换

界面中的“左半屏来源”可以选择一个已有主题包目录，也可以直接选择一个 `.kzb` 文件。制作时工具会先复制模板包，再从来源 `.kzb` 中查找 `Resource Files/Images/*wallpaper*.png` 条目，并把这些条目的二进制数据写入输出包的目标 `.kzb`。

这个功能用于“用已有可运行主题包里的左半屏壁纸替换模板包里的左半屏壁纸”。当前制作流程也可以把普通 JPG/JPEG 裁切、编码并写入 CT5 使用的 `.kzb` 内部图像块。

## KZB 解包

点击界面中的 `Unpack KZB`，选择一个 `.kzb` 文件和输出目录。工具会解析 `KZBF` 条目表，只把最终可查看 PNG 写入所选输出目录。

图片条目按 `ASTC 8x8 / 8960x1320` 解码，并只做垂直翻转，不做水平翻转。中间 ASTC 和未翻转 PNG 使用临时目录，完成后自动清理。

解码依赖 `astcenc-native.exe`。工具会优先查找程序同目录下的 `astcenc-native.exe`，开发机上也会查找 `D:/Cadillac/kzb-tools/bin/astcenc-native.exe`。
