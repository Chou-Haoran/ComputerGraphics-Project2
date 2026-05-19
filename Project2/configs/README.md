# 场景文件指南

这个目录只放文档说明，真正的 `.scene` 文件与场景资产放在一起，通常位于 `assets/models/<scene_name>/`。

## 加载规则

渲染器调用：

```text
./build/Project2Renderer <scene_dir> <view_name>
```

实际会读取：

```text
<scene_dir>/<view_name>.scene
```

`.scene` 内部的相对路径都相对 `<scene_dir>` 解析。

## 支持的顶层块

- `render { ... }`
- `camera { ... }`
- `envmap "..." { ... }`
- `mesh "..." { ... }`
- `light { ... }`

注释支持 `#` 和 `//`。

## render 块

```text
render {
    width   = 960
    height  = 720
    spp     = 64
    depth   = 8
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
    bg      = 0.055 0.055 0.058
}
```

常用字段说明：

- `width` / `height`：输出分辨率。
- `spp`：像素最大采样数。
- `depth`：路径最大递归深度。
- `shadows`：是否启用阴影测试。
- `adaptiveSampling`：是否启用自适应采样。
- `adaptiveMinSpp`：每个像素至少采多少样本后才允许提前停止。
- `adaptiveBatch`：每隔多少个样本重新评估一次提前停止条件。
- `adaptiveThreshold`：提前停止使用的相对误差阈值，越小越保守。
- `denoise`：是否输出内置后处理降噪图。
- `denoiseRadius`：联合双边滤波窗口半径。
- `denoiseSigmaSpatial`：空间距离权重。
- `denoiseSigmaColor`：颜色差异权重。
- `denoiseSigmaNormal`：法线差异权重。
- `denoiseSigmaAlbedo`：反照率差异权重。
- `bg`：没有命中物体时的背景颜色。

## camera 块

```text
camera {
    pos       = 0.162 0.134 0.194
    lookAt    = 0.026 0.073 0.002
    up        = 0.0 1.0 0.0
    fov       = 25
    aperture  = 0.0
    focus     = 0.228
}
```

## envmap 块

```text
envmap "../../envmaps/typewriter_studio_soft.hdr" {
    intensity = 0.18
}
```

## mesh 块

```text
mesh "typewriter/typewriter.obj" {
    materialMap = "typewriter/typewriter.materialmap"
    offset      = 0.024 0.062 -0.002
    rotation    = 0.000 180.000 0.000
    scale       = 1.0 1.0 1.0
}
```

常用字段：

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

## light 块

```text
light {
    type      = AREA_DISK
    pos       = 0.028 0.118 0.002
    normal    = 0.000 -1.000 0.000
    radius    = 0.040
    emission  = 1.00 0.97 0.92
    intensity = 16.0
    enabled   = true
}
```

支持灯光类型：

- `AREA_RECT`
- `AREA_DISK`
- `AREA_SPHERE`
- `POINT`
- `SPOT`
- `DIRECTIONAL`

支持字段：`type`、`pos`、`dir`、`normal`、`size`、`radius`、`emission`、`intensity`、`range`、`innerAngle`、`outerAngle`、`enabled`、`showModel`。

## .materialmap 示例

```text
materialMap {
    defaultType = MICROFACET
    defaultRoughnessMin = 0.18
}

mtl "Metal_Chrome" {
    type = METAL
    color = 0.85 0.85 0.84
    roughness = 0.10
    Ks = 0.95
}
```

## 中文速查模板

### 新增一个模型

```text
mesh "my_prop/prop.obj" {
    material = MICROFACET
    color    = 0.7 0.7 0.7
    offset   = 0.020 0.015 -0.010
    rotation = 0.000 45.000 0.000
    scale    = 1.0 1.0 1.0
}
```

### 桌子和打字机组合

```text
mesh "table/table.obj" {
    materialMap = "table/table.materialmap"
    offset = 0.000 0.000 -0.008
}

mesh "typewriter/typewriter.obj" {
    materialMap = "typewriter/typewriter.materialmap"
    offset = 0.024 0.062 -0.002
    rotation = 0.000 180.000 0.000
}
```

### 一个基础产品布光

```text
render {
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

light {
    type = AREA_DISK
    pos = 0.028 0.118 0.002
    normal = 0.000 -1.000 0.000
    radius = 0.040
    emission = 1.00 0.97 0.92
    intensity = 16.0
}

light {
    type = AREA_SPHERE
    pos = 0.086 0.082 -0.022
    radius = 0.015
    emission = 0.88 0.92 1.00
    intensity = 4.0
}
```

## 降噪与采样建议

- 想尽量保细节：减小 `denoiseRadius`，同时减小 `denoiseSigmaColor`。
- 想更强地压噪点：增大 `denoiseRadius` 或 `denoiseSigmaColor`。
- 场景边缘被抹糊：减小 `denoiseSigmaNormal` 和 `denoiseSigmaAlbedo`。
- 想把渲染时间压下来：保留 `adaptiveSampling = true`，并适当放宽 `adaptiveThreshold`。
- 如果已经高 `spp` 渲染，可以把 `denoise = false`，只输出原图。

### 显示可见灯体

```text
light {
    type = AREA_DISK
    pos = 0.0 0.15 0.0
    normal = 0.0 -1.0 0.0
    radius = 0.03
    emission = 1.0 1.0 1.0
    intensity = 12.0
    showModel = true
}
```

## CLI 覆盖

- `WxH` 覆盖 `render.width` 和 `render.height`
- `spp` 覆盖 `render.spp`
- `--no-shadow` 覆盖 `render.shadows`
- `--envmap=...` 覆盖环境图路径
- `--aperture=...` 覆盖 `camera.aperture`
- `--focus=...` 覆盖 `camera.focus`
