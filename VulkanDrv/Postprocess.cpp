
#include "Precomp.h"
#include "Postprocess.h"

#undef M_PI
#define M_PI 3.1415926535897932384626433832795f

static bool r_bloom = false;
static float r_bloom_amount = 1.4f;
static float r_exposure_scale = 1.3f;
static float r_exposure_min = 0.35f;
static float r_exposure_base = 0.35f;
static float r_exposure_speed = 0.05f;
static float r_menu_blur = 2.0f;

/*
CVarBool r_bloom("r_bloom", false);
CVarFloat r_bloom_amount("r_bloom_amount", 1.4f, [](float v)
{
	v = std::max(v, 0.1f);
	return v;
});

CVarFloat r_exposure_scale("r_exposure_scale", 1.3f);
CVarFloat r_exposure_min("r_exposure_min", 0.35f);
CVarFloat r_exposure_base("r_exposure_base", 0.35f);
CVarFloat r_exposure_speed("r_exposure_speed", 0.05f);
CVarFloat r_menu_blur("r_menu_blur", 2.0f);
*/

PPResource *PPResource::first = nullptr;

/////////////////////////////////////////////////////////////////////////////

void PPBloom::updateTextures(int width, int height)
{
	if (width == lastWidth && height == lastHeight)
		return;

	int bloomWidth = (width + 1) / 2;
	int bloomHeight = (height + 1) / 2;

	for (int i = 0; i < NumBloomLevels; i++)
	{
		auto &blevel = levels[i];
		blevel.viewport.x = 0;
		blevel.viewport.y = 0;
		blevel.viewport.width = (bloomWidth + 1) / 2;
		blevel.viewport.height = (bloomHeight + 1) / 2;
		blevel.vTexture = { blevel.viewport.width, blevel.viewport.height, PixelFormat::rgba16f };
		blevel.hTexture = { blevel.viewport.width, blevel.viewport.height, PixelFormat::rgba16f };

		bloomWidth = blevel.viewport.width;
		bloomHeight = blevel.viewport.height;
	}

	lastWidth = width;
	lastHeight = height;
}

void PPBloom::renderBloom(PPRenderState *renderstate, int sceneWidth, int sceneHeight)
{
	if (!r_bloom || sceneWidth <= 0 || sceneHeight <= 0)
	{
		return;
	}

	updateTextures(sceneWidth, sceneHeight);

	ExtractUniforms extractUniforms;
	extractUniforms.scale = { 1.0f };
	extractUniforms.offset = { 0.0f };

	auto &level0 = levels[0];

	// Extract blooming pixels from scene texture:
	renderstate->clear();
	renderstate->shader = &bloomExtract;
	renderstate->uniforms.set(extractUniforms);
	renderstate->viewport = level0.viewport;
	renderstate->setInputCurrent(0, PPFilterMode::Linear);
	renderstate->setInputTexture(1, &renderstate->model->exposure.cameraTexture);
	renderstate->setOutputTexture(&level0.vTexture);
	renderstate->setNoBlend();
	renderstate->draw();

	const float blurAmount = r_bloom_amount;
	BlurUniforms blurUniforms;
	computeBlurSamples(7, blurAmount, blurUniforms.sampleWeights);

	// Blur and downscale:
	for (int i = 0; i < NumBloomLevels - 1; i++)
	{
		auto &blevel = levels[i];
		auto &next = levels[i + 1];

		blurStep(renderstate, blurUniforms, blevel.vTexture, blevel.hTexture, blevel.viewport, false);
		blurStep(renderstate, blurUniforms, blevel.hTexture, blevel.vTexture, blevel.viewport, true);

		// Linear downscale:
		renderstate->clear();
		renderstate->shader = &bloomCombine;
		renderstate->uniforms.clear();
		renderstate->viewport = next.viewport;
		renderstate->setInputTexture(0, &blevel.vTexture, PPFilterMode::Linear);
		renderstate->setOutputTexture(&next.vTexture);
		renderstate->setNoBlend();
		renderstate->draw();
	}

	// Blur and upscale:
	for (int i = NumBloomLevels - 1; i > 0; i--)
	{
		auto &blevel = levels[i];
		auto &next = levels[i - 1];

		blurStep(renderstate, blurUniforms, blevel.vTexture, blevel.hTexture, blevel.viewport, false);
		blurStep(renderstate, blurUniforms, blevel.hTexture, blevel.vTexture, blevel.viewport, true);

		// Linear upscale:
		renderstate->clear();
		renderstate->shader = &bloomCombine;
		renderstate->uniforms.clear();
		renderstate->viewport = next.viewport;
		renderstate->setInputTexture(0, &blevel.vTexture, PPFilterMode::Linear);
		renderstate->setOutputTexture(&next.vTexture);
		renderstate->setNoBlend();
		renderstate->draw();
	}

	blurStep(renderstate, blurUniforms, level0.vTexture, level0.hTexture, level0.viewport, false);
	blurStep(renderstate, blurUniforms, level0.hTexture, level0.vTexture, level0.viewport, true);

	// Add bloom back to scene texture:
	renderstate->clear();
	renderstate->shader = &bloomCombine;
	renderstate->uniforms.clear();
	renderstate->viewport.x = 0;
	renderstate->viewport.y = 0;
	renderstate->viewport.width = sceneWidth;
	renderstate->viewport.height = sceneHeight;
	renderstate->setInputTexture(0, &level0.vTexture, PPFilterMode::Linear);
	renderstate->setOutputCurrent();
	renderstate->setAdditiveBlend();
	renderstate->draw();
}

void PPBloom::renderBlur(PPRenderState *renderstate, int sceneWidth, int sceneHeight)
{
	// No scene, no blur!
	if (sceneWidth <= 0 || sceneHeight <= 0)
		return;

	updateTextures(sceneWidth, sceneHeight);

	float blurAmount = r_menu_blur;
	if (blurAmount <= 0.0f)
		return;

	const int numLevels = 3;
	static_assert(numLevels <= NumBloomLevels, "numLevels is bigger than NumBloomLevels");

	auto &level0 = levels[0];

	// Grab the area we want to bloom:
	renderstate->clear();
	renderstate->shader = &bloomCombine;
	renderstate->uniforms.clear();
	renderstate->viewport = level0.viewport;
	renderstate->setInputCurrent(0, PPFilterMode::Linear);
	renderstate->setOutputTexture(&level0.vTexture);
	renderstate->setNoBlend();
	renderstate->draw();

	BlurUniforms blurUniforms;
	computeBlurSamples(7, blurAmount, blurUniforms.sampleWeights);

	// Blur and downscale:
	for (int i = 0; i < numLevels - 1; i++)
	{
		auto &blevel = levels[i];
		auto &next = levels[i + 1];

		blurStep(renderstate, blurUniforms, blevel.vTexture, blevel.hTexture, blevel.viewport, false);
		blurStep(renderstate, blurUniforms, blevel.hTexture, blevel.vTexture, blevel.viewport, true);

		// Linear downscale:
		renderstate->clear();
		renderstate->shader = &bloomCombine;
		renderstate->uniforms.clear();
		renderstate->viewport = next.viewport;
		renderstate->setInputTexture(0, &blevel.vTexture, PPFilterMode::Linear);
		renderstate->setOutputTexture(&next.vTexture);
		renderstate->setNoBlend();
		renderstate->draw();
	}

	// Blur and upscale:
	for (int i = numLevels - 1; i > 0; i--)
	{
		auto &blevel = levels[i];
		auto &next = levels[i - 1];

		blurStep(renderstate, blurUniforms, blevel.vTexture, blevel.hTexture, blevel.viewport, false);
		blurStep(renderstate, blurUniforms, blevel.hTexture, blevel.vTexture, blevel.viewport, true);

		// Linear upscale:
		renderstate->clear();
		renderstate->shader = &bloomCombine;
		renderstate->uniforms.clear();
		renderstate->viewport = next.viewport;
		renderstate->setInputTexture(0, &blevel.vTexture, PPFilterMode::Linear);
		renderstate->setOutputTexture(&next.vTexture);
		renderstate->setNoBlend();
		renderstate->draw();
	}

	blurStep(renderstate, blurUniforms, level0.vTexture, level0.hTexture, level0.viewport, false);
	blurStep(renderstate, blurUniforms, level0.hTexture, level0.vTexture, level0.viewport, true);

	// Copy blur back to scene texture:
	renderstate->clear();
	renderstate->shader = &bloomCombine;
	renderstate->uniforms.clear();
	renderstate->viewport.x = 0;
	renderstate->viewport.y = 0;
	renderstate->viewport.width = sceneWidth;
	renderstate->viewport.height = sceneHeight;
	renderstate->setInputTexture(0, &level0.vTexture, PPFilterMode::Linear);
	renderstate->setOutputCurrent();
	renderstate->setNoBlend();
	renderstate->draw();
}

void PPBloom::blurStep(PPRenderState *renderstate, const BlurUniforms &blurUniforms, PPTexture &input, PPTexture &output, PPViewport viewport, bool vertical)
{
	renderstate->clear();
	renderstate->shader = vertical ? &blurVertical : &blurHorizontal;
	renderstate->uniforms.set(blurUniforms);
	renderstate->viewport = viewport;
	renderstate->setInputTexture(0, &input);
	renderstate->setOutputTexture(&output);
	renderstate->setNoBlend();
	renderstate->draw();
}

float PPBloom::computeBlurGaussian(float n, float theta) // theta = Blur Amount
{
	return (float)((1.0f / sqrtf(2 * (float)M_PI * theta)) * expf(-(n * n) / (2.0f * theta * theta)));
}

void PPBloom::computeBlurSamples(int sampleCount, float blurAmount, float *sampleWeights)
{
	sampleWeights[0] = computeBlurGaussian(0, blurAmount);

	float totalWeights = sampleWeights[0];

	for (int i = 0; i < sampleCount / 2; i++)
	{
		float weight = computeBlurGaussian(i + 1.0f, blurAmount);

		sampleWeights[i * 2 + 1] = weight;
		sampleWeights[i * 2 + 2] = weight;

		totalWeights += weight * 2;
	}

	for (int i = 0; i < sampleCount; i++)
	{
		sampleWeights[i] /= totalWeights;
	}
}

/////////////////////////////////////////////////////////////////////////////

void PPCameraExposure::render(PPRenderState *renderstate, int sceneWidth, int sceneHeight)
{
	if (!r_bloom)
	{
		return;
	}

	updateTextures(sceneWidth, sceneHeight);

	ExposureExtractUniforms extractUniforms;
	extractUniforms.scale = { 1.0f };
	extractUniforms.offset = { 0.0f };

	ExposureCombineUniforms combineUniforms;
	combineUniforms.exposureBase = r_exposure_base;
	combineUniforms.exposureMin = r_exposure_min;
	combineUniforms.exposureScale = r_exposure_scale;
	combineUniforms.exposureSpeed = r_exposure_speed;

	auto &level0 = exposureLevels[0];

	// Extract light blevel from scene texture:
	renderstate->clear();
	renderstate->shader = &exposureExtract;
	renderstate->uniforms.set(extractUniforms);
	renderstate->viewport = level0.viewport;
	renderstate->setInputCurrent(0, PPFilterMode::Linear);
	renderstate->setOutputTexture(&level0.texture);
	renderstate->setNoBlend();
	renderstate->draw();

	// Find the average value:
	for (size_t i = 0; i + 1 < exposureLevels.size(); i++)
	{
		auto &blevel = exposureLevels[i];
		auto &next = exposureLevels[i + 1];

		renderstate->shader = &exposureAverage;
		renderstate->uniforms.clear();
		renderstate->viewport = next.viewport;
		renderstate->setInputTexture(0, &blevel.texture, PPFilterMode::Linear);
		renderstate->setOutputTexture(&next.texture);
		renderstate->setNoBlend();
		renderstate->draw();
	}

	// Combine average value with current camera exposure:
	renderstate->shader = &exposureCombine;
	renderstate->uniforms.set(combineUniforms);
	renderstate->viewport.x = 0;
	renderstate->viewport.y = 0;
	renderstate->viewport.width = 1;
	renderstate->viewport.height = 1;
	renderstate->setInputTexture(0, &exposureLevels.back().texture, PPFilterMode::Linear);
	renderstate->setOutputTexture(&cameraTexture);
	if (!firstExposureFrame)
		renderstate->setAlphaBlend();
	else
		renderstate->setNoBlend();
	renderstate->draw();

	firstExposureFrame = false;
}

void PPCameraExposure::updateTextures(int width, int height)
{
	int firstwidth = std::max(width / 2, 1);
	int firstheight = std::max(height / 2, 1);

	if (exposureLevels.size() > 0 && exposureLevels[0].viewport.width == firstwidth && exposureLevels[0].viewport.height == firstheight)
	{
		return;
	}

	exposureLevels.clear();

	int i = 0;
	do
	{
		width = std::max(width / 2, 1);
		height = std::max(height / 2, 1);

		PPExposureLevel blevel;
		blevel.viewport.x = 0;
		blevel.viewport.y = 0;
		blevel.viewport.width = width;
		blevel.viewport.height = height;
		blevel.texture = { blevel.viewport.width, blevel.viewport.height, PixelFormat::r32f };
		exposureLevels.push_back(std::move(blevel));

		i++;

	} while (width > 1 || height > 1);

	firstExposureFrame = true;
}

/////////////////////////////////////////////////////////////////////////////

void PPTonemap::render(PPRenderState *renderstate, int sceneWidth, int sceneHeight)
{
	renderstate->clear();
	renderstate->shader = &tonemap;
	renderstate->viewport.x = 0;
	renderstate->viewport.y = 0;
	renderstate->viewport.width = sceneWidth;
	renderstate->viewport.height = sceneHeight;
	renderstate->setInputCurrent(0);
	renderstate->setOutputNext();
	renderstate->setNoBlend();
	renderstate->draw();
}

/////////////////////////////////////////////////////////////////////////////

PPPresent::PPPresent()
{
	static const float data[64] =
	{
			.0078125, .2578125, .1328125, .3828125, .0234375, .2734375, .1484375, .3984375,
			.7578125, .5078125, .8828125, .6328125, .7734375, .5234375, .8984375, .6484375,
			.0703125, .3203125, .1953125, .4453125, .0859375, .3359375, .2109375, .4609375,
			.8203125, .5703125, .9453125, .6953125, .8359375, .5859375, .9609375, .7109375,
			.0390625, .2890625, .1640625, .4140625, .0546875, .3046875, .1796875, .4296875,
			.7890625, .5390625, .9140625, .6640625, .8046875, .5546875, .9296875, .6796875,
			.1015625, .3515625, .2265625, .4765625, .1171875, .3671875, .2421875, .4921875,
			.8515625, .6015625, .9765625, .7265625, .8671875, .6171875, .9921875, .7421875,
	};

	std::shared_ptr<void> pixels(new float[64], [](void *p) { delete[](float*)p; });
	memcpy(pixels.get(), data, 64 * sizeof(float));
	dither = { 8, 8, PixelFormat::r32f, pixels };
}
