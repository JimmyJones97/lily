#pragma once
#include <d3d9.h>
#pragma comment(lib, "d3d9.lib")

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"

class Hack;

class Render {
public:
	IDirect3DDevice9Ex* pDirect3DDevice9Ex;
	Render(IDirect3DDevice9Ex* pDirect3DDevice9Ex) : pDirect3DDevice9Ex(pDirect3DDevice9Ex) {}

	void ImGuiRenderDrawData() {
		pDirect3DDevice9Ex->SetRenderState(D3DRS_ZENABLE, 0);
		pDirect3DDevice9Ex->SetRenderState(D3DRS_ALPHABLENDENABLE, 0);
		pDirect3DDevice9Ex->SetRenderState(D3DRS_SCISSORTESTENABLE, 0);
		pDirect3DDevice9Ex->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0f, 0);
		if (pDirect3DDevice9Ex->BeginScene() >= 0) {
			ImGui::Render();
			ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
			pDirect3DDevice9Ex->EndScene();
		}
	}

	virtual void Present(const Hack& hack) = 0;
};