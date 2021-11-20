#pragma once

#include <memory>
#include <vector>

class vec3
{
public:
	vec3() : x(0.0f), y(0.0f), z(0.0f) { }
	vec3(float x, float y, float z) : x(x), y(y), z(z) { }
	vec3(float v) : x(v), y(v), z(v) { }

	float x, y, z;
};

class mat4
{
public:
	float m[16];

	static mat4 null()
	{
		mat4 v;
		memset(v.m, 0, sizeof(float) * 16);
	}

	static mat4 identity()
	{
		mat4 v = null();
		v.m[0] = 1.0f;
		v.m[5] = 1.0f;
		v.m[10] = 1.0f;
		v.m[15] = 1.0f;
		return v;
	}
};

class AudioSource;
class AudioSound;

class AudioMixer
{
public:
	static std::unique_ptr<AudioMixer> Create();

	virtual ~AudioMixer() = default;
	virtual AudioSound* AddSound(std::unique_ptr<AudioSource> source) = 0;
	virtual float GetSoundDuration(AudioSound* sound) = 0;
	virtual void PlaySound(void* owner, int slot, AudioSound* sound, const vec3& location, float volume, float radius, float pitch) = 0;
	virtual void PlayMusic(std::unique_ptr<AudioSource> source) = 0;
	virtual void SetViewport(const mat4& worldToView) = 0;
	virtual void Update() = 0;
};
