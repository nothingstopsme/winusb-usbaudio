#ifndef __AUDIO_PLAYER_H__
#define __AUDIO_PLAYER_H__



#include <Windows.h>
#include <winusb.h>
#include <BaseTyps.h>
#include <Audioclient.h>



class AudioPlayer
{
public:
	AudioPlayer();
	~AudioPlayer();

	bool AudioPlayer::Start(const WINUSB_INTERFACE_HANDLE usbHandle, const WINUSB_PIPE_INFORMATION_EX &pipe);
	void Stop();
	void Wait();

private:
	
	//UINT32 ConvertAudio(UCHAR *output, const UCHAR *head, const UINT32 validLength, const int resamplingPeriod, const bool upsampling, int &resamplingCountdown);
	UINT32 ConvertAudio(UCHAR *output, const UCHAR *head, const UINT32 validLength, unsigned char bytesPerChannelAfterConversion,
		const double samplingInterval, const bool replicating, double &samplingCount);

	bool AudioPlayer::Prepare();

	static DWORD WINAPI Receive(LPVOID lpParam);
	

	static float CACHE_LENGTH_IN_SECS;
	static WAVEFORMATEX AOA_FORMAT;

	WINUSB_INTERFACE_HANDLE _usbHandle;
	const WINUSB_PIPE_INFORMATION_EX *_pipe;
	
	WORD _actualFormatTag;
	WAVEFORMATEX *_audioFormat;
	IAudioClient *_audioClient;
	IAudioRenderClient *_renderClient;
	
	
	HANDLE  _receiverThreadHandle;
	
	bool _running;
	



	

};


#endif
