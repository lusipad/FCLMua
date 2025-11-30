[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter()]
    [string]$DevicePath = "\\.\FclMusa",
    [Parameter()]
    [double]$Tolerance = 1e-4,
    [Parameter()]
    [switch]$Json
)

Set-StrictMode -Version Latest

$source = @'
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace FclRegression {
    internal static class IoctlCodes {
        internal const uint CreateSphere = 0x0022E010;
        internal const uint DestroyGeometry = 0x0022E014;
        internal const uint QueryCollision = 0x0022E008;
        internal const uint QueryDistance = 0x0022E00C;
        internal const uint ConvexCcd = 0x0022E01C;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct FCL_VECTOR3 {
        public float X;
        public float Y;
        public float Z;

        public static FCL_VECTOR3 Of(float x, float y, float z) {
            return new FCL_VECTOR3 { X = x, Y = y, Z = z };
        }
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct FCL_MATRIX3X3 {
        public float M11;
        public float M12;
        public float M13;
        public float M21;
        public float M22;
        public float M23;
        public float M31;
        public float M32;
        public float M33;

        public static FCL_MATRIX3X3 Identity {
            get {
                return new FCL_MATRIX3X3 {
                    M11 = 1f,
                    M12 = 0f,
                    M13 = 0f,
                    M21 = 0f,
                    M22 = 1f,
                    M23 = 0f,
                    M31 = 0f,
                    M32 = 0f,
                    M33 = 1f
                };
            }
        }
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct FCL_TRANSFORM {
        public FCL_MATRIX3X3 Rotation;
        public FCL_VECTOR3 Translation;
    }

    internal static class FclMath {
        internal static FCL_TRANSFORM IdentityTransform() {
            return new FCL_TRANSFORM {
                Rotation = FCL_MATRIX3X3.Identity,
                Translation = FCL_VECTOR3.Of(0f, 0f, 0f)
            };
        }

        internal static FCL_TRANSFORM Translate(float x, float y, float z) {
            var transform = IdentityTransform();
            transform.Translation = FCL_VECTOR3.Of(x, y, z);
            return transform;
        }
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct FCL_GEOMETRY_HANDLE {
        public ulong Value;

        public bool IsValid => Value != 0;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct FCL_SPHERE_GEOMETRY_DESC {
        public FCL_VECTOR3 Center;
        public float Radius;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct FCL_CREATE_SPHERE_BUFFER {
        public FCL_SPHERE_GEOMETRY_DESC Desc;
        public FCL_GEOMETRY_HANDLE Handle;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct FCL_DESTROY_INPUT {
        public FCL_GEOMETRY_HANDLE Handle;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct FCL_CONTACT_INFO {
        public FCL_VECTOR3 PointOnObject1;
        public FCL_VECTOR3 PointOnObject2;
        public FCL_VECTOR3 Normal;
        public float PenetrationDepth;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct FCL_COLLISION_QUERY {
        public FCL_GEOMETRY_HANDLE Object1;
        public FCL_TRANSFORM Transform1;
        public FCL_GEOMETRY_HANDLE Object2;
        public FCL_TRANSFORM Transform2;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct FCL_COLLISION_RESULT {
        public byte IsColliding;
        public byte Reserved0;
        public byte Reserved1;
        public byte Reserved2;
        public FCL_CONTACT_INFO Contact;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct FCL_COLLISION_IO_BUFFER {
        public FCL_COLLISION_QUERY Query;
        public FCL_COLLISION_RESULT Result;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct FCL_DISTANCE_QUERY {
        public FCL_GEOMETRY_HANDLE Object1;
        public FCL_TRANSFORM Transform1;
        public FCL_GEOMETRY_HANDLE Object2;
        public FCL_TRANSFORM Transform2;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct FCL_DISTANCE_OUTPUT {
        public float Distance;
        public FCL_VECTOR3 ClosestPoint1;
        public FCL_VECTOR3 ClosestPoint2;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct FCL_DISTANCE_IO_BUFFER {
        public FCL_DISTANCE_QUERY Query;
        public FCL_DISTANCE_OUTPUT Result;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct FCL_INTERP_MOTION {
        public FCL_TRANSFORM Start;
        public FCL_TRANSFORM End;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct FCL_CONTINUOUS_COLLISION_RESULT {
        public byte Intersecting;
        public byte Pad0;
        public byte Pad1;
        public byte Pad2;
        public byte Pad3;
        public byte Pad4;
        public byte Pad5;
        public byte Pad6;
        public double TimeOfImpact;
        public FCL_CONTACT_INFO Contact;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    internal struct FCL_CONVEX_CCD_BUFFER {
        public FCL_GEOMETRY_HANDLE Object1;
        public FCL_INTERP_MOTION Motion1;
        public FCL_GEOMETRY_HANDLE Object2;
        public FCL_INTERP_MOTION Motion2;
        public FCL_CONTINUOUS_COLLISION_RESULT Result;
    }

    internal static class NativeMethods {
        internal const uint GENERIC_READ = 0x80000000;
        internal const uint GENERIC_WRITE = 0x40000000;
        internal const uint FILE_SHARE_READ = 0x00000001;
        internal const uint FILE_SHARE_WRITE = 0x00000002;
        internal const uint OPEN_EXISTING = 3;

        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        internal static extern SafeFileHandle CreateFile(
            string lpFileName,
            uint dwDesiredAccess,
            uint dwShareMode,
            IntPtr lpSecurityAttributes,
            uint dwCreationDisposition,
            uint dwFlagsAndAttributes,
            IntPtr hTemplateFile);

        [DllImport("kernel32.dll", SetLastError = true)]
        internal static extern bool DeviceIoControl(
            SafeFileHandle hDevice,
            uint dwIoControlCode,
            IntPtr lpInBuffer,
            int nInBufferSize,
            IntPtr lpOutBuffer,
            int nOutBufferSize,
            out int lpBytesReturned,
            IntPtr lpOverlapped);
    }

    internal sealed class DriverSession : IDisposable {
        private SafeFileHandle handle;

        private DriverSession(SafeFileHandle handle) {
            this.handle = handle;
        }

        internal static DriverSession Open(string devicePath) {
            var handle = NativeMethods.CreateFile(
                devicePath,
                NativeMethods.GENERIC_READ | NativeMethods.GENERIC_WRITE,
                NativeMethods.FILE_SHARE_READ | NativeMethods.FILE_SHARE_WRITE,
                IntPtr.Zero,
                NativeMethods.OPEN_EXISTING,
                0,
                IntPtr.Zero);
            if (handle.IsInvalid) {
                throw new Win32Exception(Marshal.GetLastWin32Error(), $"CreateFile({devicePath}) failed.");
            }
            return new DriverSession(handle);
        }

        public void Dispose() {
            if (handle != null && !handle.IsInvalid) {
                handle.Dispose();
            }
            handle = null;
        }

        internal GeometryLease CreateSphere(float radius, FCL_VECTOR3 center) {
            var payload = new FCL_CREATE_SPHERE_BUFFER {
                Desc = new FCL_SPHERE_GEOMETRY_DESC {
                    Center = center,
                    Radius = radius
                },
                Handle = default(FCL_GEOMETRY_HANDLE)
            };
            InvokeIoctl(ref payload, IoctlCodes.CreateSphere);
            if (!payload.Handle.IsValid) {
                throw new InvalidOperationException("Driver returned invalid sphere handle.");
            }
            return new GeometryLease(this, payload.Handle);
        }

        internal void Destroy(FCL_GEOMETRY_HANDLE handleValue) {
            if (!handleValue.IsValid) {
                return;
            }
            var payload = new FCL_DESTROY_INPUT { Handle = handleValue };
            InvokeIoctl(ref payload, IoctlCodes.DestroyGeometry);
        }

        internal CollisionResult QueryCollision(
            FCL_GEOMETRY_HANDLE object1,
            FCL_TRANSFORM transform1,
            FCL_GEOMETRY_HANDLE object2,
            FCL_TRANSFORM transform2) {
            var payload = new FCL_COLLISION_IO_BUFFER {
                Query = new FCL_COLLISION_QUERY {
                    Object1 = object1,
                    Transform1 = transform1,
                    Object2 = object2,
                    Transform2 = transform2
                }
            };
            InvokeIoctl(ref payload, IoctlCodes.QueryCollision);
            return new CollisionResult(payload.Result);
        }

        internal DistanceResult QueryDistance(
            FCL_GEOMETRY_HANDLE object1,
            FCL_TRANSFORM transform1,
            FCL_GEOMETRY_HANDLE object2,
            FCL_TRANSFORM transform2) {
            var payload = new FCL_DISTANCE_IO_BUFFER {
                Query = new FCL_DISTANCE_QUERY {
                    Object1 = object1,
                    Transform1 = transform1,
                    Object2 = object2,
                    Transform2 = transform2
                }
            };
            InvokeIoctl(ref payload, IoctlCodes.QueryDistance);
            return new DistanceResult(payload.Result);
        }

        internal ContinuousResult RunConvexCcd(
            FCL_GEOMETRY_HANDLE movingHandle,
            FCL_TRANSFORM start,
            FCL_TRANSFORM end,
            FCL_GEOMETRY_HANDLE targetHandle,
            FCL_TRANSFORM targetTransform) {
            var payload = new FCL_CONVEX_CCD_BUFFER {
                Object1 = movingHandle,
                Motion1 = new FCL_INTERP_MOTION { Start = start, End = end },
                Object2 = targetHandle,
                Motion2 = new FCL_INTERP_MOTION { Start = targetTransform, End = targetTransform }
            };
            InvokeIoctl(ref payload, IoctlCodes.ConvexCcd);
            return new ContinuousResult(payload.Result);
        }

        private void InvokeIoctl<TPayload>(ref TPayload payload, uint ioctl)
            where TPayload : struct {
            int size = Marshal.SizeOf<TPayload>();
            IntPtr buffer = Marshal.AllocHGlobal(size);
            try {
                int bytesReturned = 0;
                Marshal.StructureToPtr(payload, buffer, false);
                if (!NativeMethods.DeviceIoControl(handle, ioctl, buffer, size, buffer, size, out bytesReturned, IntPtr.Zero)) {
                    throw new Win32Exception(Marshal.GetLastWin32Error(), $"DeviceIoControl(0x{ioctl:X}) failed.");
                }
                payload = Marshal.PtrToStructure<TPayload>(buffer);
            } finally {
                Marshal.FreeHGlobal(buffer);
            }
        }
    }

    internal sealed class GeometryLease : IDisposable {
        private DriverSession session;
        private FCL_GEOMETRY_HANDLE handle;

        internal GeometryLease(DriverSession session, FCL_GEOMETRY_HANDLE handle) {
            this.session = session;
            this.handle = handle;
        }

        internal FCL_GEOMETRY_HANDLE Handle => handle;

        public void Dispose() {
            if (session != null) {
                session.Destroy(handle);
                handle = default(FCL_GEOMETRY_HANDLE);
                session = null;
            }
        }
    }

    internal readonly struct CollisionResult {
        internal CollisionResult(FCL_COLLISION_RESULT raw) {
            IsColliding = raw.IsColliding != 0;
            Contact = raw.Contact;
        }

        internal bool IsColliding { get; }
        internal FCL_CONTACT_INFO Contact { get; }
    }

    internal readonly struct DistanceResult {
        internal DistanceResult(FCL_DISTANCE_OUTPUT raw) {
            Distance = raw.Distance;
            ClosestPoint1 = raw.ClosestPoint1;
            ClosestPoint2 = raw.ClosestPoint2;
        }

        internal float Distance { get; }
        internal FCL_VECTOR3 ClosestPoint1 { get; }
        internal FCL_VECTOR3 ClosestPoint2 { get; }
    }

    internal readonly struct ContinuousResult {
        internal ContinuousResult(FCL_CONTINUOUS_COLLISION_RESULT raw) {
            Intersecting = raw.Intersecting != 0;
            TimeOfImpact = raw.TimeOfImpact;
        }

        internal bool Intersecting { get; }
        internal double TimeOfImpact { get; }
    }

    public sealed class ScenarioResult {
        public ScenarioResult(string name, bool passed, string expected, string actual, string details) {
            Name = name;
            Passed = passed;
            Expected = expected;
            Actual = actual;
            Details = details;
        }

        public string Name { get; }
        public bool Passed { get; }
        public string Expected { get; }
        public string Actual { get; }
        public string Details { get; }
    }

    internal static class DriverScenarios {
        internal static IReadOnlyList<ScenarioResult> RunAll(DriverSession session, double tolerance) {
            var results = new List<ScenarioResult>();
            results.Add(RunCollisionScenario(session));
            results.Add(RunSeparationScenario(session));
            results.Add(RunDistanceScenario(session, tolerance));
            results.Add(RunContinuousScenario(session, tolerance));
            return results;
        }

        private static ScenarioResult RunCollisionScenario(DriverSession session) {
            using (var sphereA = session.CreateSphere(0.5f, FCL_VECTOR3.Of(0f, 0f, 0f)))
            using (var sphereB = session.CreateSphere(0.5f, FCL_VECTOR3.Of(0f, 0f, 0f))) {
                var tfA = FclMath.IdentityTransform();
                var tfB = FclMath.Translate(0.8f, 0f, 0f);
                var result = session.QueryCollision(sphereA.Handle, tfA, sphereB.Handle, tfB);
                var expected = "collision=true";
                var actual = $"collision={(result.IsColliding ? "true" : "false")}";
                var details = result.IsColliding ? $"penetration={result.Contact.PenetrationDepth:F6}" : "collision expected";
                return new ScenarioResult("collision-spheres", result.IsColliding, expected, actual, details);
            }
        }

        private static ScenarioResult RunSeparationScenario(DriverSession session) {
            using (var sphereA = session.CreateSphere(0.5f, FCL_VECTOR3.Of(0f, 0f, 0f)))
            using (var sphereB = session.CreateSphere(0.5f, FCL_VECTOR3.Of(0f, 0f, 0f))) {
                var tfA = FclMath.IdentityTransform();
                var tfB = FclMath.Translate(2.5f, 0f, 0f);
                var result = session.QueryCollision(sphereA.Handle, tfA, sphereB.Handle, tfB);
                var expected = "collision=false";
                var actual = $"collision={(result.IsColliding ? "true" : "false")}";
                var details = result.IsColliding ? "separation expected" : "separation confirmed";
                return new ScenarioResult("separation-spheres", !result.IsColliding, expected, actual, details);
            }
        }

        private static ScenarioResult RunDistanceScenario(DriverSession session, double tolerance) {
            using (var sphereA = session.CreateSphere(0.5f, FCL_VECTOR3.Of(0f, 0f, 0f)))
            using (var sphereB = session.CreateSphere(0.5f, FCL_VECTOR3.Of(0f, 0f, 0f))) {
                var tfA = FclMath.IdentityTransform();
                var tfB = FclMath.Translate(2.0f, 0f, 0f);
                var result = session.QueryDistance(sphereA.Handle, tfA, sphereB.Handle, tfB);
                const double expectedDistance = 1.0;
                var distanceDelta = Math.Abs(result.Distance - expectedDistance);
                var passed = distanceDelta <= tolerance;
                var expected = $"distance={expectedDistance:F6}";
                var actual = $"distance={result.Distance:F6}";
                var details = $"delta={distanceDelta:F6}";
                return new ScenarioResult("distance-spheres", passed, expected, actual, details);
            }
        }

        private static ScenarioResult RunContinuousScenario(DriverSession session, double tolerance) {
            using (var moving = session.CreateSphere(0.5f, FCL_VECTOR3.Of(0f, 0f, 0f)))
            using (var target = session.CreateSphere(0.5f, FCL_VECTOR3.Of(0f, 0f, 0f))) {
                var start = FclMath.IdentityTransform();
                var end = FclMath.Translate(4f, 0f, 0f);
                var staticTf = FclMath.Translate(3f, 0f, 0f);
                var result = session.RunConvexCcd(moving.Handle, start, end, target.Handle, staticTf);
                const double expectedToi = 0.5;
                var toiDelta = Math.Abs(result.TimeOfImpact - expectedToi);
                var passed = result.Intersecting && toiDelta <= tolerance;
                var expected = $"ccd=true,toi={expectedToi:F6}";
                var actual = $"ccd={(result.Intersecting ? "true" : "false")},toi={result.TimeOfImpact:F6}";
                var details = $"delta={toiDelta:F6}";
                return new ScenarioResult("ccd-spheres", passed, expected, actual, details);
            }
        }
    }
}
'@

Add-Type -TypeDefinition $source -Language CSharp

$session = $null
$exitCode = 0
try {
    $session = [FclRegression.DriverSession]::Open($DevicePath)
    $results = [FclRegression.DriverScenarios]::RunAll($session, $Tolerance)
    if ($Json) {
        $results | ConvertTo-Json -Depth 4
    } else {
        foreach ($item in $results) {
            $tag = if ($item.Passed) { 'PASS' } else { 'FAIL' }
            Write-Host ("[{0}] {1}" -f $tag, $item.Name)
            Write-Host ("  Expected: {0}" -f $item.Expected)
            Write-Host ("  Actual  : {0}" -f $item.Actual)
            if ($item.Details) {
                Write-Host ("  Details : {0}" -f $item.Details)
            }
            Write-Host ""
        }
    }
    $failed = $results | Where-Object { -not $_.Passed }
    if ($failed) {
        $exitCode = 200
    } else {
        $exitCode = 0
    }
} finally {
    if ($session -ne $null) {
        $session.Dispose()
    }
}
exit $exitCode
