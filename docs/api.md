FCL+Musa Driver API �ĵ�
========================

> ˵�������ļ��������ǵ� 1 �� **FCL �ں˹���ģ��** ��¶�� C �ӿڡ�  
> IOCTL ��ֻ������û�̬ buffer ӳ�䵽��Щ API�������Ӷ�������߼���

## ��ʼ��

### NTSTATUS FclInitialize();
- ��ʼ���ڲ��ڴ�ϵͳ��Musa.Runtime���Բ����ģ�飻
- �ѳ�ʼ��������ٴε��÷��� `STATUS_ALREADY_INITIALIZED`��

### VOID FclCleanup();
- �ͷ��ڲ���Դ��ֹͣ�ڴ�ͳ�ƣ�
- ����ж�ػ���Խ���ʱ���á�

## ���ι���

### NTSTATUS FclCreateGeometry(FCL_GEOMETRY_TYPE type, const VOID* geometryDesc, FCL_GEOMETRY_HANDLE* handle);
- ���� Sphere / OBB / Mesh ���ζ���
- ������
  - Sphere: `FCL_SPHERE_GEOMETRY_DESC`
  - OBB: `FCL_OBB_GEOMETRY_DESC`
  - Mesh: `FCL_MESH_GEOMETRY_DESC`
- �������Ч�� `FCL_GEOMETRY_HANDLE`��

### NTSTATUS FclDestroyGeometry(FCL_GEOMETRY_HANDLE handle);
- ���ټ��ζ���ȷ���޻���ú��ͷš�

### NTSTATUS FclUpdateMeshGeometry(FCL_GEOMETRY_HANDLE handle, const FCL_MESH_GEOMETRY_DESC* desc);
- ���� Mesh ����/�������ݣ����ؽ�������ڲ� BVH��

### BOOLEAN FclIsGeometryHandleValid(FCL_GEOMETRY_HANDLE handle);
- У�鼸�ξ���Ƿ����ڼ��ι���ģ����ע�ᡣ

### NTSTATUS FclAcquireGeometryReference(FCL_GEOMETRY_HANDLE handle, FCL_GEOMETRY_REFERENCE* reference, FCL_GEOMETRY_SNAPSHOT* snapshot);
- ��ȡ���ο���������ײ / ���� / CCD ���㣻
- ���� `FCL_GEOMETRY_REFERENCE` �ڼ伸�β��ᱻ���٣�
- ʹ����ɺ������� `FclReleaseGeometryReference`��

## ��ײ / ���� API

### NTSTATUS FclCollisionDetect(FCL_GEOMETRY_HANDLE object1, const FCL_TRANSFORM* transform1, ...);
- ���� upstream FCL ִ����ײ��⣬֧�� Sphere / OBB / Mesh��
- ��� `FCL_CONTACT_INFO`�������Ӵ��㡢���ߺʹ�͸��ȡ�

### NTSTATUS FclCollideObjects(const FCL_COLLISION_OBJECT_DESC* object1, const ...);
- ������÷��ĸ߽׽ӿڣ���װ���ξ�� + �任��
- ���� `FCL_COLLISION_QUERY_RESULT`���Ƿ��ཻ + �Ӵ���Ϣ����

### NTSTATUS FclDistanceCompute(..., FCL_DISTANCE_RESULT* result);
- ������̾��뼰����㣬���� upstream FCL `distance` �㷨��
- ��� `FCL_DISTANCE_RESULT`��

## ������ײ��CCD��

### NTSTATUS FclInterpMotionInitialize(const FCL_INTERP_MOTION_DESC* desc, FCL_INTERP_MOTION* motion);
- ������ֹλ�˹������Բ�ֵ�˶�������

### NTSTATUS FclInterpMotionEvaluate(const FCL_INTERP_MOTION* motion, double t, FCL_TRANSFORM* transform);
- �� `[0,1]` ������������ֵ�˶�������м�λ�ˡ�

### NTSTATUS FclScrewMotionInitialize(const FCL_SCREW_MOTION_DESC* desc, FCL_SCREW_MOTION* motion);
- ���������˶���������ֹλ�� + �� + �ٶȲ�������

### NTSTATUS FclScrewMotionEvaluate(const FCL_SCREW_MOTION* motion, double t, FCL_TRANSFORM* transform);
- �� `[0,1]` ���������������˶���

### NTSTATUS FclContinuousCollision(const FCL_CONTINUOUS_COLLISION_QUERY* query, FCL_CONTINUOUS_COLLISION_RESULT* result);
- ���� upstream FCL ��������ײ�㷨��Conservative Advancement �ȣ����� TOI + �Ӵ���Ϣ��
- `query->Tolerance` / `MaxIterations` Ϊ 0 ʱ��ʹ��ģ���ڵĺ���Ĭ��ֵ��

## �Բ��뽡�����

### NTSTATUS FclRunSelfTest(FCL_SELF_TEST_RESULT* result);
- ִ�ж˵����Բ��ԣ���ʼ�� / ���� / ��ײ / CCD / ���׶� / Verifier ��س�������
- ��ÿ���ӳ�������Լ�����״̬д�� `FCL_SELF_TEST_RESULT`��

### NTSTATUS FclQueryHealth(FCL_PING_RESPONSE* response);
- Ϊ `IOCTL_FCL_PING` �ṩʵ�֣�
- ���������汾����ʼ��״̬�������������ʱ���Լ��ڴ��ͳ�Ƶȡ�

### NTSTATUS FclQueryDiagnostics(FCL_DIAGNOSTICS_RESPONSE* response);
- Ϊ `IOCTL_FCL_QUERY_DIAGNOSTICS` �ṩʵ�֣�
- �Ա��˺���/����/CCD �ں˲��Խ�����з�ʱ��˵���ͳ�ƣ�
  ���вֶζ�����Ϊ�������ʼ��ͳ��ʱ�䣨΢�룩�Լ���С/���ʱ�䡣

