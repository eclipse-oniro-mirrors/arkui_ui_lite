# 图形UI组件<a name="ZH-CN_TOPIC_0000001123180295"></a>

-   [简介](#section11660541593)
-   [目录](#section161941989596)
-   [约束](#section119744591305)
-   [编译构建](#section137768191623)
-   [说明](#section1312121216216)
    -   [组件说明](#section66002422015)
    -   [使用说明](#section129654513264)

-   [本次改动](#section_changes_zh)
-   [相关仓](#section1371113476307)

## 简介<a name="section11660541593"></a>

图形UI组件实现了一套系统级的图形引擎。

该组件为应用开发提供UIKit接口，包括了动画、布局、图形转换、事件处理，以及丰富的UI组件。

组件内部直接调用HAL接口，或者使用WMS\(Window Manager Service\)提供的客户端与硬件交互，以完成事件响应、图像绘制等操作。

**图 1**  图形子系统架构图<a name="fig1358754417214"></a>  
![](figures/图形子系统架构图.png "图形子系统架构图")

## 目录<a name="section161941989596"></a>

```
/foundation/arkui/ui_lite
├── frameworks                  # 框架代码
│   ├── animator                # 动画模块
│   ├── common                  # 公共模块
│   ├── components              # 组件
│   ├── core                    # ui主流程（渲染、任务管理等）
│   ├── default_resource
│   ├── dfx                     # 维测功能
│   ├── dock                    # 驱动适配层
│   │   └── ohos                # ohos平台适配
│   ├── draw                    # 绘制逻辑
│   ├── engines                 # 绘制引擎
│   ├── events                  # 事件
│   ├── font                    # 字体
│   ├── imgdecode               # 图片管理
│   ├── layout                  # 页面布局
│   ├── themes                  # 主题管理
│   ├── window                  # 窗口管理适配层
│   └── window_manager
│       └── dfb
├── interfaces                  # 接口
│   ├── innerkits               # 模块间接口
│   │   └── xxx                 # 子模块的接口
│   └── kits                    # 对外接口
│       └── xxx                 # 子模块的接口
├── test                        # 测试代码
│   ├── framework
│   │   ├── include             # 测试框架头文件
│   │   └── src                 # 测试框架源码
│   ├── uitest                  # 显示效果测试(可执行程序在foundation/window/window_manager_lite/test:sample_ui)
│   │   └── test_xxx            # 具体UI组件效果测试
│   └── unittest                # 单元测试
│       └── xxx                 # 具体UI组件单元测试
└── tools                       # 测试和模拟器工具（模拟器工程、资源文件）
    └── qt                      # QT工程
```

## 约束<a name="section119744591305"></a>

平台约束

-   Windows平台仅支持QT和OHOS IDE。
-   其他平台支持情况参考vendor/hisilicon/\[product\_name\]/config.json中的graphic标签（不存在graphic标签即该产品不存在图形子系统）。

## 编译构建<a name="section137768191623"></a>

```
# 通过gn编译，在out目录下对应产品的文件夹中生成图形库
hb build -T //foundation/arkui/ui_lite:ui_lite -f  # 编译libui.so
hb build -T //foundation/window/window_manager_lite/test:sample_ui -f # 编译UI用例
hb build -T //foundation/arkui/ui_lite/test/unittest:arkui_ui_lite_test -f # 编译TDD用例

# 编译qt库可直接参考qt模拟器工程：arkui/ui_lite/tools/qt/simulator/simulator.pro
```

## 说明<a name="section1312121216216"></a>

### 组件说明<a name="section66002422015"></a>

组件分为基础组件和容器组件

-   基础组件：仅实现组件自身单一功能，比如按钮、文字、图片等；
-   容器组件：可将其他组件作为自己的子组件，通过组合实现复杂功能。

**图 2**  图形组件一览<a name="fig1594213196218"></a>  
![](figures/图形组件一览.png "图形组件一览")

### 使用说明<a name="section129654513264"></a>

foundation/arkui/ui_lite/test/uitest中提供了图形所有组件和功能接口的使用范例。

-   Windows环境可运行QT工程调试，详见[加入指引](https://gitee.com/openharmony/arkui_ui_lite/wikis/%E5%BF%AB%E9%80%9F%E5%8A%A0%E5%85%A5/%E5%8A%A0%E5%85%A5%E6%8C%87%E5%BC%95)

    工程文件路径：

    ```
    arkui/ui_lite/tools/qt/simulator/simulator.pro
    ```

-   其他调试环境可以编译运行foundation/window/window_manager_lite/test:sample\_ui

    编译成功后得到可执行程序out/\[product\_name\]/dev\_tools/bin/sample\_ui，在实际环境上运行即可观察对应组件显示效果。


## 本次改动<a name="section_changes_zh"></a>

本仓针对 HiSpark AiFly（Hi3403V100）开发板进行功能适配。为便于区分，以下按“新增能力”与“原有功能增强”两部分说明。

### 新增能力

1. 新增 Hi3519 图形引擎
    -   新增 `frameworks/engines/gfx/hi3519/hi3519_engine.{h,cpp}`，实现 `Hi3519GfxEngine`（继承 `SoftEngine`）。
    -   `Blit`：支持带透明度混合的硬件加速搬移（`GfxBlitWithOpt`）。
    -   `Fill`：填充区域不小于 50×50 时走硬件填充（`GfxFillArea`）。
    -   `DrawTransform`：无旋转且传入有效物理地址时走硬件缩放搬移。
    -   硬件接口不可用时自动回退到软件绘制（`SoftEngine`）。
    -   通过构造函数属性自动注册为默认图形引擎。

2. 新增 PNG / JPEG 专用图片解码器
    -   新增 `frameworks/imgdecode/png_img_decoder.{h,cpp}`：基于 libpng，支持灰度、调色板、16 位、tRNS 等格式自动转换，统一输出 ARGB8888。
    -   新增 `frameworks/imgdecode/jpeg_img_decoder.{h,cpp}`：基于 libjpeg，逐行解码并统一输出 ARGB8888。
    -   两者均继承原有 `FileImgDecoder`，实现 `Open`、`Close`、`GetHeader`、`ReadToCache`。
    -   开启 `CACHE_JPEG_AND_PNG` 后，JPEG/PNG 解码改由上述专用解码器写入缓存，原有内联解码路径关闭。

3. 新增 Gralloc 显存分配接入
    -   启动流程新增 `GrallocEngines::Init()`（`frameworks/common/graphic_startup.cpp`）。
    -   根视图映射缓冲区改为通过 `GrallocEngines::AllocBuffer` / `FreeBuffer` 申请与释放（`frameworks/components/root_view.cpp`），申请时使用 `HBM_USE_ASSIGN_SIZE | HBM_USE_MEM_MMZ`。

4. 新增图片绘制物理地址（phyAddr）支持
    -   `TransformDataInfo` 新增 `phyAddr` 字段（`interfaces/innerkits/engines/gfx/gfx_engine_manager.h`）。
    -   `DrawUtils::DrawImage` 新增 `imgPhyAddr` 入参（`frameworks/draw/draw_utils.{h,cpp}`），并在 `BufferInfo` 中透传物理地址，供硬件引擎使用。

5. 新增构建配置
    -   `BUILD.gn`、`ui.gni` 新增 `png_img_decoder.cpp`、`jpeg_img_decoder.cpp`。
    -   按 `media_interface_upgrade` 选择图形引擎：定义时编译 `hi3519_engine.cpp`，否则编译 `hi3516_engine.cpp`。
    -   `OAT.xml` 新增 HiSilicon 版权策略（Copyright (c) 2026 HiSilicon）。

### 原有功能增强

1. 原有文件图片解码器（`FileImgDecoder`）增强为可派生基类
    -   原有功能：提供图片文件的打开、头部读取与缓存读取。
    -   本次增强：`Open`、`Close`、`GetHeader`、`ReadLine`、`ReadToCache` 改为虚函数，构造/析构调整为 `protected`/虚析构，成员 `inCache_` 更名 `inCache`；`GetHeader` 复用已打开的 fd，`Open` 不再校验颜色模式；`ReadToCache` 拆分出 `ReadFileHeader`、`GetFileSize`、`ReadImageData`、`HandleReadFailure`，并在硬件引擎场景按行读取、按 stride 计算数据大小。

2. 原有图片缓存管理（`CacheManager`）增强
    -   原有功能：按路径缓存并复用已解码图片，未命中时解码后写入缓存。
    -   本次增强：`TryDecode` 依据图片类型选择 PNG/JPEG 专用解码器（其余仍走 `FileImgDecoder`）；调整为先 `Open` 后 `GetHeader`；命中缓存时不再重复 `ReadToCache`；缓存读取失败时清理条目并返回错误，同时补充失败日志。

3. 原有图片绘制流程增强
    -   原有功能：`DrawImage::DrawCommon` 在缓存命中与未命中两种情况下分别绘制。
    -   本次增强：拆分为私有方法 `DrawFromCache`（命中缓存）与 `DrawFromFile`（逐行读取文件），并在硬件引擎场景向底层绘制接口传递物理地址。

4. 原有组件与接口配套调整
    -   `UIImageView` 抽出 `DrawImageWithTransform` 私有方法，统一处理变换绘制。
    -   `RootView::BlitMapBuffer`、`UICanvas::BlitMapBuffer`、`UIAnalogClock::DrawHandImage`、`DrawLabel::DrawLetterWithRotate` 按硬件引擎开关构造带 `phyAddr` 的 `TransformDataInfo`。
    -   `Image::CheckImgType` 调整为静态方法并开放给 `CacheManager`。
    -   `BUILD.gn` 修正 `ui_video.cpp` 源文件变量的拼写（`source` → `sources`）。

## 相关仓<a name="section1371113476307"></a>

[图形子系统](https://gitee.com/openharmony/docs/blob/master/zh-cn/readme/%E5%9B%BE%E5%BD%A2%E5%AD%90%E7%B3%BB%E7%BB%9F.md)

[window_window_manager_lite](https://gitee.com/openharmony/window_window_manager_lite/blob/master/README.md)

[graphic_surface_lite](https://gitee.com/openharmony/graphic_surface_lite/blob/master/README.md)

**arkui_ui_lite**

[graphic_graphic_utils_lite](https://gitee.com/openharmony/graphic_graphic_utils_lite/blob/master/README.md)