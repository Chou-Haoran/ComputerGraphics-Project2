# desktop_scene

这个目录是项目里最主要的桌面静物场景工作区。你可以把它理解为：

- 一组 OBJ / MTL / 贴图资源
- 一组可直接运行的 `.scene` 场景预设
- 一个适合继续搭场景、改材质、调灯光和出图的实验目录

如果你现在要做：

- 最终展示图
- 材质对比图
- 灯光测试图
- 打字机 + 桌面的构图调整

基本都应该从这里开始。

## 目录里有什么

### 资源子目录

- `typewriter/`：打字机 OBJ、MTL、贴图和 `typewriter.materialmap`
- `table/`：桌子 OBJ、MTL、贴图和 `table.materialmap`

### 场景预设

- `typewriter_table_showcase.scene`：当前主展示场景，桌子 + 打字机 + 多灯布光
- `typewriter_material_test.scene`：材质测试用场景
- `light_types_demo.scene`：不同灯光类型的演示
- `area_light_shapes_demo.scene`：面积灯形状演示
- `area_light_shapes_visible_demo.scene`：可见灯体演示

## 怎么运行指定场景

在 `Project2/` 根目录运行：

```bash
./build/Project2Renderer assets/models/desktop_scene <view_name>
```

例如运行主展示场景：

```bash
./build/Project2Renderer assets/models/desktop_scene typewriter_table_showcase
```

这里的 `<view_name>` 对应：

```text
assets/models/desktop_scene/<view_name>.scene
```

所以：

- `typewriter_table_showcase` 会读取 `typewriter_table_showcase.scene`
- `light_types_demo` 会读取 `light_types_demo.scene`

## 常用运行示例

### 默认按场景文件自带分辨率和采样数渲染

```bash
./build/Project2Renderer assets/models/desktop_scene typewriter_table_showcase
```

### 指定分辨率和采样数

```bash
./build/Project2Renderer assets/models/desktop_scene typewriter_table_showcase 800x600 64
```

### 临时关闭阴影

```bash
./build/Project2Renderer assets/models/desktop_scene typewriter_table_showcase 800x600 64 --no-shadow
```

### 临时覆盖环境图

```bash
./build/Project2Renderer assets/models/desktop_scene typewriter_table_showcase 800x600 64 --envmap=assets/envmaps/typewriter_studio_soft.hdr
```

### 临时覆盖景深参数

```bash
./build/Project2Renderer assets/models/desktop_scene typewriter_table_showcase 800x600 64 --aperture=0.0 --focus=0.228
```

## 输出文件会在哪里

当前渲染结果默认输出到：

- `Project2/output/<view_name>.ppm`
- `Project2/output/<view_name>.png`

如果场景里开启了内置降噪，还会额外输出：

- `Project2/output/<view_name>_denoised.ppm`
- `Project2/output/<view_name>_denoised.png`

## 场景文件怎么增删改

这个目录里的 `.scene` 文件就是你最主要的编辑入口。

### 新增一个场景

最简单的方式是：

1. 复制一份现有场景，例如复制 `typewriter_table_showcase.scene`
2. 改成新名字，例如 `my_showcase.scene`
3. 运行时使用：

```bash
./build/Project2Renderer assets/models/desktop_scene my_showcase
```

这种方式最适合做：

- 不同构图版本
- 不同灯光版本
- 最终展示图和实验图分离

### 删除一个场景

如果确认不再需要，直接删除对应的 `.scene` 文件即可。

### 修改一个场景

最常见的修改包括：

- 改渲染参数
- 改相机
- 增删模型
- 调模型摆位
- 增删灯光
- 切换环境图

建议顺序通常是：

1. 先改 `camera`
2. 再改 `mesh`
3. 再改 `light`
4. 最后改 `render`

这样效率比较高，因为构图和布光通常比采样参数更先决定画面好不好看。

## `.scene` 里能配置哪些块

当前这个目录下的场景文件支持：

- `render { ... }`
- `camera { ... }`
- `envmap "..." { ... }`
- `mesh "..." { ... }`
- `light { ... }`

注释支持：

- `#`
- `//`

## `render` 块可配置参数

用于控制渲染质量和输出行为：

```text
render {
    width = 960
    height = 720
    spp = 64
    depth = 8
    shadows = true
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
    bg = 0.055 0.055 0.058
}
```

参数说明：

- `width` / `height`：输出分辨率
- `spp`：每像素最大采样数
- `depth`：路径最大递归深度
- `shadows`：是否启用阴影射线
- `adaptiveSampling`：是否启用自适应采样
- `adaptiveMinSpp`：每像素至少采多少后才允许提前停止
- `adaptiveBatch`：每隔多少个样本重新检查一次
- `adaptiveThreshold`：提前停止阈值，越小越保守
- `denoise`：是否启用内置降噪输出
- `denoiseRadius`：降噪窗口半径
- `denoiseSigmaSpatial`：空间距离权重
- `denoiseSigmaColor`：颜色差异权重
- `denoiseSigmaNormal`：法线差异权重
- `denoiseSigmaAlbedo`：反照率差异权重
- `bg`：未命中物体时的背景颜色

## `camera` 块可配置参数

用于控制构图和视角：

```text
camera {
    pos = 0.162 0.134 0.194
    lookAt = 0.026 0.073 0.002
    up = 0.0 1.0 0.0
    fov = 25
    aperture = 0.0
    focus = 0.228
}
```

参数说明：

- `pos`：相机位置
- `lookAt`：观察目标点
- `up`：画面上方向参考
- `fov`：视场角
- `aperture`：景深光圈参数
- `focus`：对焦距离

常见改法：

- 想更俯视：提高 `pos.y`
- 想更近景：让 `pos` 更接近物体
- 想压透视：减小 `fov`
- 想更广角：增大 `fov`

## `envmap` 块可配置参数

用于指定 HDR 环境光：

```text
envmap "../../envmaps/typewriter_studio_soft.hdr" {
    intensity = 0.18
}
```

参数说明：

- 路径字符串：HDR 文件路径，相对当前 `desktop_scene/`
- `intensity`：环境光强度

## `mesh` 块可配置参数

这是你增删改场景物体时最常操作的块。

示例：

```text
mesh "typewriter/typewriter.obj" {
    materialMap = "typewriter/typewriter.materialmap"
    offset = 0.024 0.062 -0.002
    rotation = 0.000 180.000 0.000
    scale = 1.0 1.0 1.0
}
```

支持的常见字段：

- `material`
- `materialMap`
- `color`
- `offset`
- `rotation`
- `scale`
- `ior`
- `roughness`
- `emission`
- `diffuseTex`
- `normalTex`
- `bumpTex`
- `roughnessTex`
- `bumpScale`

### 新增一个模型

例如新增一个道具：

```text
mesh "my_prop/prop.obj" {
    material = MICROFACET
    color = 0.7 0.7 0.7
    offset = 0.020 0.015 -0.010
    rotation = 0.000 45.000 0.000
    scale = 1.0 1.0 1.0
}
```

### 删除一个模型

直接删掉对应的 `mesh { ... }` 块即可。

### 调模型位置

优先改：

- `offset`
- `rotation`
- `scale`

### 给复杂模型分不同材质

优先给它配一个：

- `materialMap = "xxx.materialmap"`

例如打字机和桌子就是这样管理的。

## `light` 块可配置参数

用于控制场景布光：

```text
light {
    type = AREA_DISK
    pos = 0.028 0.118 0.002
    normal = 0.000 -1.000 0.000
    radius = 0.040
    emission = 1.00 0.97 0.92
    intensity = 16.0
    enabled = true
}
```

支持灯光类型：

- `AREA_RECT`
- `AREA_DISK`
- `AREA_SPHERE`
- `POINT`
- `SPOT`
- `DIRECTIONAL`

支持字段：

- `type`
- `pos`
- `dir`
- `normal`
- `size`
- `radius`
- `emission`
- `intensity`
- `range`
- `innerAngle`
- `outerAngle`
- `enabled`
- `showModel`

### 新增一盏灯

直接新增一个 `light { ... }` 块即可。

### 删除一盏灯

删掉对应 `light { ... }` 块即可。

### 临时关闭一盏灯

写：

```text
enabled = false
```

### 让灯体可见

对面积灯加：

```text
showModel = true
```

## 推荐编辑工作流

如果你是从主展示场景出发做一个新版本，推荐这样做：

1. 复制 `typewriter_table_showcase.scene`
2. 先改相机，确定构图
3. 再改模型摆位
4. 再调灯光
5. 最后调 `render` 里的采样和降噪参数

## 从哪个场景开始最合适

- 想做最终展示图：从 `typewriter_table_showcase.scene` 开始
- 想专门看材质差异：从 `typewriter_material_test.scene` 开始
- 想了解灯光系统：从 `light_types_demo.scene` 或 `area_light_shapes_demo.scene` 开始

## 相关文档

- 总体场景语法说明：`Project2/configs/README.md`
- 源码场景模块说明：`Project2/src/scene/README.md`
- 材质分配与 `.materialmap`：`Project2/src/material/README.md`

## 一句话建议

如果你只是想做自己的展示版本，不要直接从零写一个 `.scene`，最省时间的方法通常是：

- 复制 `typewriter_table_showcase.scene`
- 改名字
- 在副本上做相机、灯光和模型摆位微调
