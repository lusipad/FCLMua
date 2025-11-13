FCL+Musa ����ָ��
=================

1. ǰ������
------------
- Windows 10/11 x64 ���Ի�
- �����ԱȨ�ޣ�Ҫ�ܵ��� `signtool` �� `certutil`
- ��װ WDK/VS �Ա���루��ֱ��ʹ�ñ���õ� .sys�����Թ���
- `tools/manual_build.cmd` ���Զ���ʼ�� VsDevCmd + WDK ����������������Σ������ٻ�；`build_driver.cmd` �ڲ���¿�ѯ����֤����Ȼ���øýű���ɵ���룬����Ҫ�û����롣

2. ����
--------
```powershell
PS> cd D:\Repos\FCL+Musa
PS> tools\manual_build.cmd
# ���ߣ�
PS> .\build_driver.cmd
```
- ���`kernel/FclMusaDriver/out/x64/Debug/FclMusaDriver.sys`
- ʹ�� `build_driver.cmd` ���з���ʱ���Զ����� `tools/sign_driver.ps1` ������ `FclMusaTestCert.pfx` �� `FclMusaTestCert.cer`；ε�� `tools/manual_build.cmd` ʱ����Ҫ�ֶ�������ﴦ��ǩ����

3. ���뾫��֤��
----------------
- ��������ɵ� `FclMusaTestCert.cer` ��Ŀ�������ϣ�
- �ڲ������е� Trusted Root �� Trusted Publisher �����У�
```cmd
certutil -addstore Root C:\path\FclMusaTestCert.cer
certutil -addstore TrustedPublisher C:\path\FclMusaTestCert.cer
```
- ����������� Win10 ���棬���Ը��ݲ���Ҫ�ع� `bcdedit /set testsigning on`

4. ��װ����
-------------
```cmd
sc create FclMusa type= kernel binPath= C:\path\FclMusaDriver.sys
sc start FclMusa
```
- �����ԱȨ��

5. ����/����
--------------
- ���� IOCTL��ʾ��α���룩��
```c
HANDLE h = CreateFile("\\\\.\\FclMusa", ...);
FCL_PING_RESPONSE ping = {};
DeviceIoControl(h, IOCTL_FCL_PING, nullptr, 0, &ping, sizeof(ping), ...);
```
- �Բ⣺`IOCTL_FCL_SELF_TEST`

6. ж��
--------
```cmd
sc stop FclMusa
sc delete FclMusa
```

7. Ŀ¼�ṹ���ĵ�/���룩
-----------------------
- `docs/api.md`��API ˵��
- `docs/usage.md`��ʹ��ָ��
- `docs/architecture.md`���ܹ�����
- `docs/testing.md`�����Ա���
- `docs/known_issues.md`����֪����
- `docs/release_notes.md`������˵��
### FCL 上游信息
部署时请确保同时同步 fcl-source/ 目录 (当前基于 commit 5f7776e2101b8ec95d5054d732684d00dac45e3d)，以保证驱动使用的碰撞/距离/CCD 算法与 upstream FCL 一致。
