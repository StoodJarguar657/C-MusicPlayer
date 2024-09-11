#include "MusicPlayer.hpp"
#include "../ImGui/imgui.h"
#include <algorithm>
#include <chrono>
#include <iostream>

MusicPlayer_t MusicPlayer;

bool MusicPlayer_t::Track_t::Init(const std::filesystem::directory_entry& Path, float Volume) {
	this->Path = Path;
	if (!this->Path.exists())
		return false;

	this->TargetVolume = Volume;
	this->InternalVolume = Volume;
	this->IsFading = false;
	this->Stream = NULL;
	return true;
}
bool MusicPlayer_t::Track_t::Play() {
	if (!this->Stream) {
		const std::string& Path = this->Path.path().string();
		this->Stream = BASS_StreamCreateFile(FALSE, Path.c_str(), 0, 0, 0);
		if (!this->Stream) {
			printf("Failed to create stream from file '%s'\n", Path.c_str());
			return false;
		}

		if (!BASS_ChannelPlay(this->Stream, FALSE)) {
			BASS_StreamFree(this->Stream);
			printf("Failed to play stream\n");
			return false;
		}

		if (!BASS_ChannelSetAttribute(this->Stream, BASS_ATTRIB_VOL, static_cast<float>(this->InternalVolume) / 100.0f)) {
			BASS_StreamFree(this->Stream);
			printf("Failed to set channel attributes\n");
			return false;
		}
	} else {
		int Activity = this->GetActivity();
		if (Activity == 3) {
			if (!BASS_ChannelPlay(this->Stream, FALSE)) {
				BASS_StreamFree(this->Stream);
				printf("Failed to play stream\n");
				return false;
			}
		}
	}
	return true;
}
bool MusicPlayer_t::Track_t::Pause() {
	if (!BASS_ChannelPause(this->Stream)) {
		printf("Failed to pause track\n");
		return false;
	}
	return true;
}

bool MusicPlayer_t::Track_t::SetVolume(float Volume) {
	this->InternalVolume = Volume;
	if (!BASS_ChannelSetAttribute(this->Stream, BASS_ATTRIB_VOL, static_cast<float>(this->InternalVolume) / 100.0f)) {
		printf("Failed to set volume\n");
		return false;
	}
	return true;
}

bool MusicPlayer_t::Track_t::Free() {
	if (!BASS_StreamFree(this->Stream)) {
		printf("Failed to free stream\n");
		return false;
	}
	return true;
}

void MusicPlayer_t::Track_t::FadeIn(float Seconds, float Volume) {
	if (this->IsFading)
		return;
	
	this->FadeLength = Seconds;
	this->TargetVolume = Volume;
	this->StartFade = std::chrono::high_resolution_clock::now();
	this->IsFading = true;
	this->OldVolume = this->InternalVolume;
}
void MusicPlayer_t::Track_t::FadeOut(float Seconds) {
	if (this->IsFading)
		return;

	this->FadeLength = Seconds;
	this->TargetVolume = 0.0f;
	this->StartFade = std::chrono::high_resolution_clock::now();
	this->IsFading = true;
	this->OldVolume = this->InternalVolume;
}

void MusicPlayer_t::Track_t::Copy(Track_t* Track) {
	this->InternalVolume = Track->InternalVolume;
	this->IsFading = Track->IsFading;
	this->Stream = Track->Stream;
	this->Path = Track->Path;
}

int MusicPlayer_t::Track_t::GetActivity() {
	return BASS_ChannelIsActive(this->Stream);
}

double MusicPlayer_t::Track_t::GetDuration() {
	QWORD BytesLength = BASS_ChannelGetLength(this->Stream, BASS_POS_BYTE);
	return BASS_ChannelBytes2Seconds(this->Stream, BytesLength);
}
double MusicPlayer_t::Track_t::GetCurrentPosition() {
	QWORD BytesPos = BASS_ChannelGetPosition(this->Stream, BASS_POS_BYTE);
	return BASS_ChannelBytes2Seconds(this->Stream, BytesPos);
}


void MusicPlayer_t::Track_t::Update() {
	
	if (this->IsFading) {
		float Ratio = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - this->StartFade).count());
		Ratio /= 1000.0f;
		Ratio /= static_cast<float>(this->FadeLength);
		Ratio = std::clamp(Ratio, 0.0f, 1.0f);
		
		this->InternalVolume = this->TargetVolume * Ratio + this->OldVolume * (1.0f - Ratio);

		if (!BASS_ChannelSetAttribute(this->Stream, BASS_ATTRIB_VOL, static_cast<float>(this->InternalVolume) / 100.0f)) {
			printf("Failed to set attribute\n");
		}
		
		if (Ratio == 1.0f)
			this->IsFading = false;
	}
}

std::vector<float> MusicPlayer_t::Track_t::GetFFT(float DeltaTime) {

	// Predefined ranges and multipliers
	const std::vector<std::vector<int>> VisualData = {
		{ 0, 8, 31 },
		{ 8, 10, 31 },
		{ 16, 30, 25 },
		{ 75, 300, 18 },
		{ 300, 450, 15 },
		{ 500, 600, 12 },
		{ 650, 1024, 8 },
	};

	std::vector<float> Out;
	Out.resize(VisualData.size());

	// Make sure to only query, when music is running
	bool IsRunning = BASS_ChannelIsActive(this->Stream) == 1;
	if (IsRunning) {
		// Get music data
		std::vector<float> MusicData;
		MusicData.resize(2048);
		BASS_ChannelGetData(this->Stream, MusicData.data(), BASS_DATA_FFT2048);

		// Compress it into 7 bars
		int Index = 0;
		for (const std::vector<int>& Range : VisualData) {
			float Average = 0.0f;
			for (int i = Range[0]; i < Range[1]; i++)
				Average += MusicData[i];
			Out[Index] = sqrt(Average) * (static_cast<float>(Range[2]) / 100.0f);
			Index++;
		}
	} else {
		// Set music data to 0.0
		for (size_t i = 0; i < VisualData.size(); i++) {
			Out[i] = 0.0f;
		}
	}

	// Preinit FFT
	if (this->FFT.empty()) {
		this->FFT.resize(VisualData.size());
	}

	// Smoothing of output
	for (size_t i = 0; i < this->FFT.size(); i++) {
		this->FFT[i] -= (this->FFT[i] - Out[i]) * DeltaTime;
	}

	return this->FFT;
}


std::filesystem::directory_entry MusicPlayer_t::GetNextTrack() {
	std::filesystem::directory_entry NextTrack("NULL");
	const std::string& CurrentTrackPath = this->CurrentTrack->Path.path().string();
	for (const std::filesystem::directory_entry& MusicTrack : this->MusicTracks) {

		if (NextTrack == std::filesystem::directory_entry("PICK")) {
			NextTrack = MusicTrack;
			break;
		}

		const std::string& TrackPath = MusicTrack.path().string();
		if (strcmp(TrackPath.c_str(), CurrentTrackPath.c_str()) != 0)
			continue;

		if (MusicTrack == this->MusicTracks.back()) {
			NextTrack = this->MusicTracks.front();
			break;
		}

		NextTrack = std::filesystem::directory_entry("PICK");
	}
	return NextTrack;
}
std::filesystem::directory_entry MusicPlayer_t::GetPrevTrack() {
	std::filesystem::directory_entry NextTrack("NULL");
	const std::string& CurrentTrackPath = this->CurrentTrack->Path.path().string();
	for (const std::filesystem::directory_entry& MusicTrack : this->MusicTracks) {
		const std::string& TrackPath = MusicTrack.path().string();
		if (strcmp(TrackPath.c_str(), CurrentTrackPath.c_str()) != 0) {
			NextTrack = MusicTrack;
			continue;
		}
		break;
	}
	return NextTrack;
}

MusicPlayer_t::MusicPlayer_t() {
	if (!BASS_Init(-1, 44100, 0, 0, NULL)) {
		printf("Failed to initialize BASS\n");
		return;
	}

	{
		char UsernameBuf[MAX_PATH];
		DWORD UsernameLen = MAX_PATH + 1;
		GetUserNameA(UsernameBuf, &UsernameLen);

		std::string FolderPath = "C:\\Users\\" + std::string(UsernameBuf, UsernameLen - 1) + "\\Music\\";
		this->MusicFolder = std::filesystem::directory_entry(FolderPath);
	}

	{
		if (!this->MusicFolder.exists())
			return;

		for (const auto& Entry : std::filesystem::directory_iterator(this->MusicFolder)) {
			if (Entry.path().extension() != ".mp3")
				continue;

			this->MusicTracks.push_back(Entry);

			if (this->CurrentTrack != nullptr)
				continue;

			this->CurrentTrack = new Track_t;
			this->CurrentTrack->Init(Entry, this->Volume);
		}
	}
	
}

void MusicPlayer_t::Update() {
	
	static time_t LastQuery = time(nullptr);
	if ((time(nullptr) - LastQuery) > 0) {
		LastQuery = time(nullptr);
		MusicTracks.clear();
		for (const auto& Entry : std::filesystem::directory_iterator(this->MusicFolder)) {
			if (Entry.path().extension() != ".mp3")
				continue;

			MusicTracks.push_back(Entry);
		}
	}

	double CurrentDur = 0.0;
	if (this->CurrentTrack) {
		this->CurrentTrack->Update();
		CurrentDur = this->CurrentTrack->GetDuration() - this->CurrentTrack->GetCurrentPosition();
	}

	double OldDur = 0.0;
	if (this->OldTrack) {
		this->OldTrack->Update();
		OldDur = this->OldTrack->GetDuration() - this->OldTrack->GetCurrentPosition();
	}
	
	if (this->OldTrack && OldDur == 0.0 && this->OldTrack->IsFading) {
		this->OldTrack->Free();
		delete this->OldTrack;
		this->OldTrack = nullptr;
		return;
	}
	
	if (CurrentDur > this->TrackFade || CurrentDur == 0.0)
		return;
	
	if (this->OldTrack != nullptr)
		return;

	std::filesystem::directory_entry NextTrack = this->GetNextTrack();
	
	this->OldTrack = this->CurrentTrack;
	this->OldTrack->FadeOut(this->TrackFade);

	this->CurrentTrack = new Track_t;
	this->CurrentTrack->Init(NextTrack, 0.0f);
	this->CurrentTrack->FadeIn(this->TrackFade, this->Volume);
	this->CurrentTrack->Play();
}

void MusicPlayer_t::DrawDuration() {

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImVec2 Min = ImGui::GetWindowPos();
	const ImVec2 Max = { Min.x + ImGui::GetWindowWidth(), Min.y + ImGui::GetWindowHeight() };
	
	
	float Width = Max.x - Min.x;
	float Height = Max.y - Min.y;
	
	double CurrentPos = this->CurrentTrack->GetCurrentPosition();
	double MaxDuration = this->CurrentTrack->GetDuration();
	
	int CurHours = (static_cast<int>(CurrentPos) / 3600);
	int CurMinutes = ((static_cast<int>(CurrentPos) % 3600) / 60);
	int CurSeconds = (static_cast<int>(CurrentPos) % 60);
	
	int MaxHours = (static_cast<int>(MaxDuration) / 3600);
	int MaxMinutes = ((static_cast<int>(MaxDuration) % 3600) / 60);
	int MaxSeconds = (static_cast<int>(MaxDuration) % 60);
	
	{
		char CurBuffer[9];
		snprintf(CurBuffer, sizeof(CurBuffer), "%02d:%02d:%02d", CurHours, CurMinutes, CurSeconds);
		const std::string CurPos(CurBuffer);
	
		char MaxBuffer[9];
		snprintf(MaxBuffer, sizeof(MaxBuffer), "%02d:%02d:%02d", MaxHours, MaxMinutes, MaxSeconds);
		const std::string MaxDur(MaxBuffer);
	
		const ImVec2& Text1Size = ImGui::CalcTextSize(CurPos.c_str());
		const ImVec2& Text1Pos = ImVec2(Min.x, Max.y - Height / 2.0f - Text1Size.y / 2.0f);
		DrawList->AddText(Text1Pos, ImColor(1.0f, 1.0f, 1.0f), CurPos.c_str());
	
		const ImVec2& Text2Size = ImGui::CalcTextSize(MaxDur.c_str());
		const ImVec2& Text2Pos = ImVec2(Max.x - Text2Size.x, Max.y - Height / 2.0f - Text2Size.y / 2.0f);
		DrawList->AddText(Text2Pos, ImColor(1.0f, 1.0f, 1.0f), MaxDur.c_str());
	}
	
	{
		const ImVec2& GapBorder = ImGui::CalcTextSize("88:88:88");
		float Ratio = static_cast<float>(CurrentPos) / static_cast<float>(MaxDuration);
	
		const ImVec2& AbsStart = ImVec2(Min.x + GapBorder.x + 5.0f, Max.y - Height / 2.0f - 5.0f);
		const ImVec2& AbsEnd = ImVec2(Max.x - GapBorder.x - 5.0f, Max.y - Height / 2.0f + 5.0f);
	
		const ImVec2& ProgStart = AbsStart;
		const ImVec2& ProgEnd = ImVec2(AbsStart.x + (AbsEnd.x - AbsStart.x) * Ratio, AbsEnd.y);
	
		DrawList->AddRectFilled(AbsStart, AbsEnd, ImColor(0.1f, 0.1f, 0.1f), 10.0f);
		DrawList->AddRectFilled(ProgStart, ProgEnd, ImColor(1.0f, 1.0f, 1.0f), 10.0f);
	}
}

void MusicPlayer_t::DrawNextButton() {
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImVec2 Min = ImGui::GetWindowPos();
	const ImVec2 Max = { Min.x + ImGui::GetWindowWidth(), Min.y + ImGui::GetWindowHeight() };

	float Width = Max.x - Min.x;
	float Height = Max.y - Min.y;

	const ImVec2 LeftCenter = { Min.x + Width / 5.0f, Min.y + Height / 2.0f };
	const ImVec2 RightCenter = { Min.x + Width / 1.8f, Min.y + Height / 2.0f };
	
	static bool HasPressed = false;
	static bool Animate = false;
	ImVec2 MousePos = ImGui::GetMousePos();
	if (MousePos.x > Min.x && MousePos.x < Max.x && MousePos.y > Min.y && MousePos.y < Max.y && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		HasPressed = true;

		if (this->OldTrack) {
			this->OldTrack->Free();
			delete this->OldTrack;
			this->OldTrack = nullptr;
		}

		std::filesystem::directory_entry NextTrack = this->GetNextTrack();

		this->OldTrack = this->CurrentTrack;
		this->OldTrack->FadeOut(this->TrackFade);

		this->CurrentTrack = new Track_t;
		this->CurrentTrack->Init(NextTrack, 0.0f);
		this->CurrentTrack->FadeIn(1.0f, this->Volume);
		this->CurrentTrack->Play();
	}

	static std::chrono::steady_clock::time_point NextButtonChange;
	static bool OldShowPlayButton = false;
	if (HasPressed) {
		NextButtonChange = std::chrono::high_resolution_clock::now();
		HasPressed = false;
		Animate = true;
	}

	float ButtonAnim = 0.0f;
	float TimeDiff = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - NextButtonChange).count()) / 150.0f;

	if (Animate) {
		ButtonAnim = std::clamp(TimeDiff, 0.0f, 1.0f);
	}
	
	// Draw arrow function
	auto Arrow = [](ImVec2 Center, float Size, ImDrawList* Drawlist) {

		std::vector<ImVec2> Offsets = {
			{ Center.x, Center.y - Size / 2 },
			{ Center.x + Size / 1.3f, Center.y },
			{ Center.x, Center.y + Size / 2 },
		};

		Drawlist->AddConcavePolyFilled(Offsets.data(), 3, ImColor(1.0f, 1.0f, 1.0f));
			
	};

	Arrow(ImVec2(LeftCenter.x + ButtonAnim * 18.0f, LeftCenter.y), 25.0f * (1.0f - ButtonAnim) + 15.0f * ButtonAnim, DrawList);
	if (ButtonAnim < 0.9f)
		Arrow(ImVec2(RightCenter.x + ButtonAnim * 13.0f, RightCenter.y), 15.0f * (1.0f - ButtonAnim), DrawList);
	Arrow(ImVec2(LeftCenter.x, RightCenter.y), ButtonAnim * 25.0f, DrawList);
}

void MusicPlayer_t::DrawPrevButton() {
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImVec2 Min = ImGui::GetWindowPos();
	const ImVec2 Max = { Min.x + ImGui::GetWindowWidth(), Min.y + ImGui::GetWindowHeight() };

	float Width = Max.x - Min.x;
	float Height = Max.y - Min.y;

	const ImVec2 LeftCenter = { Min.x + Width / 2.3f, Min.y + Height / 2.0f };
	const ImVec2 RightCenter = { Min.x + Width / 1.25f, Min.y + Height / 2.0f };

	static bool HasPressed = false;
	static bool Animate = false;
	ImVec2 MousePos = ImGui::GetMousePos();
	if (MousePos.x > Min.x && MousePos.x < Max.x && MousePos.y > Min.y && MousePos.y < Max.y && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		HasPressed = true;

		if (this->OldTrack) {
			this->OldTrack->Free();
			delete this->OldTrack;
			this->OldTrack = nullptr;
		}

		std::filesystem::directory_entry PrevTrack = this->GetPrevTrack();

		this->OldTrack = this->CurrentTrack;
		this->OldTrack->FadeOut(this->TrackFade);

		this->CurrentTrack = new Track_t;
		this->CurrentTrack->Init(PrevTrack, 0.0f);
		this->CurrentTrack->FadeIn(1.0f, this->Volume);
		this->CurrentTrack->Play();
	}

	static std::chrono::steady_clock::time_point NextButtonChange;
	static bool OldShowPlayButton = false;
	if (HasPressed) {
		NextButtonChange = std::chrono::high_resolution_clock::now();
		HasPressed = false;
		Animate = true;
	}

	float ButtonAnim = 0.0f;
	float TimeDiff = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - NextButtonChange).count()) / 150.0f;

	if (Animate) {
		ButtonAnim = std::clamp(TimeDiff, 0.0f, 1.0f);
	}

	// Draw arrow function
	auto Arrow = [](ImVec2 Center, float Size, ImDrawList* Drawlist) {

		std::vector<ImVec2> Offsets = {
			{ Center.x - Size / 1.3f, Center.y },
			{ Center.x, Center.y - Size / 2 },
			{ Center.x, Center.y + Size / 2 },
		};

		Drawlist->AddConcavePolyFilled(Offsets.data(), 3, ImColor(1.0f, 1.0f, 1.0f));

	};

	Arrow(ImVec2(RightCenter.x - ButtonAnim * 18.0f, RightCenter.y), 25.0f * (1.0f - ButtonAnim) + 15.0f * ButtonAnim, DrawList);
	if (ButtonAnim < 0.9f)
		Arrow(ImVec2(LeftCenter.x - ButtonAnim * 13.0f, LeftCenter.y), 15.0f * (1.0f - ButtonAnim), DrawList);
	Arrow(ImVec2(RightCenter.x, RightCenter.y), ButtonAnim * 25.0f, DrawList);
}

void MusicPlayer_t::DrawFreqResponse() {

	ImGuiIO* io = &ImGui::GetIO();
	
	// Deltatime for smoothing
	const std::vector<float>& Data = this->CurrentTrack->GetFFT(io->DeltaTime * 16.0f);
	
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImVec2 Min = ImGui::GetWindowPos();
	const ImVec2 Max = { Min.x + ImGui::GetWindowWidth(), Min.y + ImGui::GetWindowHeight() };
	
	float Width = Max.x - Min.x;
	float Height = Max.y - Min.y;
	float Padding = 5.0f;
	float BarWidth = Width / Data.size() - Padding + Padding / Data.size();
	
	for (size_t i = 0; i < Data.size(); i++) {
	
		float ModData = pow(Data[i], 0.7f) * 45.0f;
	
		float XStart = Min.x + i * (BarWidth + Padding);
		float XEnd = XStart + BarWidth;
	
		float YStart = Max.y - (Height / 2.0f + BarWidth / 2.0f);
		float YEnd = Max.y - (Height / 2.0f - BarWidth / 2.0f);
	
		const ImVec2& StartPos = ImVec2(XStart, YStart - ModData);
		const ImVec2& EndPos = ImVec2(XEnd, YEnd + ModData);
		DrawList->AddRectFilled(StartPos, EndPos, ImColor(1.0f, 1.0f, 1.0f), BarWidth);
	}
}

void MusicPlayer_t::DrawPlayButton() {
	
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImVec2 Min = ImGui::GetWindowPos();
	const ImVec2 Max = { Min.x + ImGui::GetWindowWidth(), Min.y + ImGui::GetWindowHeight() };
	
	bool IsMusicPlaying = this->CurrentTrack->GetActivity() == 1;
	
	static std::chrono::steady_clock::time_point PlayStateChange;
	static bool OldShowPlayButton = false;
	if (OldShowPlayButton != IsMusicPlaying) {
		PlayStateChange = std::chrono::high_resolution_clock::now();
		OldShowPlayButton = IsMusicPlaying;
	}
	
	float ButtonAnim = 0.0f;
	float TimeDiff = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - PlayStateChange).count()) / 150.0f;
	
	if (IsMusicPlaying) {
		ButtonAnim = std::clamp(1.0f - TimeDiff, 0.0f, 1.0f);
	} else {
		ButtonAnim = std::clamp(TimeDiff, 0.0f, 1.0f);
	}
	
	const ImVec2 Center = ImVec2(Max.x - (Max.x - Min.x) / 2.0f, Max.y - (Max.y - Min.y) / 2.0f);
	
	if (ButtonAnim > 0.5f) {
		float PlayButtonScale = std::clamp((ButtonAnim - 0.5f) * 2.0f, 0.0f, 1.0f);
		const std::vector<ImVec2> PlayButton = {
			ImVec2(Center.x - 15.0f * PlayButtonScale, Center.y - 15.0f * PlayButtonScale),
			ImVec2(Center.x + 15.0f * PlayButtonScale, Center.y),
			ImVec2(Center.x - 15.0f * PlayButtonScale, Center.y + 15.0f * PlayButtonScale)
		};
		DrawList->AddConvexPolyFilled(PlayButton.data(), 3, ImColor(1.0f, 1.0f, 1.0f));
	} else {
		float PauseButtonScale = std::clamp((0.5f - ButtonAnim) * 2.0f, 0.0f, 1.0f);
		const std::vector<ImVec2> PauseButtonL = {
			ImVec2(Center.x - 10.0f * PauseButtonScale, Center.y + 15.0f * PauseButtonScale),
			ImVec2(Center.x - 5.0f * PauseButtonScale, Center.y + 15.0f * PauseButtonScale),
			ImVec2(Center.x - 5.0f * PauseButtonScale, Center.y - 15.0f * PauseButtonScale),
			ImVec2(Center.x - 10.0f * PauseButtonScale, Center.y - 15.0f * PauseButtonScale),
		};
		const std::vector<ImVec2> PauseButtonR = {
			ImVec2(Center.x + 11.0f * PauseButtonScale, Center.y + 15.0f * PauseButtonScale),
			ImVec2(Center.x + 5.0f * PauseButtonScale, Center.y + 15.0f * PauseButtonScale),
			ImVec2(Center.x + 5.0f * PauseButtonScale, Center.y - 16.0f * PauseButtonScale),
			ImVec2(Center.x + 11.0f * PauseButtonScale, Center.y - 16.0f * PauseButtonScale),
		};
	
		DrawList->AddConvexPolyFilled(PauseButtonL.data(), 4, ImColor(1.0f, 1.0f, 1.0f));
		DrawList->AddConvexPolyFilled(PauseButtonR.data(), 4, ImColor(1.0f, 1.0f, 1.0f));
	}
	
	ImVec2 MousePos = ImGui::GetMousePos();
	if (MousePos.x > Min.x && MousePos.x < Max.x && MousePos.y > Min.y && MousePos.y < Max.y && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		if (this->CurrentTrack->GetActivity() == 1) {
			this->CurrentTrack->Pause();
		} else {
			this->CurrentTrack->Play();
		}
	}
}