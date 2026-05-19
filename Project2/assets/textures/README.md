# assets/textures

这个目录存放共享贴图资源。

## 常见贴图类型

- `diffuse` 或 `albedo`
- `normal`
- `bump`
- `roughness`
- `gloss`

## 贴图可以从哪里引用

- `.scene` 文件中的 mesh 块
- `.materialmap` 中针对 OBJ 子材质的覆盖
- 原始 `.mtl` 文件

## 说明

- 路径建议写成相对于引用它的 `.scene` 或 `.materialmap`。
- 法线贴图和凹凸贴图是分开的。
- 粗糙度贴图也可以按光泽图解释。
