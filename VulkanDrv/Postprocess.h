#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include "PixelBuffer.h"
#include "vec.h"

enum class UniformType
{
	Int,
	UInt,
	Float,
	Vec2,
	Vec3,
	Vec4,
	IVec2,
	IVec3,
	IVec4,
	UVec2,
	UVec3,
	UVec4,
	Mat4
};

class UniformFieldDesc
{
public:
	UniformFieldDesc() { }
	UniformFieldDesc(const char *name, UniformType type, std::size_t offset) : name(name), type(type), offset(offset) { }

	const char *name;
	UniformType type;
	std::size_t offset;
};

enum class PPBlendMode
{
	none,
	additive,
	alpha
};

struct PPViewport
{
	int x, y, width, height;
};

class PPTexture;
class PPShader;
class Postprocess;

enum class PPFilterMode { Nearest, Linear };
enum class PPWrapMode { Clamp, Repeat };
enum class PPTextureType { CurrentPipelineTexture, NextPipelineTexture, PPTexture, SwapChain };

class PPTextureInput
{
public:
	PPFilterMode filter = PPFilterMode::Nearest;
	PPWrapMode wrap = PPWrapMode::Clamp;
	PPTextureType type = PPTextureType::CurrentPipelineTexture;
	PPTexture *texture = nullptr;
};

class PPOutput
{
public:
	PPTextureType type = PPTextureType::NextPipelineTexture;
	PPTexture *texture = nullptr;
};

class PPUniforms
{
public:
	PPUniforms()
	{
	}

	PPUniforms(const PPUniforms &src)
	{
		data = src.data;
	}

	~PPUniforms()
	{
		clear();
	}

	PPUniforms &operator=(const PPUniforms &src)
	{
		data = src.data;
		return *this;
	}

	void clear()
	{
		data.clear();
	}

	template<typename T>
	void set(const T &v)
	{
		if (data.size() != (int)sizeof(T))
		{
			data.resize(sizeof(T));
			memcpy(data.data(), &v, data.size());
		}
	}

	std::vector<uint8_t> data;
};

class PPRenderState
{
public:
	virtual ~PPRenderState() = default;

	virtual void draw() = 0;

	void clear()
	{
		shader = nullptr;
		textures = std::vector<PPTextureInput>();
		uniforms = PPUniforms();
		viewport = PPViewport();
		blendMode = PPBlendMode();
		output = PPOutput();
	}

	void setInputTexture(int index, PPTexture *texture, PPFilterMode filter = PPFilterMode::Nearest, PPWrapMode wrap = PPWrapMode::Clamp)
	{
		if ((int)textures.size() < index + 1)
			textures.resize(index + 1);
		auto &tex = textures[index];
		tex.filter = filter;
		tex.wrap = wrap;
		tex.type = PPTextureType::PPTexture;
		tex.texture = texture;
	}

	void setInputCurrent(int index, PPFilterMode filter = PPFilterMode::Nearest, PPWrapMode wrap = PPWrapMode::Clamp)
	{
		setInputSpecialType(index, PPTextureType::CurrentPipelineTexture, filter, wrap);
	}

	void setInputSpecialType(int index, PPTextureType type, PPFilterMode filter = PPFilterMode::Nearest, PPWrapMode wrap = PPWrapMode::Clamp)
	{
		if ((int)textures.size() < index + 1)
			textures.resize(index + 1);
		auto &tex = textures[index];
		tex.filter = filter;
		tex.wrap = wrap;
		tex.type = type;
		tex.texture = nullptr;
	}

	void setOutputTexture(PPTexture *texture)
	{
		output.type = PPTextureType::PPTexture;
		output.texture = texture;
	}

	void setOutputCurrent()
	{
		output.type = PPTextureType::CurrentPipelineTexture;
		output.texture = nullptr;
	}

	void setOutputNext()
	{
		output.type = PPTextureType::NextPipelineTexture;
		output.texture = nullptr;
	}

	void setOutputSwapChain()
	{
		output.type = PPTextureType::SwapChain;
		output.texture = nullptr;
	}

	void setNoBlend()
	{
		blendMode = PPBlendMode::none;
	}

	void setAdditiveBlend()
	{
		blendMode = PPBlendMode::additive;
	}

	void setAlphaBlend()
	{
		blendMode = PPBlendMode::alpha;
	}

	Postprocess* model = nullptr;
	PPShader *shader;
	std::vector<PPTextureInput> textures;
	PPUniforms uniforms;
	PPViewport viewport;
	PPBlendMode blendMode;
	PPOutput output;
};

class PPResource
{
public:
	PPResource()
	{
		next = first;
		first = this;
		if (next) next->prev = this;
	}

	PPResource(const PPResource &)
	{
		next = first;
		first = this;
		if (next) next->prev = this;
	}

	virtual ~PPResource()
	{
		if (next) next->prev = prev;
		if (prev) prev->next = next;
		else first = next;
	}

	PPResource &operator=(const PPResource &other)
	{
		return *this;
	}

	static void resetAll()
	{
		for (PPResource *cur = first; cur; cur = cur->next)
			cur->resetBackend();
	}

	virtual void resetBackend() = 0;

private:
	static PPResource *first;
	PPResource *prev = nullptr;
	PPResource *next = nullptr;
};

class PPTextureBackend
{
public:
	virtual ~PPTextureBackend() = default;
};

class PPTexture : public PPResource
{
public:
	PPTexture() = default;
	PPTexture(int width, int height, PixelFormat format, std::shared_ptr<void> data = {}) : width(width), height(height), format(format), data(data) { }

	void resetBackend() override { backend.reset(); }

	int width;
	int height;
	PixelFormat format;
	std::shared_ptr<void> data;

	std::unique_ptr<PPTextureBackend> backend;
};

class PPShaderBackend
{
public:
	virtual ~PPShaderBackend() = default;
};

class PPShader : public PPResource
{
public:
	PPShader() = default;
	PPShader(const std::string &fragment, const std::string &defines, const std::vector<UniformFieldDesc> &uniforms, int version = 330) : fragmentShader(fragment), defines(defines), uniforms(uniforms), version(version) { }

	void resetBackend() override { backend.reset(); }

	std::string fragmentShader;
	std::string defines;
	std::vector<UniformFieldDesc> uniforms;
	int version = 330;

	std::unique_ptr<PPShaderBackend> backend;
};

/////////////////////////////////////////////////////////////////////////////

struct ExtractUniforms
{
	vec2 scale;
	vec2 offset;

	static std::vector<UniformFieldDesc> desc()
	{
		return
		{
			{ "Scale", UniformType::Vec2, offsetof(ExtractUniforms, scale) },
			{ "Offset", UniformType::Vec2, offsetof(ExtractUniforms, offset) }
		};
	}
};

struct BlurUniforms
{
	float sampleWeights[8];

	static std::vector<UniformFieldDesc> desc()
	{
		return
		{
			{ "SampleWeights0", UniformType::Float, offsetof(BlurUniforms, sampleWeights[0]) },
			{ "SampleWeights1", UniformType::Float, offsetof(BlurUniforms, sampleWeights[1]) },
			{ "SampleWeights2", UniformType::Float, offsetof(BlurUniforms, sampleWeights[2]) },
			{ "SampleWeights3", UniformType::Float, offsetof(BlurUniforms, sampleWeights[3]) },
			{ "SampleWeights4", UniformType::Float, offsetof(BlurUniforms, sampleWeights[4]) },
			{ "SampleWeights5", UniformType::Float, offsetof(BlurUniforms, sampleWeights[5]) },
			{ "SampleWeights6", UniformType::Float, offsetof(BlurUniforms, sampleWeights[6]) },
			{ "SampleWeights7", UniformType::Float, offsetof(BlurUniforms, sampleWeights[7]) },
		};
	}
};

enum { NumBloomLevels = 4 };

class PPBlurLevel
{
public:
	PPViewport viewport;
	PPTexture vTexture;
	PPTexture hTexture;
};

class PPBloom
{
public:
	void renderBloom(PPRenderState *renderstate, int sceneWidth, int sceneHeight);
	void renderBlur(PPRenderState *renderstate, int sceneWidth, int sceneHeight);

private:
	void blurStep(PPRenderState *renderstate, const BlurUniforms &blurUniforms, PPTexture &input, PPTexture &output, PPViewport viewport, bool vertical);
	void updateTextures(int width, int height);

	static float computeBlurGaussian(float n, float theta);
	static void computeBlurSamples(int sampleCount, float blurAmount, float *sampleWeights);

	PPBlurLevel levels[NumBloomLevels];
	int lastWidth = 0;
	int lastHeight = 0;

	PPShader bloomCombine = { "shaders/BloomCombine.frag", "", {} };
	PPShader bloomExtract = { "shaders/BloomExtract.frag", "", ExtractUniforms::desc() };
	PPShader blurVertical = { "shaders/Blur.frag", "#define BLUR_VERTICAL\n", BlurUniforms::desc() };
	PPShader blurHorizontal = { "shaders/Blur.frag", "#define BLUR_HORIZONTAL\n", BlurUniforms::desc() };
};

/////////////////////////////////////////////////////////////////////////////

struct ExposureExtractUniforms
{
	vec2 scale;
	vec2 offset;

	static std::vector<UniformFieldDesc> desc()
	{
		return
		{
			{ "Scale", UniformType::Vec2, offsetof(ExposureExtractUniforms, scale) },
			{ "Offset", UniformType::Vec2, offsetof(ExposureExtractUniforms, offset) }
		};
	}
};

struct ExposureCombineUniforms
{
	float exposureBase;
	float exposureMin;
	float exposureScale;
	float exposureSpeed;

	static std::vector<UniformFieldDesc> desc()
	{
		return
		{
			{ "ExposureBase", UniformType::Float, offsetof(ExposureCombineUniforms, exposureBase) },
			{ "ExposureMin", UniformType::Float, offsetof(ExposureCombineUniforms, exposureMin) },
			{ "ExposureScale", UniformType::Float, offsetof(ExposureCombineUniforms, exposureScale) },
			{ "ExposureSpeed", UniformType::Float, offsetof(ExposureCombineUniforms, exposureSpeed) }
		};
	}
};

class PPExposureLevel
{
public:
	PPViewport viewport;
	PPTexture texture;
};

class PPCameraExposure
{
public:
	void render(PPRenderState *renderstate, int sceneWidth, int sceneHeight);

	PPTexture cameraTexture = { 1, 1, PixelFormat::r32f };

private:
	void updateTextures(int width, int height);

	std::vector<PPExposureLevel> exposureLevels;
	bool firstExposureFrame = true;

	PPShader exposureExtract = { "shaders/ExposureExtract.frag", "", ExposureExtractUniforms::desc() };
	PPShader exposureAverage = { "shaders/ExposureAverage.frag", "", {}, 400 };
	PPShader exposureCombine = { "shaders/ExposureCombine.frag", "", ExposureCombineUniforms::desc() };
};

class PPTonemap
{
public:
	void render(PPRenderState *renderstate, int sceneWidth, int sceneHeight);

	PPShader tonemap = { "shaders/Tonemap.frag", "", {} };
};

struct PresentUniforms
{
	float invGamma;
	float padding1;
	float padding2;
	float padding3;

	static std::vector<UniformFieldDesc> desc()
	{
		return
		{
			{ "InvGamma", UniformType::Float, offsetof(PresentUniforms, invGamma) }
		};
	}
};

class PPPresent
{
public:
	PPPresent();

	PPTexture dither;

	PPShader present = { "shaders/Present.frag", "", PresentUniforms::desc() };
};

/////////////////////////////////////////////////////////////////////////////

class Postprocess
{
public:
	PPBloom bloom;
	PPCameraExposure exposure;
	PPTonemap tonemap;
	PPPresent present;
};
