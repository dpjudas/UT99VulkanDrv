
#include "Precomp.h"
#include "ShaderManager.h"
#include "FileResource.h"
#include "VulkanBuilders.h"
#include "UVulkanRenderDevice.h"

ShaderManager::ShaderManager(UVulkanRenderDevice* renderer) : renderer(renderer)
{
	ShaderBuilder::Init();

	vertexShader = ShaderBuilder()
		.VertexShader(LoadShaderCode("shaders/Scene.vert"))
		.DebugName("vertexShader")
		.Create("vertexShader", renderer->Device);

	fragmentShader = ShaderBuilder()
		.FragmentShader(LoadShaderCode("shaders/Scene.frag"))
		.DebugName("fragmentShader")
		.Create("fragmentShader", renderer->Device);

	fragmentShaderAlphaTest = ShaderBuilder()
		.FragmentShader(LoadShaderCode("shaders/Scene.frag", "#define ALPHATEST"))
		.DebugName("fragmentShader")
		.Create("fragmentShader", renderer->Device);

	ppVertexShader = ShaderBuilder()
		.VertexShader(LoadShaderCode("shaders/PPStep.vert"))
		.DebugName("ppVertexShader")
		.Create("ppVertexShader", renderer->Device);

	ppFragmentPresentShader = ShaderBuilder()
		.FragmentShader(LoadShaderCode("shaders/Present.frag"))
		.DebugName("ppFragmentPresentShader")
		.Create("ppFragmentPresentShader", renderer->Device);
}

ShaderManager::~ShaderManager()
{
	ShaderBuilder::Deinit();
}

std::string ShaderManager::LoadShaderCode(const std::string& filename, const std::string& defines)
{
	const char* shaderversion = R"(
		#version 450
		#extension GL_ARB_separate_shader_objects : enable
	)";
	return shaderversion + defines + "\r\n#line 1\r\n" + FileResource::readAllText(filename);
}
