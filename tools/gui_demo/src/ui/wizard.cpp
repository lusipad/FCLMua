#include "wizard.h"
#include <commctrl.h>
#include <sstream>

#pragma comment(lib, "comctl32.lib")

// Dialog IDs
#define IDD_SPHERE_WIZARD 1000
#define IDD_BOX_WIZARD    1001
#define IDD_MESH_WIZARD   1002

#define IDC_NAME          2000
#define IDC_POS_X         2001
#define IDC_POS_Y         2002
#define IDC_POS_Z         2003
#define IDC_RADIUS        2004
#define IDC_SIZE_X        2005
#define IDC_SIZE_Y        2006
#define IDC_SIZE_Z        2007
#define IDOK_CREATE       2100
#define IDCANCEL_WIZARD   2101

GeometryWizard::GeometryWizard(Scene* scene, HINSTANCE hInstance)
    : m_scene(scene)
    , m_hInstance(hInstance)
{
}

GeometryWizard::~GeometryWizard()
{
}

void GeometryWizard::ShowCreateSphereDialog(HWND parent)
{
    // Create a dynamic dialog template
    // For simplicity, we'll use a message box approach
    // In a full implementation, you would create a proper dialog template

    DialogBoxParam(
        m_hInstance,
        MAKEINTRESOURCE(IDD_SPHERE_WIZARD),
        parent,
        SphereDialogProc,
        reinterpret_cast<LPARAM>(this)
    );
}

void GeometryWizard::ShowCreateBoxDialog(HWND parent)
{
    DialogBoxParam(
        m_hInstance,
        MAKEINTRESOURCE(IDD_BOX_WIZARD),
        parent,
        BoxDialogProc,
        reinterpret_cast<LPARAM>(this)
    );
}

void GeometryWizard::ShowCreateMeshDialog(HWND parent)
{
    DialogBoxParam(
        m_hInstance,
        MAKEINTRESOURCE(IDD_MESH_WIZARD),
        parent,
        MeshDialogProc,
        reinterpret_cast<LPARAM>(this)
    );
}

INT_PTR CALLBACK GeometryWizard::SphereDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static GeometryWizard* wizard = nullptr;

    switch (message)
    {
    case WM_INITDIALOG:
    {
        wizard = reinterpret_cast<GeometryWizard*>(lParam);

        // Set default values
        SetDlgItemTextA(hDlg, IDC_NAME, "New Sphere");
        SetDlgItemTextA(hDlg, IDC_POS_X, "0.0");
        SetDlgItemTextA(hDlg, IDC_POS_Y, "2.0");
        SetDlgItemTextA(hDlg, IDC_POS_Z, "0.0");
        SetDlgItemTextA(hDlg, IDC_RADIUS, "1.0");

        return TRUE;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDOK_CREATE)
        {
            if (wizard && wizard->m_scene)
            {
                // Get values from dialog
                char name[256], posX[64], posY[64], posZ[64], radius[64];
                GetDlgItemTextA(hDlg, IDC_NAME, name, sizeof(name));
                GetDlgItemTextA(hDlg, IDC_POS_X, posX, sizeof(posX));
                GetDlgItemTextA(hDlg, IDC_POS_Y, posY, sizeof(posY));
                GetDlgItemTextA(hDlg, IDC_POS_Z, posZ, sizeof(posZ));
                GetDlgItemTextA(hDlg, IDC_RADIUS, radius, sizeof(radius));

                // Parse values
                float x = static_cast<float>(atof(posX));
                float y = static_cast<float>(atof(posY));
                float z = static_cast<float>(atof(posZ));
                float r = static_cast<float>(atof(radius));

                // Create sphere
                wizard->m_scene->AddSphere(name, XMFLOAT3(x, y, z), r);
            }

            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        else if (LOWORD(wParam) == IDCANCEL || LOWORD(wParam) == IDCANCEL_WIZARD)
        {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    }

    return FALSE;
}

INT_PTR CALLBACK GeometryWizard::BoxDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static GeometryWizard* wizard = nullptr;

    switch (message)
    {
    case WM_INITDIALOG:
    {
        wizard = reinterpret_cast<GeometryWizard*>(lParam);

        // Set default values
        SetDlgItemTextA(hDlg, IDC_NAME, "New Box");
        SetDlgItemTextA(hDlg, IDC_POS_X, "0.0");
        SetDlgItemTextA(hDlg, IDC_POS_Y, "2.0");
        SetDlgItemTextA(hDlg, IDC_POS_Z, "0.0");
        SetDlgItemTextA(hDlg, IDC_SIZE_X, "1.0");
        SetDlgItemTextA(hDlg, IDC_SIZE_Y, "1.0");
        SetDlgItemTextA(hDlg, IDC_SIZE_Z, "1.0");

        return TRUE;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDOK_CREATE)
        {
            if (wizard && wizard->m_scene)
            {
                // Get values from dialog
                char name[256], posX[64], posY[64], posZ[64];
                char sizeX[64], sizeY[64], sizeZ[64];

                GetDlgItemTextA(hDlg, IDC_NAME, name, sizeof(name));
                GetDlgItemTextA(hDlg, IDC_POS_X, posX, sizeof(posX));
                GetDlgItemTextA(hDlg, IDC_POS_Y, posY, sizeof(posY));
                GetDlgItemTextA(hDlg, IDC_POS_Z, posZ, sizeof(posZ));
                GetDlgItemTextA(hDlg, IDC_SIZE_X, sizeX, sizeof(sizeX));
                GetDlgItemTextA(hDlg, IDC_SIZE_Y, sizeY, sizeof(sizeY));
                GetDlgItemTextA(hDlg, IDC_SIZE_Z, sizeZ, sizeof(sizeZ));

                // Parse values
                float x = static_cast<float>(atof(posX));
                float y = static_cast<float>(atof(posY));
                float z = static_cast<float>(atof(posZ));
                float sx = static_cast<float>(atof(sizeX));
                float sy = static_cast<float>(atof(sizeY));
                float sz = static_cast<float>(atof(sizeZ));

                // Create box
                wizard->m_scene->AddBox(name, XMFLOAT3(x, y, z), XMFLOAT3(sx, sy, sz));
            }

            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        else if (LOWORD(wParam) == IDCANCEL || LOWORD(wParam) == IDCANCEL_WIZARD)
        {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    }

    return FALSE;
}

INT_PTR CALLBACK GeometryWizard::MeshDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    // TODO: Implement mesh creation dialog (load from file, etc.)
    return FALSE;
}
