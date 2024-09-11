#include "WindowManager/WindowManager.hpp"
#include "MusicPlayer_t/MusicPlayer.hpp"

#include <vector>
#include <string>
#include <iostream>

#include <time.h>
#include <filesystem>
bool DrawMusicPicker(std::filesystem::directory_entry* PickedPath) {
	
	std::filesystem::directory_entry SelectedTrack;
	bool HasPicked = false;
	for (const auto& Track : MusicPlayer.MusicTracks) {
		const std::string& Name = Track.path().filename().string();
		const std::string& Path = Track.path().string();
		bool IsSelected = strcmp(Path.c_str(), PickedPath->path().string().c_str()) == 0;

		if (ImGui::Selectable(Name.c_str(), IsSelected)) {
			SelectedTrack = Track;
			HasPicked = true;
		}
	}

	if (HasPicked) {
		*PickedPath = SelectedTrack;
		return HasPicked;
	}

	return HasPicked;
}

void SetStyle()	 {
	ImGuiStyle* Style = &ImGui::GetStyle();

	Style->WindowRounding = 20.0f;
	Style->WindowMinSize = ImVec2(0.0f, 0.0f);
	
	Style->Colors[ImGuiCol_WindowBg] = ImColor(1.0f, 1.0f, 1.0f, 0.03f);
	Style->Colors[ImGuiCol_ResizeGrip] = ImColor(0.0f, 0.0f, 0.0f, 0.0f);
	Style->Colors[ImGuiCol_ResizeGripActive] = ImColor(0.0f, 0.0f, 0.0f, 0.0f);
	Style->Colors[ImGuiCol_ResizeGripHovered] = ImColor(0.0f, 0.0f, 0.0f, 0.0f);

	Style->Colors[ImGuiCol_Separator] = ImColor(1.0f, 1.0f, 1.0f, 0.2f);
	Style->Colors[ImGuiCol_SeparatorHovered] = ImColor(1.0f, 1.0f, 1.0f, 0.35f);
	Style->Colors[ImGuiCol_SeparatorActive] = ImColor(1.0f, 1.0f, 1.0f, 0.5f);

	Style->Colors[ImGuiCol_Header] = ImColor(1.0f, 1.0f, 1.0f, 0.2f);
	Style->Colors[ImGuiCol_HeaderActive] = ImColor(1.0f, 1.0f, 1.0f, 0.35f);
	Style->Colors[ImGuiCol_HeaderHovered] = ImColor(1.0f, 1.0f, 1.0f, 0.5f);

	Style->ScrollbarSize = 2.0f;
}

//int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
int main() {
	WindowManager.Init("C++ MusicPlayer", ImVec2(508, 508));
	SetStyle();
	
	while (WindowManager.IsRunning) {
		WindowManager.Begin();
	
		// Update backend
		MusicPlayer.Update();
		
		const ImGuiStyle* Style = &ImGui::GetStyle();
		ImGui::SetNextWindowSizeConstraints(ImVec2(250, 185), ImVec2(500, 500));
		
		ImGui::SetNextWindowPos(ImVec2(2, 2));
		
		ImGui::SetNextWindowSize(ImVec2(500, 500), ImGuiCond_Once);
		ImGui::Begin("MusicPlayer", nullptr, ImGuiWindowFlags_NoTitleBar);
		{
			// Resize the windows window
			static ImVec2 OldSize = ImVec2(0.0f, 0.0f);
			ImVec2 CurrentSize = ImGui::GetWindowSize();
			if (CurrentSize.x != OldSize.x || CurrentSize.y != OldSize.y) {
				OldSize = CurrentSize;
		
				RECT WindowRect;
				GetWindowRect(WindowManager.WindowHandle, &WindowRect);
		
				SetWindowPos(WindowManager.WindowHandle, NULL, WindowRect.left, WindowRect.top, static_cast<int>(CurrentSize.x + 10), static_cast<int>(CurrentSize.y + 10), 0);
			}
		
			ImGui::BeginChild("WindowFrame", ImVec2(0.0f, 20.0f));
			{
				ImVec2 WindowPos = ImGui::GetWindowPos();
				float HeightCenter = WindowPos.y + ImGui::GetWindowHeight() / 2.0f;
		
				ImDrawList* DrawList = ImGui::GetWindowDrawList();
				ImVec2 MousePos = ImGui::GetMousePos();
		
				// Close button
				{
					ImVec2 CenterPos = { WindowPos.x + 10.0f, HeightCenter };
					float Distance = static_cast<float>(sqrt(pow(CenterPos.x - MousePos.x, 2) + pow(CenterPos.y - MousePos.y, 2)));
					if (Distance < 8.0f) {
						DrawList->AddCircleFilled(CenterPos, 8.0f, ImColor(0.5f, 0.0f, 0.0f));
						if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
							WindowManager.IsRunning = false;
						}
					} else {
						DrawList->AddCircleFilled(CenterPos, 8.0f, ImColor(1.0f, 0.0f, 0.0f));
					}
				}
		
				// Minimize
				{
					ImVec2 CenterPos = { WindowPos.x + 35.0f, HeightCenter };
					float Distance = static_cast<float>(sqrt(pow(CenterPos.x - MousePos.x, 2) + pow(CenterPos.y - MousePos.y, 2)));
					if (Distance < 8.0f) {
						DrawList->AddCircleFilled(CenterPos, 8.0f, ImColor(0.5f, 0.25f, 0.0f));
						if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
							ShowWindow(WindowManager.WindowHandle, SW_MINIMIZE);
						}
					} else {
						DrawList->AddCircleFilled(CenterPos, 8.0f, ImColor(1.0f, 0.5f, 0.0f));
					}
				}
		
				// Fullscreen
				DrawList->AddCircleFilled(ImVec2(WindowPos.x + 60.0f, HeightCenter), 8.0f, ImColor(0.3f, 0.3f, 0.3f));
		
			}
			ImGui::EndChild();
		
			float Size = std::clamp(CurrentSize.y - 190.0f, 0.1f, 1000.0f);
			if (Size > 19.0f) {
				ImGui::PushFont(WindowManager.Fonts["SanFranciscoMedium"]);
				ImGui::BeginChild("TrackPicker", ImVec2(0.0f, Size));
				{
					const ImVec2 Start = ImGui::GetWindowPos();
					const ImVec2 End = { Start.x + ImGui::GetWindowWidth(), Start.y + ImGui::GetWindowHeight() };
					
					std::filesystem::directory_entry CurrentPlayingTrack = MusicPlayer.CurrentTrack->Path;
					if (DrawMusicPicker(&CurrentPlayingTrack)) {
						
						if (MusicPlayer.CurrentTrack) {
							MusicPlayer.CurrentTrack->Pause();
							MusicPlayer.CurrentTrack->Free();
							delete MusicPlayer.CurrentTrack;
						}
					
						if (MusicPlayer.OldTrack) {
							MusicPlayer.OldTrack->Pause();
							MusicPlayer.OldTrack->Free();
							delete MusicPlayer.OldTrack;
						}
					
						const std::string& TrackName = CurrentPlayingTrack.path().filename().string();
					
						MusicPlayer.CurrentTrack = new MusicPlayer_t::Track_t;
						MusicPlayer.CurrentTrack->Init(CurrentPlayingTrack, 0.0f);
						MusicPlayer.CurrentTrack->FadeIn(MusicPlayer.TrackFade, MusicPlayer.Volume);
						MusicPlayer.CurrentTrack->Play();
					}
				}
				ImGui::EndChild();
				ImGui::PopFont();
			}
			
			ImGui::BeginChild("TrackInfo", ImVec2(CurrentSize.x - 65.0f - Style->ItemSpacing.x * 2 - Style->WindowPadding.x, 60.0f));
			{
				ImGui::PushFont(WindowManager.Fonts["SanFranciscoBold"]);
				
				const ImVec2 Start = ImGui::GetWindowPos();
				const ImVec2 End = { Start.x + ImGui::GetWindowWidth(), Start.y + ImGui::GetWindowHeight()};
				
				std::string TrackName = MusicPlayer.CurrentTrack->Path.path().filename().string();
				TrackName = TrackName.substr(0, TrackName.size() - 4);
				const ImVec2 TextSize = ImGui::CalcTextSize(TrackName.c_str());
				
				// If the textsize is too big, scroll
				static float ScrollPx = 0;
				if (TextSize.x > ImGui::GetWindowWidth()) {
					static bool DidReset = true;
					static bool CanMove = false;
					static time_t NonMoveTime = time(nullptr);
			
					if (DidReset) {
						// Make the text stay for a second
						if (ScrollPx > -1.0f && ScrollPx < 1.0f) {
							if (CanMove) {
								CanMove = false;
								NonMoveTime = time(nullptr);
							}
			
							if ((time(nullptr) - NonMoveTime) > 1) {
								CanMove = true;
								DidReset = false;
							}
						}
					}
			
					if (CanMove)
						ScrollPx += ImGui::GetIO().DeltaTime * 64.0f;
			
					if (ScrollPx >= TextSize.x) {
						ScrollPx = -ImGui::GetWindowWidth() - 10.0f;
						DidReset = true;
					}
			
					ImGui::GetWindowDrawList()->AddText(ImVec2(Start.x - ScrollPx, Start.y + ImGui::GetWindowHeight() / 2.0f - TextSize.y / 2.0f), ImColor(1.0f, 1.0f, 1.0f), TrackName.c_str());
			
				} else {
					ScrollPx = 0.0f;
					ImGui::GetWindowDrawList()->AddText(ImVec2(Start.x + (ImGui::GetWindowWidth() + 75.0f) / 2.0f - TextSize.x / 2.0f, Start.y + ImGui::GetWindowHeight() / 2.0f - TextSize.y / 2.0f), ImColor(1.0f, 1.0f, 1.0f), TrackName.c_str());
				}
		
				ImGui::PopFont();
			}
			ImGui::EndChild();
		
			ImGui::SameLine();
		
			ImGui::BeginChild("Visualizer", ImVec2(65.0f, 60.0f));
			{
				MusicPlayer.DrawFreqResponse();
			}
			ImGui::EndChild();
		
			ImGui::BeginChild("Duration", ImVec2(0.0f, 27.5f));
			{
				ImGui::PushFont(WindowManager.Fonts["SanFranciscoMedium"]);
				MusicPlayer.DrawDuration();
				ImGui::PopFont();
			}
			ImGui::EndChild();
			
			ImGui::BeginChild("Spacing", ImVec2(std::clamp(CurrentSize.x / 2.0f - 100.0f, 30.0f, 1000.0f), 50.0f));
			ImGui::EndChild();
			
			ImGui::SameLine();
		
			ImGui::BeginChild("Beforebutton", ImVec2(50.0f, 50.0f));
			{
				MusicPlayer.DrawPrevButton();
			}
			ImGui::EndChild();
		
			ImGui::SameLine();
			
			ImGui::BeginChild("PlayButton", ImVec2(50.0f, 50.0f));
			{
				MusicPlayer.DrawPlayButton();
			}
			ImGui::EndChild();
			ImGui::SameLine();
			ImGui::BeginChild("NextButton", ImVec2(50.0f, 50.0f));
			{
				MusicPlayer.DrawNextButton();
			}
			ImGui::EndChild();
		}
		ImGui::End();
	
		WindowManager.End();
	}

	return 0;
}
