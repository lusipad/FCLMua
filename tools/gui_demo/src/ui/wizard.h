#pragma once
#include <windows.h>
#include <string>
#include <directxmath.h>
#include "../scene.h"

using namespace DirectX;

// Wizard dialog for creating geometry
class GeometryWizard
{
public:
    GeometryWizard(Scene* scene, HINSTANCE hInstance);
    ~GeometryWizard();

    // Show wizard dialog
    void ShowCreateSphereDialog(HWND parent);
    void ShowCreateBoxDialog(HWND parent);
    void ShowCreateMeshDialog(HWND parent);

private:
    static INT_PTR CALLBACK SphereDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK BoxDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK MeshDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

    Scene* m_scene;
    HINSTANCE m_hInstance;
};
