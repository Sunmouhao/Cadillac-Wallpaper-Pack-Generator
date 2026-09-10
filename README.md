# Cadillac-Wallpaper-Pack-Generator

可便捷生成凯迪拉克 CT5、CT6、XT5 等大连屏壁纸包的软件。

## CT5 Wallpaper Packager

## Light/Dark image sources

- Light mode uses the main JPG/JPEG image and the light crop previews.
- Dark mode can use a separate JPG/JPEG image and independent dark crop previews.
- If the dark image is left empty, the tool keeps the old behavior and derives dark resources from the light image.
- Light resources are written to `light_*` PNG files and KZB light entries `072/073/074`.
- Dark resources are written to `dark_*` PNG files and KZB dark entries `069/070/071`.

## 当前版本

当前版本从 JPG/JPEG 图片生成完整壁纸包，不需要选择模板资源包。发布目录内置基础 KZB，程序会自动复制它并写入新壁纸内容。

- 主图：生成右半屏 PNG、预览图和屏保图。
- 左半屏图片：可单独选择 JPG/JPEG；留空时使用主图。
- 左半屏 KZB：工具会裁切为 `8960x1320`，按 KZB 解包的逆过程做垂直翻转，通过 `astcenc-native.exe` 编码为 ASTC 8x8，并写入 `.kzb` 中的 6 个 `Resource Files/Images/*wallpaper*.png` 图像块。
  - `069/072 *_blur*`：整图模糊后的深色/浅色版本。
  - `070/073 *_dic*`：左右屏分界附近虚化后的深色/浅色版本。
  - `071/074 *wallpaper*`：普通深色/浅色版本。
- `067/068` mask 图保留基础 KZB 原样。
- `astcenc-native.exe` 必须和发布版 `Ct5WallpaperPackager.exe` 放在同一目录；开发机上也会查找 `D:/Cadillac/kzb-tools/bin/astcenc-native.exe`。

Qt Widgets 工具，用于把 JPG/JPEG 图片制作成 CT5 壁纸包目录。

## 输入

- JPG/JPEG 图片
- 可选的深色模式 JPG/JPEG 图片

## 输出

默认输出完整目录：

```text
E:\paper\output\BFA3A0F4596C4C57A6BCDC1EB3348932
```

工具会从内置基础资源创建包，并写入 `ipd\wallpaper\*.kzb`。其中：

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

## 操作流程

1. 选择 JPG/JPEG 图片。
2. 每个预览框只显示最终裁切出的部分，多余部分不显示；在框内拖动图片即可单独调整位置。
3. 在任意预览框上使用鼠标滚轮，可以单独缩放该区域：
   - 向上滚动：放大
   - 向下滚动：缩小
4. 选择输出目录后点击“制作壁纸包”。

制作过程会在后台线程执行，界面不会卡死；进度条会显示当前步骤。右半屏壁纸、预览图、屏保图会分别按固定目标尺寸输出。左半屏会按 `8960x1320` 裁切、垂直翻转、ASTC 8x8 编码后写入内置基础 KZB。

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

CMake 构建会将必要的基础 KZB 复制到发布目录的 `template` 子目录；运行时请保留该目录。

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
## KZB 解包

点击界面中的 `Unpack KZB`，选择一个 `.kzb` 文件和输出目录。工具会解析 `KZBF` 条目表，只把最终可查看 PNG 写入所选输出目录。

图片条目按 `ASTC 8x8 / 8960x1320` 解码，并只做垂直翻转，不做水平翻转。中间 ASTC 和未翻转 PNG 使用临时目录，完成后自动清理。

解码依赖 `astcenc-native.exe`。工具会优先查找程序同目录下的 `astcenc-native.exe`，开发机上也会查找 `D:/Cadillac/kzb-tools/bin/astcenc-native.exe`。
