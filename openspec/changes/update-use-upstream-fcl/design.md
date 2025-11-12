# Design: �ں˲��ֿ����� FCL ԭ������Һ���

## 背景
��ǰ��ײ���㷨���ڵİ汾�����������ڵ�ԭ�� FCL �����У����������Լ��� GJK/EPA ������������

## 目标
1. �ں�̬ API ��ȫʹ�� FCL upstream �ṩ�㷨��
2. ����������ض���ƣ�ֻ���� allocator/logging/NTSTATUS ��̬���޸ġ�
3. �������ַ/�ѱ�Ա��Դ�����ͨ������֤����

## 接入架构
1. **Geometry Handle Adapter**
   - ���� FCL upstream �� `CollisionObject` ʵ�������ں� `FCL_GEOMETRY_HANDLE`
   - ��֤���ڴ�����ʹ�� FclPoolAllocator (NonPagedPool)

2. **Computation Bridge**
   - `FclCollisionDetect` 直接���� FCL upstream `fcl::collide`
   - `FclDistanceCompute` ���� `fcl::distance`
   - `FclContinuousCollision` ���� `fcl::continuousCollision` / `ConservativeAdvancement`
   - �쳣�� try/catch(...) -> NTSTATUS

3. **Configuration Overrides**
   - ͨ�� trait ���߻ص�������Ϊ upstream Eigen/libccd/FCL ���� allocator/log hook
   - ���û������ڴ����������� driver pool

4. **检测策略**
   - �����ȶ������ܲ��Թ淶 (Sphere/Sphere, Sphere/Mesh, Mesh/Mesh)
   - Compare result with upstream unit tests if possible (PC-based harness)
