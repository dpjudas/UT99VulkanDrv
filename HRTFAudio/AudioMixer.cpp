
#include "Precomp.h"
#include "AudioMixer.h"
#include "AudioSource.h"
#include "AudioPlayer.h"
#include "kissfft/kiss_fft.h"
#include "miniz/miniz.h"
#include <mutex>
#include <stdexcept>
#include <map>
#include <string>
#include <cmath>

class AudioMixerImpl;

class AudioSound
{
public:
	AudioSound(int mixing_frequency, std::unique_ptr<AudioSource> source, const AudioLoopInfo& inloopinfo) : loopinfo(inloopinfo)
	{
		duration = samples.size() / 44100.0f;

		if (source->GetChannels() == 1)
		{
			uint64_t oldSampleCount = source->GetSamples();
			if (source->GetFrequency() != mixing_frequency)
			{
				source = AudioSource::CreateResampler(mixing_frequency, std::move(source));
			}

			samples.resize(source->GetSamples());
			samples.resize(source->ReadSamples(samples.data(), samples.size()));

			// Remove any audio pops at end of the sound that the resampler might have created
			if (samples.size() >= 16)
			{
				for (size_t i = 1; i < 16; i++)
				{
					float t = i / 16.0f;
					size_t pos = samples.size() - i;
					samples[pos] = samples[pos] * t;
				}
			}

			if (oldSampleCount > 0)
			{
				uint64_t newSampleCount = samples.size();
				loopinfo.LoopStart = std::max(std::min(loopinfo.LoopStart * newSampleCount / oldSampleCount, newSampleCount - 1), (uint64_t)0);
				loopinfo.LoopEnd = std::max(std::min(loopinfo.LoopEnd * newSampleCount / oldSampleCount, newSampleCount), (uint64_t)0);
			}
			else
			{
				loopinfo = {};
			}
		}
		else
		{
			throw std::runtime_error("Only mono sounds are supported");
		}
	}

	std::vector<float> samples;
	float duration = 0.0f;
	AudioLoopInfo loopinfo;
};

class HRTFAudioChannel;

class ActiveSound
{
public:
	int channel = 0;
	bool play = false;
	bool update = false;
	AudioSound* sound = nullptr;
	float volume = 0.0f;
	float pan = 0.0f;
	float pitch = 0.0f;
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;

	double pos = 0;
	HRTFAudioChannel* prevHrtfChannel = nullptr;
	HRTFAudioChannel* curHrtfChannel = nullptr;

	bool SoundEnded() const
	{
		return pos >= (double)sound->samples.size();
	}

	bool MixInto(kiss_fft_cpx* outputOld, kiss_fft_cpx* outputNew, size_t samples)
	{
		if (SoundEnded())
			return false;

		const float* src = sound->samples.data();
		double srcpos = pos;
		double srcpitch = pitch;
		int srcsize = (int)sound->samples.size();
		int srcmax = srcsize - 1;
		if (srcmax < 0)
			return false;

		float vol = volume;
		float rcpSamples = 1.0f / samples;

		if (sound->loopinfo.Looped)
		{
			double loopStart = (double)sound->loopinfo.LoopStart;
			double loopEnd = (double)sound->loopinfo.LoopEnd;
			double loopLen = loopEnd - loopStart;

			for (size_t i = 0; i < samples; i++)
			{
				double p0 = srcpos;
				double p1 = srcpos + 1;
				if (p0 >= loopEnd) p0 -= loopLen;
				if (p1 >= loopEnd) p1 -= loopLen;

				int pos = (int)p0;
				int pos2 = (int)p1;
				float t = (float)(p0 - pos);

				pos = std::min(pos, srcmax);
				pos2 = std::min(pos2, srcmax);

				float value = (src[pos] * t + src[pos2] * (1.0f - t)) * vol;
				float tt = i * rcpSamples;
				outputOld[i].r += value * (1.0f - tt);
				outputNew[i].r += value * tt;

				srcpos += srcpitch;
				while (srcpos >= loopEnd)
					srcpos -= loopLen;
			}

			pos = srcpos;
			return true;
		}
		else
		{
			for (size_t i = 0; i < samples; i++)
			{
				int pos = (int)srcpos;
				int pos2 = pos + 1;
				float t = (float)(srcpos - pos);

				pos = std::min(pos, srcmax);
				pos2 = std::min(pos2, srcmax);

				float value = (src[pos] * t + src[pos2] * (1.0f - t)) * vol;
				float tt = i * rcpSamples;
				outputOld[i].r += value * (1.0f - tt);
				outputNew[i].r += value * tt;

				srcpos += srcpitch;
			}

			pos = srcpos;
			return pos < (double)sound->samples.size();
		}
	}

	bool MixInto(kiss_fft_cpx* output, size_t samples)
	{
		if (SoundEnded())
			return false;

		const float* src = sound->samples.data();
		double srcpos = pos;
		double srcpitch = pitch;
		int srcsize = (int)sound->samples.size();
		int srcmax = srcsize - 1;
		if (srcmax < 0)
			return false;

		float vol = volume;

		if (sound->loopinfo.Looped)
		{
			double loopStart = (double)sound->loopinfo.LoopStart;
			double loopEnd = (double)sound->loopinfo.LoopEnd;
			double loopLen = loopEnd - loopStart;

			for (size_t i = 0; i < samples; i++)
			{
				double p0 = srcpos;
				double p1 = srcpos + 1;
				if (p0 >= loopEnd) p0 -= loopLen;
				if (p1 >= loopEnd) p1 -= loopLen;

				int pos = (int)p0;
				int pos2 = (int)p1;
				float t = (float)(p0 - pos);

				pos = std::min(pos, srcmax);
				pos2 = std::min(pos2, srcmax);

				float value = (src[pos] * t + src[pos2] * (1.0f - t));
				output[i].r += value * vol;

				srcpos += srcpitch;
				while (srcpos >= loopEnd)
					srcpos -= loopLen;
			}

			pos = srcpos;
			return true;
		}
		else
		{
			for (size_t i = 0; i < samples; i++)
			{
				int pos = (int)srcpos;
				int pos2 = pos + 1;
				float t = (float)(srcpos - pos);

				pos = std::min(pos, srcmax);
				pos2 = std::min(pos2, srcmax);

				float value = (src[pos] * t + src[pos2] * (1.0f - t));
				output[i].r += value * vol;

				srcpos += srcpitch;
			}

			pos = srcpos;
			return pos < (double)sound->samples.size();
		}
	}

	bool MixInto(float* output, size_t samples, float globalvolume)
	{
		const float* src = sound->samples.data();
		double srcpos = pos;
		double srcpitch = pitch;
		int srcsize = (int)sound->samples.size();
		int srcmax = srcsize - 1;
		if (srcmax < 0)
			return false;

		float leftVolume, rightVolume;
		GetVolume(leftVolume, rightVolume, globalvolume);

		if (sound->loopinfo.Looped)
		{
			double loopStart = (double)sound->loopinfo.LoopStart;
			double loopEnd = (double)sound->loopinfo.LoopEnd;
			double loopLen = loopEnd - loopStart;

			for (size_t i = 0; i < samples; i++)
			{
				double p0 = srcpos;
				double p1 = srcpos + 1;
				if (p0 >= loopEnd) p0 -= loopLen;
				if (p1 >= loopEnd) p1 -= loopLen;

				int pos = (int)p0;
				int pos2 = (int)p1;
				float t = (float)(p0 - pos);

				pos = std::min(pos, srcmax);
				pos2 = std::min(pos2, srcmax);

				float value = (src[pos] * t + src[pos2] * (1.0f - t));
				output[i << 1] += value * leftVolume;
				output[(i << 1) + 1] += value * rightVolume;

				srcpos += srcpitch;
				while (srcpos >= loopEnd)
					srcpos -= loopLen;
			}

			pos = srcpos;
			return true;
		}
		else
		{
			if (leftVolume > 0.0f || rightVolume > 0.0f)
			{
				for (size_t i = 0; i < samples; i++)
				{
					int pos = (int)srcpos;
					int pos2 = pos + 1;
					float t = (float)(srcpos - pos);

					pos = std::min(pos, srcmax);
					pos2 = std::min(pos2, srcmax);

					float value = (src[pos] * t + src[pos2] * (1.0f - t));
					output[i << 1] += value * leftVolume;
					output[(i << 1) + 1] += value * rightVolume;

					srcpos += srcpitch;
				}
			}
			else
			{
				srcpos += srcpitch * samples;
			}

			if (srcpos < sound->samples.size())
			{
				pos = srcpos;
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	void GetVolume(float& leftVolume, float& rightVolume, float globalvolume)
	{
		auto clamp = [](float x, float minval, float maxval) { return std::max(std::min(x, maxval), minval); };
		leftVolume = clamp(volume * std::min(1.0f - pan, 1.0f) * globalvolume, 0.0f, 1.0f);
		rightVolume = clamp(volume * std::min(1.0f + pan, 1.0f) * globalvolume, 0.0f, 1.0f);
	}
};

class ZipFileStream
{
public:
	ZipFileStream(std::vector<uint8_t> data) : data(std::move(data)) { }

	void read(void* ptr, size_t size)
	{
		size_t available = data.size() - pos;
		if (size <= available)
		{
			memcpy(ptr, data.data() + pos, size);
			pos += size;
		}
		else
		{
			throw std::runtime_error("Unexpected end of file");
		}
	}

	uint32_t read_double() { double d; read(&d, sizeof(double)); return d; }
	uint32_t read_float() { float d; read(&d, sizeof(float)); return d; }
	int32_t read_int32() { int32_t d; read(&d, sizeof(int32_t)); return d; }
	int16_t read_int16() { int16_t d; read(&d, sizeof(int16_t)); return d; }
	int8_t read_int8() { int8_t d; read(&d, sizeof(int8_t)); return d; }
	uint32_t read_uint32() { uint32_t d; read(&d, sizeof(uint32_t)); return d; }
	uint16_t read_uint16() { uint16_t d; read(&d, sizeof(uint16_t)); return d; }
	uint8_t read_uint8() { uint8_t d; read(&d, sizeof(uint8_t)); return d; }

private:
	size_t pos = 0;
	std::vector<uint8_t> data;
};

class ZipReader
{
public:
	ZipReader(const void* data, size_t size)
	{
		mz_bool result = mz_zip_reader_init_mem(&zip, data, size, 0);
		if (result == MZ_FALSE)
			throw std::runtime_error("Could not open zip file");
	}

	~ZipReader()
	{
		mz_zip_reader_end(&zip);
	}

	std::unique_ptr<ZipFileStream> read_file(const std::string& filename)
	{
		mz_uint32 file_index = 0;
		mz_bool result = mz_zip_reader_locate_file_v2(&zip, filename.c_str(), nullptr, 0, &file_index);
		if (result == MZ_FALSE)
			throw std::runtime_error("Could not find " + filename);

		mz_zip_archive_file_stat info = {};
		result = mz_zip_reader_file_stat(&zip, file_index, &info);
		if (result == MZ_FALSE)
			throw std::runtime_error("Could not get file stat for " + filename);

		std::vector<uint8_t> buffer(info.m_uncomp_size);
		result = mz_zip_reader_extract_to_mem(&zip, file_index, buffer.data(), buffer.size(), 0);
		if (result == MZ_FALSE)
			throw std::runtime_error("Could not extract " + filename);
		return std::make_unique<ZipFileStream>(std::move(buffer));
	}

private:
	mz_zip_archive zip = {};
};

class HRTF_Direction
{
public:
	void load(std::unique_ptr<ZipFileStream> file)
	{
		uint32_t ChunkID = file->read_uint32(); // "RIFF"
		uint32_t ChunkSize = file->read_uint32();
		uint32_t Format = file->read_uint32(); // "WAVE"
		uint32_t FormatChunkID = file->read_uint32(); // "fmt "
		uint32_t FormatChunkSize = file->read_uint32();
		uint16_t AudioFormat = file->read_uint16();
		uint16_t NumChannels = file->read_uint16();
		uint32_t SampleRate = file->read_uint32();
		uint32_t ByteRate = file->read_uint32();
		uint16_t BlockAlign = file->read_uint16();
		uint16_t BitsPerSample = file->read_uint16();
		uint32_t DataChunkID = file->read_uint32(); // "data"
		uint32_t DataChunkSize = file->read_uint32();

		if (ChunkID != 0x46464952 || Format != 0x45564157 || FormatChunkID != 0x20746d66)
			throw std::runtime_error("Not a wav file");

		if (FormatChunkSize != 16 || DataChunkID != 0x61746164)
			throw std::runtime_error("Wav file subformat not supported");

		if (NumChannels != 2 || BitsPerSample != 16 || BlockAlign != 4)
			throw std::runtime_error("Wav file must be 16 bit stereo");

		std::vector<int16_t> samples(DataChunkSize / sizeof(int16_t));
		file->read(samples.data(), samples.size() * sizeof(int16_t));
		file.reset();

		size_t size = samples.size() / 2;
		if (size > 128)
			throw std::runtime_error("Invalid HRTF file");

		std::vector<kiss_fft_cpx> left(nfft);
		std::vector<kiss_fft_cpx> right(nfft);
		memset(left.data(), 0, left.size() * sizeof(kiss_fft_cpx));
		memset(right.data(), 0, right.size() * sizeof(kiss_fft_cpx));

		for (size_t i = 0; i < size; i++)
		{
			left[i].r = samples[i << 1] * (1.0f / 32768.0f);
			right[i].r = samples[(i << 1) + 1] * (1.0f / 32768.0f);
		}

		left_freq = transform_channel(left);
		right_freq = transform_channel(right);
	}

	std::vector<kiss_fft_cpx> transform_channel(const std::vector<kiss_fft_cpx>& samples)
	{
		std::vector<kiss_fft_cpx> freq(samples.size());
		memset(freq.data(), 0, freq.size() * sizeof(kiss_fft_cpx));

		kiss_fft_cfg cfg = kiss_fft_alloc((int)samples.size(), 0, nullptr, nullptr);
		kiss_fft(cfg, samples.data(), freq.data());
		kiss_fft_free(cfg);

		return freq;
	}

	static const int nfft = 1024;

	std::vector<kiss_fft_cpx> left_freq;
	std::vector<kiss_fft_cpx> right_freq;
};

class HRTF_Data
{
public:
	HRTF_Data(ZipReader* zip)
	{
		data.resize(N_ELEV);
		for (int el_index = 0; el_index < N_ELEV; el_index++)
		{
			int nfaz = get_nfaz(el_index);
			data[el_index].resize(nfaz);
			for (int az_index = 0; az_index < nfaz; az_index++)
			{
				data[el_index][az_index].load(zip->read_file(hrtf_name(el_index, az_index)));
			}
		}

		cfg_forward = kiss_fft_alloc(HRTF_Direction::nfft, 0, nullptr, nullptr);
		cfg_inverse = kiss_fft_alloc(HRTF_Direction::nfft, 1, nullptr, nullptr);
	}

	~HRTF_Data()
	{
		kiss_fft_free(cfg_forward);
		kiss_fft_free(cfg_inverse);
	}

	// Get the closest HRTF to the specified elevation and azimuth in degrees
	void get_hrtf(float elev, float azim, kiss_fft_cpx** p_left, kiss_fft_cpx** p_right)
	{
		// Clip angles and convert to indices
		int el_index = 0, az_index = 0;
		bool flip_flag = false;
		get_indices(elev, azim, &el_index, &az_index, &flip_flag);

		// Get data and flip channels if necessary
		auto hd = &data[el_index][az_index];
		if (flip_flag)
		{
			*p_left = hd->right_freq.data();
			*p_right = hd->left_freq.data();
		}
		else
		{
			*p_left = hd->left_freq.data();
			*p_right = hd->right_freq.data();
		}
	}

	kiss_fft_cfg cfg_forward;
	kiss_fft_cfg cfg_inverse;

private:
	// Return the number of azimuths actually stored in file system
	int get_nfaz(int el_index)
	{
		return (elev_data[el_index] / 2) + 1;
	}

	// Get (closest) elevation index for given elevation
	int get_el_index(float elev)
	{
		int el_index = (int)std::round((elev - MIN_ELEV) / ELEV_INC);
		if (el_index < 0)
			el_index = 0;
		else if (el_index >= N_ELEV)
			el_index = N_ELEV - 1;
		return el_index;
	}

	// For a given elevation and azimuth in degrees, return the indices for the proper HRTF
	// *p_flip will be set true if left and right channels need to be flipped
	void get_indices(float elev, float azim, int* p_el, int* p_az, bool* p_flip)
	{
		int el_index = get_el_index(elev);
		int naz = elev_data[el_index];
		int nfaz = get_nfaz(el_index);

		// Coerce azimuth into legal range and calculate flip if any
		azim = std::fmod(azim, 360.0f);
		if (azim < 0) azim += 360.0f;
		if (azim > 180.0f)
		{
			azim = 360.0f - azim;
			*p_flip = true;
		}

		// Now 0 <= azim <= 180. Calculate index and clip to legal range just to be sure
		int az_index = (int)std::round(azim / (360.0 / naz));
		if (az_index < 0)
			az_index = 0;
		else if (az_index >= nfaz)
			az_index = nfaz - 1;

		*p_el = el_index;
		*p_az = az_index;
	}

	// Convert index to elevation
	int index_to_elev(int el_index)
	{
		return MIN_ELEV + el_index * ELEV_INC;
	}

	// Convert index to azimuth
	int index_to_azim(int el_index, int az_index)
	{
		return (int)std::round(az_index * 360.0f / elev_data[el_index]);
	}

	// Return pathname of HRTF specified by indices
	std::string hrtf_name(int el_index, int az_index)
	{
		int elev = index_to_elev(el_index);
		int azim = index_to_azim(el_index, az_index);

		// zero prefix azim number
		std::string azimstr = std::to_string(azim);
		azimstr = std::string(3 - azimstr.size(), '0') + azimstr;

		return "elev" + std::to_string(elev) + "/H" + std::to_string(elev) + "e" + azimstr + "a.wav";
	}

	static const int MIN_ELEV = -40;
	static const int MAX_ELEV = 90;
	static const int ELEV_INC = 10;
	static const int N_ELEV = (((MAX_ELEV - MIN_ELEV) / ELEV_INC) + 1);

	// This array gives the total number of azimuths measured per elevation, and hence the AZIMUTH INCREMENT.
	// Note that only azimuths up to and including 180 degrees actually exist in file system (because the data is symmetrical).
	static int elev_data[N_ELEV];

	std::vector<std::vector<HRTF_Direction>> data;
};

int HRTF_Data::elev_data[N_ELEV] = { 56, 60, 72, 72, 72, 72, 72, 60, 56, 45, 36, 24, 12, 1 };

class HRTFAudioChannel
{
public:
	HRTFAudioChannel(kiss_fft_cpx* leftHRTF, kiss_fft_cpx* rightHRTF, HRTF_Data* hrtf) : leftHRTF(leftHRTF), rightHRTF(rightHRTF), hrtf(hrtf)
	{
		kiss_fft_cpx zero;
		zero.i = 0.0f;
		zero.r = 0.0f;
		left.resize(HRTF_Direction::nfft, 0.0f);
		right.resize(HRTF_Direction::nfft, 0.0f);
		samples_buf.resize(HRTF_Direction::nfft, zero);
		time_buf.resize(HRTF_Direction::nfft, zero);
		freq_buf.resize(HRTF_Direction::nfft, zero);
		workspace_buf.resize(HRTF_Direction::nfft, zero);
	}

	void MixInto(float* output)
	{
		size_t samples = HRTF_Direction::nfft / 2;
		size_t quarter = HRTF_Direction::nfft / 4;
		const float* l = left.data() + quarter;
		const float* r = right.data() + quarter;
		for (size_t i = 0; i < samples; i++)
		{
			*(output++) += *(l++);
			*(output++) += *(r++);
		}
	}

	void BeginFrame()
	{
		size_t half = HRTF_Direction::nfft / 2;
		memmove(samples_buf.data(), samples_buf.data() + half, half * sizeof(kiss_fft_cpx));
		memset(samples_buf.data() + half, 0, half * sizeof(kiss_fft_cpx));
	}

	kiss_fft_cpx* GetBuffer()
	{
		size_t half = HRTF_Direction::nfft / 2;
		return samples_buf.data() + half;
	}

	void EndFrame()
	{
		time_buf = samples_buf;
		kiss_fft(hrtf->cfg_forward, time_buf.data(), freq_buf.data());

		ApplyHRTF(left.data(), leftHRTF);
		ApplyHRTF(right.data(), rightHRTF);
	}

	kiss_fft_cpx* leftHRTF = nullptr;
	kiss_fft_cpx* rightHRTF = nullptr;
	std::vector<ActiveSound*> sounds;

private:
	HRTFAudioChannel(const HRTFAudioChannel&) = delete;
	HRTFAudioChannel& operator=(const HRTFAudioChannel&) = delete;

	std::vector<kiss_fft_cpx> samples_buf, time_buf, freq_buf, workspace_buf;
	std::vector<float> left, right;

	static kiss_fft_cpx CMul(const kiss_fft_cpx& a, const kiss_fft_cpx& b)
	{
		kiss_fft_cpx c;
		c.r = a.r * b.r - a.i * b.i;
		c.i = a.i * b.r + a.r * b.i;
		return c;
	}

	// Optimized multiply of two conjugate symmetric arrays (c = a * b)
	static void COptMul(const kiss_fft_cpx* a, const kiss_fft_cpx* b, kiss_fft_cpx* c, size_t count)
	{
		size_t half = count >> 1;
		for (size_t i = 0; i <= half; i++)
		{
			c[i] = CMul(a[i], b[i]);
		}
		for (size_t i = 1; i < half; i++)
		{
			c[half + i].r = c[half - i].r;
			c[half + i].i = -c[half - i].i;
		}
	}

	void ApplyHRTF(float* samples, kiss_fft_cpx* hrtf)
	{
		COptMul(freq_buf.data(), hrtf, workspace_buf.data(), freq_buf.size());
		kiss_fft(this->hrtf->cfg_inverse, workspace_buf.data(), time_buf.data());

		size_t nfft = HRTF_Direction::nfft;
		float rcp_nfft = 1.0f / nfft;
		for (size_t i = 0; i < nfft; i++)
			samples[i] = time_buf[i].r * rcp_nfft;
	}

	static float clamp(float v, float minval, float maxval)
	{
		return std::max(std::min(v, maxval), minval);
	}

	HRTF_Data* hrtf = nullptr;
};

// comb filter: output[i] = input[i] + gain * output[i - delay]
class ReverbCombFilter
{
public:
	ReverbCombFilter()
	{
		outbuffer.resize(0x8000, 0.0f);
	}

	std::vector<float> outbuffer;
	size_t bufferpos = 0;

	void filter(float* data, size_t samples, size_t delay, float gain)
	{
		if (delay > 0x7fff)
			delay = 0x7fff;

		float* outb = outbuffer.data();
		size_t pos = bufferpos;

		for (size_t i = 0; i < samples; i++)
		{
			size_t delaypos = (pos - delay) & 0x7fff;
			float input = data[i];

			float delayoutput = outb[delaypos];
			float output = input + gain * delayoutput;
			outb[pos] = output;

			data[i] = output;
			pos = (pos + 1) & 0x7fff;
		}

		bufferpos = pos;
	}
};

class AudioMixerSource : public AudioSource
{
public:
	AudioMixerSource(AudioMixerImpl* mixer, ZipReader *zip) : mixer(mixer), hrtf(zip)
	{
		size_t framesize = HRTF_Direction::nfft / 2;
		soundframe.resize(framesize * 2);
	}

	int GetFrequency() override;
	int GetChannels() override { return 2; }
	int GetSamples() override { return -1; }
	void SeekToSample(uint64_t position) override { }
	size_t ReadSamples(float* output, size_t samples) override;

	void TransferFromClient();
	void CopyMusic(float* output, size_t samples);
	void MixSounds(float* output, size_t samples);

	void MixFrame();

	AudioMixerImpl* mixer = nullptr;
	std::map<int, ActiveSound> sounds;
	std::vector<int> stoppedsounds;
	std::unique_ptr<AudioSource> music;
	float soundvolume = 1.0f;
	float musicvolume = 1.0f;
	struct
	{
		float volume = 0.0f;
		int hfcutoff = 44100;
		std::vector<float> time;
		std::vector<float> gain;
		std::vector<ReverbCombFilter> filters;
	} reverb;

	HRTF_Data hrtf;
	std::vector<std::unique_ptr<HRTFAudioChannel>> hrtfchannels;
	std::vector<float> soundframe;
	size_t playPos = 0;
};

class AudioMixerImpl : public AudioMixer
{
public:
	AudioMixerImpl(const void *zipData, size_t zipSize)
	{
		ZipReader zip(zipData, zipSize);
		player = AudioPlayer::Create(std::make_unique<AudioMixerSource>(this, &zip));
	}

	~AudioMixerImpl()
	{
		player.reset();
	}

	AudioSound* AddSound(std::unique_ptr<AudioSource> source, const AudioLoopInfo& loopinfo) override
	{
		sounds.push_back(std::make_unique<AudioSound>(mixing_frequency, std::move(source), loopinfo));
		return sounds.back().get();
	}

	void RemoveSound(AudioSound* sound) override
	{
	}

	float GetSoundDuration(AudioSound* sound) override
	{
		return sound->duration;
	}

	int PlaySound(int channel, AudioSound* sound, float volume, float pan, float pitch, float x, float y, float z) override
	{
		ActiveSound s;
		s.channel = channel;
		s.play = true;
		s.update = false;
		s.sound = sound;
		s.volume = volume;
		s.pan = pan;
		s.pitch = pitch;
		s.x = x;
		s.y = y;
		s.z = z;
		client.sounds.push_back(std::move(s));
		channelplaying[channel]++;
		return channel;
	}

	void UpdateSound(int channel, AudioSound* sound, float volume, float pan, float pitch, float x, float y, float z) override
	{
		ActiveSound s;
		s.channel = channel;
		s.play = false;
		s.update = true;
		s.sound = sound;
		s.volume = volume;
		s.pan = pan;
		s.pitch = pitch;
		s.x = x;
		s.y = y;
		s.z = z;
		client.sounds.push_back(std::move(s));
	}

	void StopSound(int channel) override
	{
		ActiveSound s;
		s.channel = channel;
		s.play = false;
		s.update = false;
		client.sounds.push_back(std::move(s));
	}

	bool SoundFinished(int channel) override
	{
		auto it = channelplaying.find(channel);
		return it == channelplaying.end() || it->second == 0;
	}

	void PlayMusic(std::unique_ptr<AudioSource> source) override
	{
		if (source && source->GetChannels() != 2)
			throw std::runtime_error("Only stereo music is supported");

		if (source && source->GetFrequency() != mixing_frequency)
		{
			source = AudioSource::CreateResampler(mixing_frequency, std::move(source));
		}
		client.music = std::move(source);
		client.musicupdate = true;
	}

	void SetMusicVolume(float volume) override
	{
		client.musicvolume = volume;
	}

	void SetSoundVolume(float volume) override
	{
		client.soundvolume = volume;
	}

	void SetReverb(float volume, float hfcutoff, std::vector<float> time, std::vector<float> gain)
	{
		client.reverb.volume = volume;
		client.reverb.hfcutoff = hfcutoff;
		client.reverb.time = std::move(time);
		client.reverb.gain = std::move(gain);
	}

	void Update() override
	{
		std::unique_lock<std::mutex> lock(mutex);
		for (ActiveSound& s : client.sounds)
			transfer.sounds.push_back(s);
		client.sounds.clear();
		if (client.musicupdate)
		{
			transfer.music = std::move(client.music);
			transfer.musicupdate = true;
		}
		client.music = {};
		client.musicupdate = false;

		transfer.musicvolume = client.musicvolume;
		transfer.soundvolume = client.soundvolume;
		transfer.reverb = client.reverb;

		for (int c : channelstopped)
		{
			auto it = channelplaying.find(c);
			it->second--;
			if (it->second == 0)
				channelplaying.erase(it);
		}
		channelstopped.clear();
	}

	int mixing_frequency = 44100;

	std::vector<std::unique_ptr<AudioSound>> sounds;
	std::unique_ptr<AudioPlayer> player;
	std::map<int, int> channelplaying;

	struct
	{
		std::vector<ActiveSound> sounds;
		std::unique_ptr<AudioSource> music;
		bool musicupdate = false;
		float soundvolume = 1.0f;
		float musicvolume = 1.0f;
		struct
		{
			float volume = 0.0f;
			int hfcutoff = 44100;
			std::vector<float> time;
			std::vector<float> gain;
		} reverb;
	} client, transfer;
	std::vector<int> channelstopped;

	std::mutex mutex;
	int nextid = 1;
};

/////////////////////////////////////////////////////////////////////////////

std::unique_ptr<AudioMixer> AudioMixer::Create(const void* zipData, size_t zipSize)
{
	return std::make_unique<AudioMixerImpl>(zipData, zipSize);
}

/////////////////////////////////////////////////////////////////////////////

int AudioMixerSource::GetFrequency()
{
	return mixer->mixing_frequency;
}

size_t AudioMixerSource::ReadSamples(float* output, size_t samples)
{
	TransferFromClient();
	CopyMusic(output, samples);
	MixSounds(output, samples);
	return samples;
}

void AudioMixerSource::TransferFromClient()
{
	std::unique_lock<std::mutex> lock(mixer->mutex);
	for (ActiveSound& s : mixer->transfer.sounds)
	{
		if (!s.update) // if play or stop
		{
			auto it = sounds.find(s.channel);
			if (it != sounds.end())
			{
				stoppedsounds.push_back(s.channel);
				sounds.erase(it);
			}
		}

		if (s.play)
		{
			sounds[s.channel] = s;
		}
		else if (s.update)
		{
			auto it = sounds.find(s.channel);
			if (it != sounds.end())
			{
				it->second.volume = s.volume;
				it->second.pan = s.pan;
				it->second.pitch = s.pitch;
				it->second.x = s.x;
				it->second.y = s.y;
				it->second.z = s.z;
			}
		}
	}
	mixer->transfer.sounds.clear();

	if (mixer->transfer.musicupdate)
		music = std::move(mixer->transfer.music);
	mixer->transfer.musicupdate = false;

	mixer->channelstopped.insert(mixer->channelstopped.end(), stoppedsounds.begin(), stoppedsounds.end());
	stoppedsounds.clear();

	soundvolume = mixer->transfer.soundvolume;
	musicvolume = mixer->transfer.musicvolume;

	reverb.volume = mixer->transfer.reverb.volume;
	reverb.hfcutoff = mixer->transfer.reverb.hfcutoff;
	reverb.time = mixer->transfer.reverb.time;
	reverb.gain = mixer->transfer.reverb.gain;
}

void AudioMixerSource::CopyMusic(float* output, size_t samples)
{
	size_t pos = 0;

	if (music && music->GetChannels() == 2) // Only support stereo music for now
	{
		pos = music->ReadSamples(output, samples);
	}

	for (size_t i = pos; i < samples; i++)
	{
		output[i] = 0.0f;
	}

	for (size_t i = 0; i < samples; i++)
	{
		output[i] *= musicvolume;
	}
}

void AudioMixerSource::MixSounds(float* output, size_t samples)
{
	samples /= 2;

	// Mix the sound frame into the output stream
	size_t framesize = soundframe.size() / 2;
	while (samples > 0)
	{
		size_t available = std::min(framesize - playPos, samples);
		if (available > 0)
		{
			const float* input = soundframe.data() + playPos * 2;
			float vol = soundvolume;
			for (size_t i = 0; i < available; i++)
			{
				*(output++) += *(input++) * vol;
				*(output++) += *(input++) * vol;
			}
			playPos += available;
			samples -= available;
		}
		else
		{
			MixFrame();
			playPos = 0;
		}
	}
}

void AudioMixerSource::MixFrame()
{
	size_t framesize = soundframe.size() / 2;

	// Place sounds into directional channels
	for (auto& it : sounds)
	{
		ActiveSound& sound = it.second;

		float elev = Clamp(std::atan2(sound.y, std::abs(sound.z)) * 180.0f / 3.14159265359f, -90.0f, 90.0f);
		float azim = Clamp(std::atan2(sound.x, sound.z) * 180.0f / 3.14159265359f, -180.0f, 180.0f);

		kiss_fft_cpx* leftHRTF = nullptr;
		kiss_fft_cpx* rightHRTF = nullptr;
		hrtf.get_hrtf(elev, azim, &leftHRTF, &rightHRTF);

		HRTFAudioChannel* hrtfchannel = nullptr;
		for (auto& c : hrtfchannels)
		{
			if (c->leftHRTF == leftHRTF && c->rightHRTF == rightHRTF)
			{
				hrtfchannel = c.get();
				break;
			}
		}
		if (!hrtfchannel)
		{
			hrtfchannels.push_back(std::make_unique<HRTFAudioChannel>(leftHRTF, rightHRTF, &hrtf));
			hrtfchannel = hrtfchannels.back().get();
		}

		sound.prevHrtfChannel = sound.curHrtfChannel;
		sound.curHrtfChannel = hrtfchannel;

		if (sound.prevHrtfChannel && sound.prevHrtfChannel != sound.curHrtfChannel)
			sound.prevHrtfChannel->sounds.push_back(&sound);
		hrtfchannel->sounds.push_back(&sound);
	}

	// Remove unused channels
	auto itHrtf = hrtfchannels.begin();
	while (itHrtf != hrtfchannels.end())
	{
		if ((*itHrtf)->sounds.empty())
			itHrtf = hrtfchannels.erase(itHrtf);
		else
			++itHrtf;
	}

	// Mix a frame of audio

	for (auto& hrtfchannel : hrtfchannels)
		hrtfchannel->BeginFrame();

	for (auto& it : sounds)
	{
		ActiveSound& sound = it.second;
		if (!sound.prevHrtfChannel || sound.prevHrtfChannel == sound.curHrtfChannel)
		{
			sound.MixInto(sound.curHrtfChannel->GetBuffer(), framesize);
		}
		else
		{
			sound.MixInto(sound.prevHrtfChannel->GetBuffer(), sound.curHrtfChannel->GetBuffer(), framesize);
		}
	}

	for (auto& hrtfchannel : hrtfchannels)
	{
		hrtfchannel->EndFrame();
		hrtfchannel->sounds.clear();
	}

	memset(soundframe.data(), 0, soundframe.size() * sizeof(float));
	for (auto& hrtfchannel : hrtfchannels)
		hrtfchannel->MixInto(soundframe.data());

	// Remove sounds that finished playing
	auto it = sounds.begin();
	while (it != sounds.end())
	{
		ActiveSound& sound = it->second;
		if (sound.SoundEnded())
		{
			stoppedsounds.push_back(sound.channel);
			it = sounds.erase(it);
		}
		else
		{
			++it;
		}
	}

	// Apply reverb effect
	reverb.filters.resize(reverb.time.size());
	for (size_t i = 0; i < reverb.filters.size(); i++)
	{
		size_t delay = std::max(std::min((int)std::round(reverb.time[i] * mixer->mixing_frequency), 0x7fff / 2), 1);
		float gain = reverb.gain[i] * reverb.volume;
		reverb.filters[i].filter(soundframe.data(), framesize * 2, delay * 2, gain);
	}
}
