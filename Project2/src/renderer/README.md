# renderer 模块

当 `Scene` 和 `Camera` 都准备好以后，真正“把整张图跑出来”的就是这个模块。

它负责的不只是一个简单的双重循环，而是整条图像生成管线：

- 逐像素采样
- 多线程调度
- 自适应采样
- firefly clamp
- 内置后处理降噪
- 图像落盘

## 主要文件

- `Renderer.hpp` / `Renderer.cpp`：主渲染器，负责整张图的采样和输出
- `Denoiser.hpp` / `Denoiser.cpp`：内置的 joint bilateral 降噪器
- `ImageIO.hpp` / `ImageIO.cpp`：PPM / PNG 写出
- `stb_image_write_impl.cpp`：PNG 写出依赖

## 这个模块在程序中的位置

完整顺序是：

1. `main.cpp` 调用 `SceneLoader`
2. `scene/` 构建 `Scene`
3. `camera/` 构建 `Camera`
4. `renderer/Renderer` 根据分辨率和采样参数启动主循环
5. 对每个像素重复生成主光线并调用 `Scene::castRay()`
6. 把所有结果累积成 framebuffer
7. 如果开启降噪，再做一次后处理
8. 输出到 `output/`

## 当前主渲染流程

### 1. 逐像素采样

渲染器会按像素遍历整张图，并在每个像素上重复执行：

- 生成抖动采样坐标
- 调用相机生成主光线
- 调用积分器求辐射亮度
- 累积到该像素颜色

### 2. OpenMP 并行

当前使用的是：

- 行级并行
- 动态调度

这样可以更好地应对不同区域采样耗时不一致的问题。

### 3. Firefly Clamp

每个样本会做一次简单的亮度截断，目的是减轻：

- 罕见的超亮样本
- 高光噪点
- 镜面路径导致的火花点

这不是最物理严格的方法，但对于展示图和课程项目很实用。

### 4. 自适应采样

当前已经接入：

- 每像素在线均值
- 每像素亮度方差估计
- 周期性提前停止

也就是说：

- `spp` 是最大采样数
- 某些平滑区域可能会在达到阈值后提前停止

这样能减少很多“其实已经收敛了还在继续采”的像素。

### 5. 内置降噪

如果开启 `denoise`：

- 渲染器会额外生成主表面的 `albedo`、`normal` guide buffer
- 然后调用 `Denoiser` 做一次基于 `color + normal + albedo` 的联合双边滤波

这是一种 edge-aware 后处理，不是单纯的高斯模糊。

## `.scene` 里可以调什么

与这个模块最直接相关的是 `render { ... }` 里的这些字段：

```text
render {
    width = 960
    height = 720
    spp = 64
    adaptiveSampling = true
    adaptiveMinSpp = 16
    adaptiveBatch = 8
    adaptiveThreshold = 0.08
    denoise = true
    denoiseRadius = 3
    denoiseSigmaSpatial = 2.0
    denoiseSigmaColor = 0.15
    denoiseSigmaNormal = 0.2
    denoiseSigmaAlbedo = 0.25
}
```

这些参数怎么理解：

- `spp`：最多采多少
- `adaptiveSampling`：是否允许提前停止
- `adaptiveMinSpp`：最少采多少后才允许停
- `adaptiveBatch`：每隔多少个样本检查一次
- `adaptiveThreshold`：阈值越小，越保守
- `denoise`：是否输出降噪图
- `denoiseRadius`：滤波半径
- `denoiseSigmaSpatial`：空间平滑范围
- `denoiseSigmaColor`：颜色差异敏感度
- `denoiseSigmaNormal`：法线差异敏感度
- `denoiseSigmaAlbedo`：反照率差异敏感度

## 常见操作手册

### 想更快出测试图

可以这样调：

- 降低 `spp`
- 保留 `adaptiveSampling = true`
- 适当放宽 `adaptiveThreshold`
- 保留 `denoise = true`

### 想尽量保细节

可以这样调：

- 提高 `spp`
- 减小 `adaptiveThreshold`
- 减小 `denoiseRadius`
- 减小 `denoiseSigmaColor`

### 觉得降噪后边缘发糊

优先尝试：

- 减小 `denoiseSigmaNormal`
- 减小 `denoiseSigmaAlbedo`
- 减小 `denoiseRadius`

### 只想看纯原始结果

直接在场景里：

```text
denoise = false
```

## 输出文件命名

场景加载器会设置 `Scene::outputBaseName`，最终会写出：

- `<outputBaseName>.ppm`
- `<outputBaseName>.png`
- `<outputBaseName>_denoised.ppm`
- `<outputBaseName>_denoised.png`

## 想改这个模块时应该去哪

### 改主渲染循环

看：

- `Renderer.cpp`

### 改降噪算法

看：

- `Denoiser.cpp`

### 改图片输出格式

看：

- `ImageIO.cpp`

## 想继续扩展什么

这个模块后续很适合继续做：

- 更稳定的 firefly 抑制
- tile-based 渲染
- AOV 输出
- 采样热力图
- 更强的降噪器

## 协作建议

这个模块适合单独分成两类任务：

- 一人负责采样主循环、自适应采样和性能
- 一人负责输出与后处理

这样不会和 `scene/` 的积分逻辑缠得太紧。
