[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter()]
    [string]$DevicePath = '\\.\FclMusa',

    [Parameter()]
    [switch]$Json,

    [Parameter()]
    [switch]$SkipPing
)

Set-StrictMode -Version Latest

$typeName = 'FclSelfTestTool.Native'
if (-not ([System.Type]::GetType($typeName, $false))) {
    $source = @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace FclSelfTestTool {
    internal static class Native {
        private const uint GENERIC_READ = 0x80000000;
        private const uint GENERIC_WRITE = 0x40000000;
        private const uint FILE_SHARE_READ = 0x00000001;
        private const uint FILE_SHARE_WRITE = 0x00000002;
        private const int OPEN_EXISTING = 3;

        private const uint IOCTL_FCL_PING = 0x22E000;
        private const uint IOCTL_FCL_SELF_TEST = 0x22E004;

        public static PingSummary RunPing(string devicePath) {
            return InvokeIoctl<FCL_PING_RESPONSE, PingSummary>(devicePath, IOCTL_FCL_PING, raw => new PingSummary(raw));
        }

        public static SelfTestSummary RunSelfTest(string devicePath) {
            return InvokeIoctl<FCL_SELF_TEST_RESULT, SelfTestSummary>(devicePath, IOCTL_FCL_SELF_TEST, raw => new SelfTestSummary(raw));
        }

        private static TResult InvokeIoctl<TRaw, TResult>(string devicePath, uint code, Func<TRaw, TResult> projector)
            where TRaw : struct {
            if (string.IsNullOrWhiteSpace(devicePath)) {
                throw new ArgumentException("Device path is required.", nameof(devicePath));
            }

            using var handle = OpenDevice(devicePath);
            int size = Marshal.SizeOf<TRaw>();
            IntPtr buffer = Marshal.AllocHGlobal(size);
            try {
                if (!DeviceIoControl(handle, code, IntPtr.Zero, 0, buffer, size, out _, IntPtr.Zero)) {
                    throw new Win32Exception(Marshal.GetLastWin32Error(), $"DeviceIoControl(0x{code:X}) failed.");
                }
                var raw = Marshal.PtrToStructure<TRaw>(buffer);
                if (raw is null) {
                    throw new InvalidOperationException("DeviceIoControl returned null data.");
                }
                return projector((TRaw)raw);
            } finally {
                Marshal.FreeHGlobal(buffer);
            }
        }

        private static SafeFileHandle OpenDevice(string devicePath) {
            var handle = CreateFile(
                devicePath,
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                IntPtr.Zero,
                OPEN_EXISTING,
                0,
                IntPtr.Zero);
            if (handle.IsInvalid) {
                throw new Win32Exception(Marshal.GetLastWin32Error(), $"CreateFile({devicePath}) failed.");
            }
            return handle;
        }

        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        private static extern SafeFileHandle CreateFile(
            string lpFileName,
            uint dwDesiredAccess,
            uint dwShareMode,
            IntPtr lpSecurityAttributes,
            int dwCreationDisposition,
            int dwFlagsAndAttributes,
            IntPtr hTemplateFile);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool DeviceIoControl(
            SafeFileHandle hDevice,
            uint dwIoControlCode,
            IntPtr lpInBuffer,
            int nInBufferSize,
            IntPtr lpOutBuffer,
            int nOutBufferSize,
            out int lpBytesReturned,
            IntPtr lpOverlapped);
    }

    public sealed class PingSummary {
        public DriverVersion Version { get; }
        public bool IsInitialized { get; }
        public bool IsInitializing { get; }
        public int LastError { get; }
        public TimeSpan Uptime { get; }
        public PoolStats Pool { get; }

        internal PingSummary(FCL_PING_RESPONSE raw) {
            Version = new DriverVersion(raw.Version);
            IsInitialized = raw.IsInitialized != 0;
            IsInitializing = raw.IsInitializing != 0;
            LastError = raw.LastError;
            Uptime = TimeSpan.FromTicks(raw.Uptime100ns);
            Pool = new PoolStats(raw.Pool);
        }
    }

    public sealed class SelfTestSummary {
        public DriverVersion Version { get; }
        public int InitializeStatus { get; }
        public int GeometryCreateStatus { get; }
        public int CollisionStatus { get; }
        public int DestroyStatus { get; }
        public int DistanceStatus { get; }
        public int BroadphaseStatus { get; }
        public int OverallStatus { get; }
        public bool Passed { get; }
        public bool PoolBalanced { get; }
        public bool CollisionDetected { get; }
        public bool BoundaryPassed { get; }
        public int InvalidGeometryStatus { get; }
        public int DestroyInvalidStatus { get; }
        public int CollisionInvalidStatus { get; }
        public ulong PoolBytesDelta { get; }
        public float DistanceValue { get; }
        public uint BroadphasePairCount { get; }
        public PoolStats PoolBefore { get; }
        public PoolStats PoolAfter { get; }
        public ContactSummary Contact { get; }

        internal SelfTestSummary(FCL_SELF_TEST_RESULT raw) {
            Version = new DriverVersion(raw.Version);
            InitializeStatus = raw.InitializeStatus;
            GeometryCreateStatus = raw.GeometryCreateStatus;
            CollisionStatus = raw.CollisionStatus;
            DestroyStatus = raw.DestroyStatus;
            DistanceStatus = raw.DistanceStatus;
            BroadphaseStatus = raw.BroadphaseStatus;
            OverallStatus = raw.OverallStatus;
            Passed = raw.Passed != 0;
            PoolBalanced = raw.PoolBalanced != 0;
            CollisionDetected = raw.CollisionDetected != 0;
            BoundaryPassed = raw.BoundaryPassed != 0;
            InvalidGeometryStatus = raw.InvalidGeometryStatus;
            DestroyInvalidStatus = raw.DestroyInvalidStatus;
            CollisionInvalidStatus = raw.CollisionInvalidStatus;
            PoolBytesDelta = raw.PoolBytesDelta;
            DistanceValue = raw.DistanceValue;
            BroadphasePairCount = raw.BroadphasePairCount;
            PoolBefore = new PoolStats(raw.PoolBefore);
            PoolAfter = new PoolStats(raw.PoolAfter);
            Contact = new ContactSummary(raw.Contact);
        }
    }

    public sealed record DriverVersion(uint Major, uint Minor, uint Patch, uint Build) {
        internal DriverVersion(FCL_DRIVER_VERSION raw) : this(raw.Major, raw.Minor, raw.Patch, raw.Build) { }
        public override string ToString() => $"{Major}.{Minor}.{Patch}.{Build}";
    }

    public sealed record PoolStats(ulong AllocationCount, ulong FreeCount, ulong BytesAllocated, ulong BytesFreed, ulong BytesInUse, ulong PeakBytesInUse) {
        internal PoolStats(FCL_POOL_STATS raw)
            : this(raw.AllocationCount, raw.FreeCount, raw.BytesAllocated, raw.BytesFreed, raw.BytesInUse, raw.PeakBytesInUse) { }
    }

    public sealed record Vector3(float X, float Y, float Z) {
        internal Vector3(FCL_VECTOR3 raw) : this(raw.X, raw.Y, raw.Z) { }
    }

    public sealed record ContactSummary(Vector3 PointOnObject1, Vector3 PointOnObject2, Vector3 Normal, float PenetrationDepth) {
        internal ContactSummary(FCL_CONTACT_SUMMARY raw)
            : this(new Vector3(raw.PointOnObject1), new Vector3(raw.PointOnObject2), new Vector3(raw.Normal), raw.PenetrationDepth) { }
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct FCL_DRIVER_VERSION {
        public uint Major;
        public uint Minor;
        public uint Patch;
        public uint Build;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct FCL_VECTOR3 {
        public float X;
        public float Y;
        public float Z;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct FCL_CONTACT_SUMMARY {
        public FCL_VECTOR3 PointOnObject1;
        public FCL_VECTOR3 PointOnObject2;
        public FCL_VECTOR3 Normal;
        public float PenetrationDepth;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct FCL_POOL_STATS {
        public ulong AllocationCount;
        public ulong FreeCount;
        public ulong BytesAllocated;
        public ulong BytesFreed;
        public ulong BytesInUse;
        public ulong PeakBytesInUse;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct FCL_PING_RESPONSE {
        public FCL_DRIVER_VERSION Version;
        public byte IsInitialized;
        public byte IsInitializing;
        public uint Reserved;
        public int LastError;
        public long Uptime100ns;
        public FCL_POOL_STATS Pool;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct FCL_SELF_TEST_RESULT {
        public FCL_DRIVER_VERSION Version;
        public int InitializeStatus;
        public int GeometryCreateStatus;
        public int CollisionStatus;
        public int DestroyStatus;
        public int DistanceStatus;
        public int BroadphaseStatus;
        public int MeshGjkStatus;
        public int SphereMeshStatus;
        public int MeshBroadphaseStatus;
        public int ContinuousCollisionStatus;
        public int GeometryUpdateStatus;
        public int SphereObbStatus;
        public int MeshComplexStatus;
        public int BoundaryStatus;
        public int DriverVerifierStatus;
        public byte DriverVerifierActive;
        public byte _padding0;
        public ushort _padding1;
        public int LeakTestStatus;
        public int StressStatus;
        public int PerformanceStatus;
        public ulong StressDurationMicroseconds;
        public ulong PerformanceDurationMicroseconds;
        public int OverallStatus;
        public byte Passed;
        public byte PoolBalanced;
        public byte CollisionDetected;
        public byte BoundaryPassed;
        public ushort Reserved;
        public int InvalidGeometryStatus;
        public int DestroyInvalidStatus;
        public int CollisionInvalidStatus;
        public ulong PoolBytesDelta;
        public float DistanceValue;
        public uint BroadphasePairCount;
        public uint MeshBroadphasePairCount;
        public FCL_POOL_STATS PoolBefore;
        public FCL_POOL_STATS PoolAfter;
        public FCL_CONTACT_SUMMARY Contact;
    }
}
'@
    Add-Type -TypeDefinition $source -Language CSharp -ErrorAction Stop | Out-Null
}

function Format-NtStatus {
    param([int]$Status)
    return ('0x{0:X8}' -f ([uint32]$Status))
}

try {
    $report = [ordered]@{}

    if (-not $SkipPing) {
        $ping = [FclSelfTestTool.Native]::RunPing($DevicePath)
        $report.Ping = [pscustomobject]@{
            Version        = $ping.Version.ToString()
            IsInitialized  = $ping.IsInitialized
            IsInitializing = $ping.IsInitializing
            LastError      = (Format-NtStatus -Status $ping.LastError)
            Uptime         = $ping.Uptime.ToString()
            Pool           = $ping.Pool
        }
    }

    $self = [FclSelfTestTool.Native]::RunSelfTest($DevicePath)
    $report.SelfTest = [pscustomobject]@{
        Version             = $self.Version.ToString()
        Passed              = $self.Passed
        OverallStatus       = (Format-NtStatus -Status $self.OverallStatus)
        InitializeStatus    = (Format-NtStatus -Status $self.InitializeStatus)
        GeometryCreateStatus= (Format-NtStatus -Status $self.GeometryCreateStatus)
        CollisionStatus     = (Format-NtStatus -Status $self.CollisionStatus)
        DestroyStatus       = (Format-NtStatus -Status $self.DestroyStatus)
        DistanceStatus      = (Format-NtStatus -Status $self.DistanceStatus)
        BroadphaseStatus    = (Format-NtStatus -Status $self.BroadphaseStatus)
        PoolBalanced        = $self.PoolBalanced
        CollisionDetected   = $self.CollisionDetected
        BoundaryPassed      = $self.BoundaryPassed
        InvalidGeometryStatus = (Format-NtStatus -Status $self.InvalidGeometryStatus)
        DestroyInvalidStatus  = (Format-NtStatus -Status $self.DestroyInvalidStatus)
        CollisionInvalidStatus= (Format-NtStatus -Status $self.CollisionInvalidStatus)
        PoolBytesDelta      = $self.PoolBytesDelta
        DistanceValue       = $self.DistanceValue
        BroadphasePairCount = $self.BroadphasePairCount
        PoolBefore          = $self.PoolBefore
        PoolAfter           = $self.PoolAfter
        Contact             = $self.Contact
    }

    if ($Json) {
        $report | ConvertTo-Json -Depth 6
    } else {
        if ($report.Contains('Ping')) {
            Write-Host '--- IOCTL_FCL_PING ---'
            $report.Ping | Format-List
        }
        Write-Host '--- IOCTL_FCL_SELF_TEST ---'
        $report.SelfTest | Format-List
    }

    if ($self.Passed) {
        exit 0
    } else {
        Write-Error "Self-test failed: OverallStatus $($report.SelfTest.OverallStatus)"
        exit 10
    }
} catch {
    Write-Error $_.Exception.Message
    exit 2
}
