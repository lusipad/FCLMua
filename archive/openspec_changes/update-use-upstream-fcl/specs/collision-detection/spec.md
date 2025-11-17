## ADDED Requirements

### Requirement: 复用 Upstream FCL 算法
�ں�̬碰撞/距离/CCD ���� **SHALL** ���� FCL upstream �������㷨ʵ�֣�ֻ���� allocator��日志��NTSTATUS ת�� ��Դ�����ϼ���Ӧ�á�R0 ���Բ�ԭ�ٵ� GJK/EPA/BVH/CCD �㷨���ɡ�

#### Scenario: 算法复用
- **WHEN** `FclCollisionDetect` �� `FclDistanceCompute` �� `FclContinuousCollision` �������ں˵���
- **THEN** ����Ӧ������ FCL upstream ���Ӧ API (`fcl::collide`/`fcl::distance`/Conservative Advancement)
- **AND** �κη���߻ص������޶������ڴ�/��־/�쳣ת��

#### Scenario: 允许的机制性改造
- **WHEN** ���� allocator/logging/NTSTATUS hook Ϊ���� PASSIVE_LEVEL��Verifier ����
- **THEN** �Ա��������޸�ֻ���ں��ܣ������κ� FCL �㷨ʵ�ֱ���
- **AND** ��ƫ���� bagches �汾����Ҫͨ��ͬ��Դ�����ض���������

#### Scenario: 禁止自定义算法
- **WHEN** ������Ҫ���·¶�� new `FclGjkIntersect` ��ͬ�㷨
- **THEN** ���뱣������ upstream FCL Ҫ��ʵ�֣����߶�������ԭ�뼼���޸ķֽ�
- **AND** �����׼�������Է���ǿ����飬ȷ���� �������� upstream ���и���

### Requirement: Upstream 等效性验证
��λ���� **SHALL** ͨ��自动化�Լ��ɲ��Խ����֤ R0 实现与 upstream FCL ������ͬ���飨���룬����、CCD���Լ���Լ��

#### Scenario: 离散碰撞回归
- **WHEN** ���б��ᵽ�������㷨���µĽӿڹ��
- **THEN** �� Sphere/Sphere��Sphere/OBB��Mesh/Mesh �����½�� upstream 对照测试
- **AND** ���Գ�����ֱֵ�ӱȽϣ����δƥ��ʱ���Ὣ�쳣��¼

#### Scenario: CCD/距离对齐
- **WHEN** ρ������ʹ�� Conservative Advancement / distance API
- **THEN** �����ظ� upstream CCD/distance ��ͬ�İ���ڵ�ɲ���
- **AND** ���κ����ָ�ֵ�����Ԥ��Ͷ���ֹһ����������

### Requirement: Upstream 版本�̺����
���� **SHALL** �������� FCL upstream tag/commit ���ں˲��ֿ��ʹ�ã��Լ������Ϣ��Ϊ ���ɲ��������õػ���ͬ������ĵ���

#### Scenario: Commit 锁定
- **WHEN** ���������ע����Ϊ���ɲ��
- **THEN** �ں�˵� Build ������Ӧ����ҵ upstream FCL commit/tag ��˵���� (例如 README/props)
- **AND** ���������޲���Ϣ��ɾ��ǿ��ͬ���޸�

#### Scenario: 差异追踪
- **WHEN** ���� allocator / logging / NTSTATUS hook ����
- **THEN** �� doc/design ���ṩ hook �� upstream ���ɶԱ�
- **AND** ʹ���κ���ص��Լ� patch ���� delta files �����Է��ר����վ����
