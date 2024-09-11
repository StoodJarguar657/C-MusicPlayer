#ifndef MUSICPLAYER_HPP
#define MUSICPLAYER_HPP

#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>

#include <bass/bass.h>
#pragma comment(lib, "bass.lib")

class MusicPlayer_t {
public:

	struct Track_t {
	private:
		HMUSIC Stream = NULL;

		float OldVolume = 0.0f;
		float InternalVolume = 0.0f;
		float TargetVolume = 0.0f;

		float FadeLength = 0.0f;
		std::chrono::steady_clock::time_point StartFade;

		std::vector<float> FFT = {};
	public:
		bool IsFading = false;
		std::filesystem::directory_entry Path = std::filesystem::directory_entry("NULL");

		bool Init(const std::filesystem::directory_entry& Path, float Volume);
		
		bool Play();
		bool Pause();

		bool SetVolume(float Volume);

		bool Free();

		void FadeIn(float Seconds, float Volume);
		void FadeOut(float Seconds);

		void Copy(Track_t* Track);

		int GetActivity();

		double GetDuration();
		double GetCurrentPosition();

		void Update();

		std::vector<float> GetFFT(float DeltaTime);
	};

	// Internal music folder path
	std::filesystem::directory_entry MusicFolder;
	std::vector<std::filesystem::directory_entry> MusicTracks = {};

	std::filesystem::directory_entry GetNextTrack();
	std::filesystem::directory_entry GetPrevTrack();

public:

	float TrackFade = 5.0f; // Seconds

	float Volume = 100.0f;
	Track_t* OldTrack = nullptr;
	Track_t* CurrentTrack = nullptr;

	MusicPlayer_t();

	void Update();

	void DrawDuration();
	void DrawNextButton();
	void DrawPrevButton();

	void DrawFreqResponse();
	void DrawPlayButton();

} extern MusicPlayer;


#endif MUSICPLAYER_HPP