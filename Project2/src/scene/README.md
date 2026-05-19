# scene 模块

这个模块是整个渲染器的“中枢”。它一边负责把文本场景配置转成真正的运行时对象，一边负责路径追踪积分本身。

简单说：

- `SceneLoader` 负责“把场景搭起来”
- `Scene` 负责“把光线算出来”

这也是为什么这个模块是最适合先看、也最值得重点写清楚的模块之一。

## 主要文件

- `Scene.hpp` / `Scene.cpp`：运行时场景、顶层物体列表、灯光列表、BVH 和 `castRay()`
- `SceneLoader.hpp` / `SceneLoader.cpp`：`.scene`、`.materialmap`、OBJ/MTL、灯光和 envmap 的解析与装配

## 这个模块在程序里做了什么

### `SceneLoader`

负责把下面这些输入资源组织起来：

- `.scene`
- `.materialmap`
- OBJ / MTL
- 贴图
- envmap

然后生成：

- `Scene`
- `Camera`
- `Material`
- `Texture`
- `Object`

### `Scene`

负责保存运行时渲染所需的信息：

- 物体列表
- 解析灯列表
- 环境图
- 采样参数
- 阴影开关
- 自适应采样和降噪参数

并在 `castRay()` 中完成真正的路径积分。

## 场景文件是怎么工作的

项目使用的主场景入口是：

- `<scene_dir>/<view_name>.scene`

它不是 Blender 那种完整场景文件，而是一个项目内自定义的文本描述格式。它的优势是：

- 好读
- 好改
- 适合协作
- 适合快速做多个版本场景

## 当前支持的顶层块

- `render { ... }`
- `camera { ... }`
- `envmap "..." { ... }`
- `mesh "..." { ... }`
- `light { ... }`

注释支持：

- `#`
- `//`

## `render` 块怎么用

这个块负责控制渲染参数，而不是控制场景物体本身。

例如：

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

它主要会影响：

- 分辨率
- 最大采样数
- 最大路径深度
- 阴影开关
- 自适应采样
- 内置降噪

## `camera` 块怎么用

这个块负责控制相机位置和构图：

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

如果你在做“同一场景不同构图版本”，最常改的就是这里。

## `mesh` 块怎么用

这是最重要的可编辑入口之一。

一个模型通常这样写：

```text
mesh "typewriter/typewriter.obj" {
    materialMap = "typewriter/typewriter.materialmap"
    offset = 0.024 0.062 -0.002
    rotation = 0.000 180.000 0.000
    scale = 1.0 1.0 1.0
}
```

你可以在这里控制：

- 模型路径
- 平移
- 旋转
- 缩放
- 是否直接指定一种材质
- 是否使用 `.materialmap`
- 是否覆盖贴图或粗糙度等参数

### 想新增模型

最简单的方法就是：

1. 把 OBJ 放到合适目录
2. 在 `.scene` 加一个 `mesh "..." { ... }`
3. 必要时给它配一个 `.materialmap`

### 想删除模型

直接删掉对应 `mesh` 块即可，不需要改代码。

### 想改模型摆放

直接改：

- `offset`
- `rotation`
- `scale`

这也是场景搭建阶段最常见的改法。

## `light` 块怎么用

这部分负责解析灯光：

```text
light {
    type = AREA_DISK
    pos = 0.028 0.118 0.002
    normal = 0.000 -1.000 0.000
    radius = 0.040
    emission = 1.00 0.97 0.92
    intensity = 16.0
}
```

你可以用它做：

- 主光
- 辅光
- 轮廓光
- 产品展示布光

如果不想删灯，只想临时关闭：

- 写 `enabled = false`

如果想让面积灯本体在画面里可见：

- 写 `showModel = true`

## `envmap` 怎么用

环境图适合提供基础氛围光：

```text
envmap "../../envmaps/typewriter_studio_soft.hdr" {
    intensity = 0.18
}
```

没有环境图时，miss 路径会回退到 `backgroundColor`。

## 场景编辑最常见的三类工作

### 1. 搭场景

主要改：

- `mesh`
- `camera`
- `light`

### 2. 调材质

主要改：

- `mesh.material`
- `mesh.materialMap`
- `.materialmap`

### 3. 调渲染质量

主要改：

- `render.spp`
- `render.adaptiveSampling`
- `render.denoise`

## 想扩展 `.scene` 语法怎么做

这也是这个模块最重要的“可维护性入口”之一。

标准流程通常是：

1. 在 `SceneLoader.hpp` 的描述结构里加字段
2. 在 `SceneLoader.cpp` 的 parser 里加新关键字解析
3. 在 build scene 阶段把解析结果写进 `Scene` 或其他运行时对象
4. 更新 README 和示例场景

例如这次新增自适应采样和内置降噪参数，就是按这个路线接进去的。

## 想新增一种顶层块怎么办

比如你以后想加：

- `postprocess { ... }`
- `animation { ... }`
- `include "..."` 

就需要改：

- tokenizer 不一定要动
- `Parser::parse()` 顶层分支要加
- 对应的描述结构要补
- 最终运行时装配逻辑要补

## `castRay()` 当前做了什么

当前积分器已经包含：

- 普通相交查询
- miss 路径环境图采样
- 自发光表面返回
- delta 材质分支
- 解析灯直接光
- 按灯光功率采样
- 环境图直接采样 + MIS
- BSDF 间接 bounce
- 俄罗斯轮盘赌

所以如果你想改“画面最终长什么样”，很多时候最终都要回到：

- `Scene.cpp`

## 常见改动怎么分配到哪里

### 想扩展场景文件语法

看：

- `SceneLoader.hpp`
- `SceneLoader.cpp`

### 想改渲染积分逻辑

看：

- `Scene.cpp`

### 想改默认场景参数

看：

- `SceneLoader.hpp` 中的描述结构默认值
- `Scene.hpp` 中运行时默认值

### 想改 OBJ/MTL 到材质的映射

看：

- `SceneLoader.cpp`
- `material/`

## 使用时的建议

- 正式展示场景尽量写完整 `.scene`，不要依赖“目录扫描 fallback”
- 复杂 OBJ 优先配 `.materialmap`
- 搭场景时建议先定相机，再摆模型，再补灯光
- 做参数对比时，复制一个新的 `.scene` 版本往往比反复手改更清晰

## 运行命令
./build/Project2Renderer assets/models/desktop_scene typewriter_table_showcase 800x600 256

## 协作建议

这个模块非常适合拆成两类任务：

- 一人维护 `SceneLoader` 和场景语法
- 一人维护 `Scene.cpp` 的积分逻辑

因为它们虽然都属于 `scene/`，但一个偏“配置解析”，一个偏“渲染算法”。
