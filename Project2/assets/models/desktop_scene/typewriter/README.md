# typewriter 资源包

这个目录包含打字机模型及其在本项目中的材质外观覆盖文件。

## 文件说明

- `typewriter.obj`：几何模型
- `typewriter.mtl`：原始材质定义
- `textures/`：原始贴图资源
- `typewriter.materialmap`：主场景使用的材质覆盖文件
- `typewriter_material_test.materialmap`：材质测试用覆盖文件

## 为什么需要 materialmap

`.materialmap` 用来把外部 MTL 材质名映射到渲染器内部材质类型和参数。
