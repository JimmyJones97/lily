#include "render.h"

void AddText(const ImFont* font, float font_size, const ImVec2& pos, ImColor Color, const char* szText, float wrap_width = 0.0f, const ImVec4* cpu_fine_clip_rect = 0) {
	ImGui::GetWindowDrawList()->AddText(font, font_size, pos, Color, szText, 0, wrap_width, cpu_fine_clip_rect);
}

void AddTextOutlined(const ImFont* font, float font_size, const ImVec2& pos, ImColor Color, const char* szText, float wrap_width, const ImVec4* cpu_fine_clip_rect) {
	const ImColor ColorOutLine = Color == IM_COL32_BLACK ? IM_COL32_WHITE : IM_COL32_BLACK;
	AddText(font, font_size, { pos.x + 1, pos.y + 1 }, ColorOutLine, szText, wrap_width, cpu_fine_clip_rect);
	AddText(font, font_size, { pos.x - 1, pos.y - 1 }, ColorOutLine, szText, wrap_width, cpu_fine_clip_rect);
	AddText(font, font_size, { pos.x + 1, pos.y - 1 }, ColorOutLine, szText, wrap_width, cpu_fine_clip_rect);
	AddText(font, font_size, { pos.x - 1, pos.y + 1 }, ColorOutLine, szText, wrap_width, cpu_fine_clip_rect);
	AddText(font, font_size, pos, Color, szText, wrap_width, cpu_fine_clip_rect);
}

ImVec2 Render::GetTextSize(float FontSize, const char* szText) {
	const ImFont* pFont = ImGui::GetIO().Fonts->Fonts[0];
	return pFont->CalcTextSizeA(FontSize, FLT_MAX, 0.0f, szText, 0, 0);
}

void Render::DrawString(const FVector& Pos, float Margin, const char* szText, float Size, ImColor Color, bool bCenterPos, bool bCenterAligned, bool bShowAlways) const {
	if (!bRender) return;

	if (Pos.Z < 0.0f && !bShowAlways)
		return;

	ImVec2 LeftTopPos = { Pos.X, Pos.Y };

	const ImFont* pFont = ImGui::GetIO().Fonts->Fonts[0];
	ImVec2 TextSize = pFont->CalcTextSizeA(Size, FLT_MAX, 0.0f, szText, 0, 0);

	float Width = ImGui::GetMainViewport()->WorkSize.x;
	float Height = ImGui::GetMainViewport()->WorkSize.y;

	if (bCenterPos)
		LeftTopPos = { LeftTopPos.x - (TextSize.x / 2.0f), LeftTopPos.y - (TextSize.y / 2.0f) };

	if (Pos.Z < 0.0f)
		LeftTopPos.y = FLT_MAX;

	if (bShowAlways) {
		const float MinPosX = Margin;
		const float MaxPosX = Width - Margin - TextSize.x;

		const float MinPosY = Margin;
		const float MaxPosY = Height - Margin - TextSize.y;

		LeftTopPos.x = std::max(LeftTopPos.x, MinPosX);
		LeftTopPos.x = std::min(LeftTopPos.x, MaxPosX);

		LeftTopPos.y = std::max(LeftTopPos.y, MinPosY);
		LeftTopPos.y = std::min(LeftTopPos.y, MaxPosY);
	}

	if (bCenterAligned) {
		float CurrentPosY = LeftTopPos.y;
		size_t pos = 0;
		while (1) {
			std::string text = szText;
			size_t cpos = text.find('\n', pos);
			std::string strLine = text.substr(pos, cpos - pos);

			//print line
			ImVec2 LineTextSize = pFont->CalcTextSizeA(Size, FLT_MAX, 0.0f, strLine.c_str(), 0, 0);
			ImVec2 CurrentPos = { LeftTopPos.x + (TextSize.x / 2.0f) - (LineTextSize.x / 2.0f), CurrentPosY };
			AddTextOutlined(pFont, Size, CurrentPos, Color, strLine.c_str(), 0.0f, 0);

			if (cpos == std::string::npos)
				break;

			CurrentPosY += LineTextSize.y;
			pos = cpos + 1;
		}
	}
	else {
		AddTextOutlined(pFont, Size, LeftTopPos, Color, szText, 0.0f, 0);
	}
}

void Render::DrawRectOutlined(const FVector& from, const FVector& to, ImColor Color, float rounding, ImDrawFlags flags, float thickness) const {
	if (!bRender) return;

	if (from.Z < 0.0f) return;
	ImGui::GetWindowDrawList()->AddRect({ from.X, from.Y }, { to.X, to.Y }, Color, rounding, flags, thickness);
}

void Render::DrawRectFilled(const FVector& from, const FVector& to, ImColor Color, float rounding, ImDrawFlags flags) const {
	if (!bRender) return;

	if (from.Z < 0.0f) return;
	ImGui::GetWindowDrawList()->AddRectFilled({ from.X, from.Y }, { to.X, to.Y }, Color, rounding, flags);
}

void Render::DrawRatioBox(const FVector& from, const FVector& to, float Ratio, ImColor ColorRemain, ImColor ColorDamaged, ImColor ColorEdge) const {
	if (!bRender) return;

	if (from.Z < 0.0f) return;

	float RemainLen = (to.X - from.X) * Ratio;
	if (Ratio < 1.0f && Ratio > 0.0f)
		RemainLen = ceilf(RemainLen);

	const FVector RemainFrom = { from.X, from.Y, 0.0f };
	const FVector RemainTo = { from.X + RemainLen, to.Y, 0.0f };
	const FVector DamagedFrom = { from.X + RemainLen, from.Y, 0.0f };
	const FVector DamagedTo = { to.X, to.Y, 0.0f };

	DrawRectFilled(DamagedFrom, DamagedTo, ColorDamaged);
	DrawRectFilled(RemainFrom, RemainTo, ColorRemain);
	DrawRectOutlined(from, to, ColorEdge);
}

void Render::DrawLine(const FVector& from, const FVector& to, ImColor Color, float thickness) const {
	if (!bRender) return;

	if (from.Z < 0.0f) return;
	ImGui::GetWindowDrawList()->AddLine({ from.X, from.Y }, { to.X, to.Y }, Color, thickness);
}

void Render::DrawCircle(const ImVec2& center, float radius, ImColor Color, int num_segments, float thickness) const {
	if (!bRender) return;

	ImGui::GetWindowDrawList()->AddCircle(center, radius, Color, num_segments, thickness);
}