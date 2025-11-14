## ADDED Requirements

### Requirement: Kernel CCD motion profile
内核模式下的 FclCore SHALL 提供一套经过裁剪的连续碰撞检测（CCD）能力，用于支持球、盒和三角网格等刚体在给定时间区间内的碰撞查询，同时满足内核栈占用与稳定性的约束。

- 内核 CCD SHALL 支持以下运动模型：
  - 纯平移（TranslationMotion）：物体在固定姿态下沿一条轨迹平移；
  - 线性插值旋转（InterpMotion）：物体在起始姿态与目标姿态之间做线性插值运动。
- 内核 CCD SHALL 支持使用 Conservative Advancement + GJK 的求解方式，用于给出时间接触（TOI）与是否相交的判定。
- 对于不在支持范围内的 motion/solver 组合，内核 CCD SHALL 返回明确的错误状态（例如 NotSupported），而不是进入未定义行为。

#### Scenario: Translation CCD with convex meshes
- **WHEN** 调用方提交一个仅涉及平移的 CCD 查询（两端姿态的旋转部分一致），几何为球/盒/中等复杂度的三角网格  
- **THEN** 内核 CCD SHALL 正确返回是否发生连续碰撞以及对应的时间接触（TOI），其结果与 R3 参考实现的差异在预先定义的容差范围内  
- **AND** 内核栈使用 SHALL 保持在约定阈值以内（例如关键 CCD 调用栈深低于某个固定值），不会触发栈溢出或蓝屏

#### Scenario: InterpMotion CCD with moderate rotation
- **WHEN** 调用方使用线性插值运动（InterpMotion）描述刚体起止姿态，并在内核 CCD 中发起 CCD 查询  
- **THEN** 内核 CCD SHALL 使用内核友好的 InterpMotion 实现，对角速度与姿态插值进行稳定计算  
- **AND** 对于球/盒/中等复杂 mesh 的典型场景，R0 CCD 返回的 TOI/是否相交与 R3 FCL 参考实现保持一致或在约定误差内  
- **AND** 即便输入在边界条件（高角速度、小角度、接近接触）附近，内核 CCD 也 SHALL 通过 NTSTATUS 报错或给出稳定结果，而不是导致 BugCheck

### Requirement: CCD safety and failure modes in kernel
内核 CCD 能力 MUST 明确其失败模式与资源约束，保证在任何输入下都不会破坏内核稳定性。

- 对于超过几何规模/运动范围限制的 CCD 请求，内核 CCD SHALL 返回错误状态并记录日志，而非强行进入计算路径；
- 在 Conservative Advancement/GJK 内部出现数值异常或 memory allocation 失败时，内核 CCD SHALL 捕获异常并转化为 NTSTATUS 返回；
- 对于当前 Kernel profile 未支持的 CCD 组合（例如某些 motion 类型或 solver 类型），内核 CCD SHALL 明确拒绝，并在文档中列出限制。

#### Scenario: Unsupported CCD combination
- **WHEN** 调用方请求了 Kernel profile 未支持的 CCD 组合（例如带 SplineMotion 的 CCD）  
- **THEN** 内核 CCD SHALL 不进入实际计算路径，而是快速返回 NotSupported/InvalidParameter 等错误码  
- **AND** 驱动日志 SHALL 记录一次告警，指示调用方当前配置不受支持  
- **AND** 在这种情况下，内核绝不发生蓝屏或内核栈溢出

