# camera 模块

这个模块负责“从哪里看场景”。它把 `.scene` 文件里的相机参数转换成运行时相机对象，并在渲染时为每个像素样本生成主光线。

## 这个模块解决什么问题

如果把整个渲染器拆开来看：

- `scene/` 决定场景里有哪些物体和灯光。
- `material/` 决定表面如何反射或透射光。
- `camera/` 决定观察位置、观察方向和视场范围。

也就是说，画面构图、透视关系、俯视/平视角度、景深参数，首先都来自这里。

## 文件与作用

- `Camera.hpp`：相机类声明，对外暴露构造函数和 `generateRay()`
- `Camera.cpp`：look-at 坐标系构建、视锥计算、主光线生成

## 运行流程

这个模块在程序中的参与顺序大致是：

1. `scene/SceneLoader.cpp` 解析 `.scene` 中的 `camera { ... }`
2. 根据 `pos`、`lookAt`、`up`、`fov`、`aperture`、`focus` 创建 `Camera`
3. `renderer/Renderer.cpp` 在每个像素、每个样本上调用 `camera.generateRay(u, v, r1, r2)`
4. 主光线送入 `scene/Scene.cpp::castRay()` 做路径追踪

## `.scene` 里怎么用

典型写法：

```text
camera {
    pos      = 0.162 0.134 0.194
    lookAt   = 0.026 0.073 0.002
    up       = 0.0 1.0 0.0
    fov      = 25
    aperture = 0.0
    focus    = 0.228
}
```

字段含义：

- `pos`：相机位置
- `lookAt`：相机看向的目标点
- `up`：用于确定画面“上方”的参考方向
- `fov`：垂直视场角，值越大画面越广
- `aperture`：光圈大小，理论上用于景深
- `focus`：对焦距离，理论上与景深配合使用

## 当前实现状态

- 已实现标准的 look-at 相机模型
- 已实现基于 `fov` 和长宽比的主光线生成
- `aperture` / `focus` 参数已经接入场景与运行时结构
- 目前真正的薄透镜景深采样仍是占位状态，`aperture > 0` 时当前仍按 pinhole 相机行为工作

这意味着：

- 你现在可以正常调整构图、透视和视角
- 但如果你想得到真正的虚化效果，还需要继续完善 `Camera::generateRay()`

## 常见修改怎么做

### 改构图

最常见的是改 `.scene` 文件里的：

- `pos`
- `lookAt`
- `fov`

经验上：

- 想更接近物体：把 `pos` 往 `lookAt` 方向移动
- 想更俯视：提高 `pos.y`，同时适当提高 `lookAt.y`
- 想更广角：增大 `fov`
- 想更产品摄影：减小 `fov`，让透视更克制

### 改程序默认相机

如果你想改“没有显式配置时”的默认值，看：

- `scene/SceneLoader.hpp` 里的 `CameraSpec`

### 真正实现景深

如果你想继续做景深，主要入口是：

- `camera/Camera.cpp` 的 `Camera::generateRay()`

建议步骤：

1. 在镜头平面上按 `r1`、`r2` 采样圆盘
2. 用采样点偏移 ray origin
3. 计算焦平面上的 focal point
4. 让新的方向指向 focal point

## 想新增相机能力时改哪里

- 想加正交相机：新增一个相机类，或者扩展 `Camera` 的模式字段
- 想加真实景深：改 `Camera::generateRay()`
- 想加相机动画/轨道：通常不在这个模块里写死，而是在 `.scene` 或更上层逻辑生成参数

## 协作时怎么分工

这个模块适合单独分配给一位同学，因为它和其他模块的边界比较清晰：

- 对上游只依赖 `scene/` 提供参数
- 对下游只输出 `Ray`
- 不直接参与材质、灯光和积分逻辑
