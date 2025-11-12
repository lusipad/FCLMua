# FCL+Musa Driver

�� Windows �ں� (Ring?0) ����ֲ FCL��Flexible Collision Library������� Musa.Runtime/Eigen/libccd��ʵ�� Sphere/OBB/Mesh ��ײ��������ײ (CCD)��BVH/OBBRSS ֧���Լ������Լ졣

## ����

```powershell
PS> git clone �� FCL+Musa
PS> cd FCL+Musa
PS> .\build_driver.cmd         # ���� kernel/FclMusaDriver/out/x64/Debug/FclMusaDriver.sys
```

������WDK 10.0.26100.0��Visual Studio 2022��Musa.Runtime���ֿ��Ѱ�������

����ɹ���Ժ󣬽ű����Զ�ִ�� `tools/sign_driver.ps1`��
- ��� `CN=FclMusaTestCert` ��ǩ��֤�����ڱ��أ��ű����Զ������Զ����� SHA-256 ǩ��֤����
- �Զ�ʹ�� `signtool` ���� `FclMusaDriver.sys` ���¼���ǩ������������ `kernel/FclMusaDriver/out/x64/Debug/` �� `FclMusaTestCert.pfx` �� `FclMusaTestCert.cer` ��

���ƽ����ɵ� `.cer` �ļ�����Ŀ�������ϣ���� `Trusted Root Certification Authorities` �� `Trusted Publishers` �����У�

```cmd
certutil -addstore Root kernel\FclMusaDriver\out\x64\Debug\FclMusaTestCert.cer
certutil -addstore TrustedPublisher kernel\FclMusaDriver\out\x64\Debug\FclMusaTestCert.cer
```

## ��װ / ����

```cmd
sc create FclMusa type= kernel binPath= C:\path\FclMusaDriver.sys
sc start FclMusa
```

> ǩ�����Ѿ�ʹ�� `CN=FclMusaTestCert` ����ǩ�������ڲ��Ի���������򽫶�Ӧ `.cer` ���뵽 Trusted Root �� Trusted Publisher ���档

## IOCTL ����

| IOCTL | ˵�� |
|-------|------|
| `IOCTL_FCL_PING` | ��ѯ�汾����ʼ��״̬���ڴ�ͳ�� |
| `IOCTL_FCL_SELF_TEST` | ִ���Լ죨����/��ײ/CCD/ѹ��/Verifier �ȣ� |
| `IOCTL_FCL_CREATE_SPHERE` / `IOCTL_FCL_DESTROY_GEOMETRY` | �������ξ�� |
| `IOCTL_FCL_SPHERE_COLLISION` | Demo�������ڲ��������岢������ײ��� |
| `IOCTL_FCL_CONVEX_CCD` | Demo��ʹ�� InterpMotion ����������ײ |
| `IOCTL_FCL_QUERY_COLLISION` / `IOCTL_FCL_QUERY_DISTANCE` | ͨ����ײ/�����ѯ |
| `IOCTL_FCL_CREATE_MESH` | ���� OBJ ��Դ����������� (FCL_GEOMETRY_MESH) |

����ṹ/�ֶμ� `docs/api.md`��`docs/demo.md`��

## �û�̬ʾ��

```powershell
PS> tools\build_demo.cmd
PS> tools\build\fcl_demo.exe
```

���� CLI ���ṩ������ʵʱ����/���� IOCTL ʾ����
- `load <name> <obj>`���� OBJ �����������
- `sphere <name> <radius> [x y z]`�����̶�����
- `move` / `collide` / `distance`������������λ�úͲ��Ի���
- `simulate` �� `ccd`�������ڲ���λ�ƶ��������ܡ�
- `destroy` �� `list` ���������ջ�
- ���� CLI ���ڵ� `tools\build`  Ŀ¼��ֱ�� `run scenes\two_spheres.txt` ������������
  - `scenes\two_spheres.txt`��˫Բ������
  - `scenes\mesh_probe.txt`���� `assets\cube.obj` ��Կ�� + ����ҩ���
  - `scenes\arena_mix.txt`����ܺ���� + ˫Բ�໥����

## �ں�ʾ��

�� Driver �����У�PASSIVE_LEVEL���ɲο� `docs/demo.md`��ֱ�ӵ��� `FclCreateGeometry`��`FclCollideObjects`��`FclContinuousCollision` �� API��ʹ��ǰȷ�� `FclInitialize()` �ɹ���ж��ǰ���� `FclCleanup()`��

## �ĵ�

- `docs/api.md`��API ˵��
- `docs/usage.md`��ʹ��ָ��
- `docs/architecture.md`���ܹ�����
- `docs/testing.md`�����Ա���
- `docs/known_issues.md`����֪����
- `docs/release_notes.md`������˵��
- `docs/deployment.md`����������
- `docs/demo.md`��Ring0/Ring3 ʾ��

## ��״

- Debug ������ʵ�� BVH/OBBRSS��GJK/EPA��InterpMotion/ScrewMotion��Conservative Advancement CCD��
- �Բ⸲�Ǽ��θ��¡�Sphere/OBB������ Mesh���߽硢������ײ��Driver Verifier��ѹ��/���ܵ�·������ͨ�� `IOCTL_FCL_SELF_TEST` ��ȡ�������
- TODO��� WinDbg ������·��׼�� Release ǩ�������� WinDbg/Verifier �����ֲ�ȣ���� `docs/known_issues.md`����
### Upstream FCL 版本
- 仓库内置 upstream FCL 源码 (目录 fcl-source/)，当前使用的 commit 为 5f7776e2101b8ec95d5054d732684d00dac45e3d。
- 驱动碰撞/距离/CCD 接口通过 upstream_bridge.cpp 直接调用 FCL 的 collide、distance、continuousCollide，内核仅保留内存与日志 hook。
- 打包/部署时同步 fcl-source/ 目录即可保持与上游一致，如需升级请显式记录新的 commit。
