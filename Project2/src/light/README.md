# light 模块

这个模块负责“光从哪里来，以及如何被更有效地采样”。

它覆盖三类光源：

- `.scene` 明确配置的解析灯光
- 带 emission 的自发光网格
- HDR 环境图

## 为什么要单独做这个模块

早期路径追踪器常见做法是：

- 面光源靠几何采样
- 点光源单独写一套
- 环境光再单独写一套

这样代码会越来越散，后续很难扩展。这个项目已经把大部分灯光采样统一到：

- `Light`
- `LightSample`
- `sampleLi()`

这样做的好处是：

- 新灯光类型更容易接入
- `scene/Scene.cpp` 的直接光逻辑更统一
- 可以继续扩展 MIS、功率采样和可见灯体

## 主要文件

- `Light.hpp`：统一灯光基类与 `LightSample`
- `LightType.hpp`：`.scene` 中可写的灯光类型枚举
- `LightSpec.hpp`：解析阶段的灯光描述
- `AreaRectLight.hpp`
- `AreaDiskLight.hpp`
- `AreaSphereLight.hpp`
- `PointLight.hpp`
- `SpotLight.hpp`
- `DirectionalLight.hpp`
- `MeshLight.hpp`：把自发光网格包装成统一灯光对象
- `EnvironmentMap.hpp` / `EnvironmentMap.cpp`：HDR 环境图
- `AreaLight.hpp`：早期兼容保留结构

## 当前支持的灯光类型

- `AREA_RECT`
- `AREA_DISK`
- `AREA_SPHERE`
- `POINT`
- `SPOT`
- `DIRECTIONAL`

另外还有一种不是在 `.scene light {}` 里直接写的：

- `MeshLight`，由自发光网格自动转换而来

## `.scene` 里怎么用

一个典型的灯光块长这样：

```text
light {
    type      = AREA_DISK
    pos       = 0.028 0.118 0.002
    normal    = 0.000 -1.000 0.000
    radius    = 0.040
    emission  = 1.00 0.97 0.92
    intensity = 16.0
    enabled   = true
    showModel = false
}
```

常用字段：

- `type`
- `pos`
- `dir` / `normal`
- `size`
- `radius`
- `emission`
- `intensity`
- `range`
- `innerAngle`
- `outerAngle`
- `enabled`
- `showModel`

不同灯光类型会用到不同字段。

## 各灯光适合什么场景

### `AREA_RECT`

- 适合模拟软箱、窗户、顶灯面板
- 更适合产品展示布光

### `AREA_DISK`

- 适合聚焦感更强的圆形软光源
- 很适合桌面展示、摄影棚顶光

### `AREA_SPHERE`

- 适合小球泡、发光球、装饰灯
- 如果 `showModel = true`，画面里会真的看到发光球

### `POINT`

- 简单快速
- 适合做辅助光、补光、测试光
- 但物理上没有真实面积，阴影边缘会更硬

### `SPOT`

- 适合手电、射灯、舞台灯
- 通过 `innerAngle` / `outerAngle` 控制锥形范围

### `DIRECTIONAL`

- 适合太阳光、远距离平行光
- 没有位置，只有方向

## 自发光网格怎么接入

如果某个模型材质是 `EMIT`，或者配置了非零 `emission`：

- 几何模块会把它当成有面积的发光表面
- `SceneLoader` 会把它自动注册成 `MeshLight`
- 之后它也会参与统一灯光采样

这意味着你可以用两种方式做“灯”：

1. 用 `light { ... }` 显式写解析灯光
2. 让一个网格自己发光

## 当前采样策略

当前灯光系统已经具备这些特性：

- 统一的 `sampleLi()` 接口
- 面向直接光的采样与 PDF 返回
- 解析灯按灯光功率做重要性采样
- 环境图支持直接采样与 MIS
- 自发光网格接入统一灯光路径

这对多灯场景很重要，因为不再是“每次都把所有灯遍历一遍”。

## 常见改动怎么做

### 新增一盏灯

最简单的方法是改 `.scene` 文件，新增一个 `light { ... }` 块。

### 删除一盏灯

直接从 `.scene` 删除对应的 `light` 块即可。

### 临时关闭一盏灯

把：

```text
enabled = false
```

写进对应灯光块。

### 想让灯体在画面里可见

对面积灯打开：

```text
showModel = true
```

这样 `SceneLoader` 会额外生成一个可见的发光几何体。

### 想增强或减弱某盏灯

优先改：

- `intensity`

如果要改色温/颜色氛围，再改：

- `emission`

## 想新增一种灯光类型怎么做

通常步骤是：

1. 在 `LightType.hpp` 增加类型
2. 写一个新的 `xxxLight` 类继承 `Light`
3. 实现 `sampleLi()`，必要时实现 `samplingWeight()`
4. 在 `scene/SceneLoader.cpp` 的 `parseLight()` 与构建分支里接入
5. 如果需要可见灯体，再补 `showModel` 生成逻辑

最关键的是：

- 要正确返回 radiance
- 要正确返回 pdf
- 要说明自己是不是 `delta` 光源

## 使用时的注意事项

- 面积灯更适合产品渲染，但成本通常比点光更高
- 点光和方向光更便宜，但真实感较弱
- 环境图适合做基础氛围光，不一定足够塑形
- 多灯场景优先考虑主光、辅光、轮廓光分层，不要一味堆强度

## 协作建议

这个模块适合这样分工：

- 一人维护 `.scene` 灯光语法与解析
- 一人维护具体灯光类
- 一人维护 envmap 与 MIS

这样灯光配置、采样实现和场景演示可以相对独立推进。
