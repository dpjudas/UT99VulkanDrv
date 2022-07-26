
#include "Precomp.h"
#include "ShaderManager.h"
#include "FileResource.h"
#include "VulkanBuilders.h"
#include "UVulkanRenderDevice.h"

ShaderManager::ShaderManager(UVulkanRenderDevice* renderer) : renderer(renderer)
{
	ShaderBuilder::init();

	vertexShader = ShaderManager::CreateVertexShader(renderer->Device, "shaders/Scene.vert");
	fragmentShader = ShaderManager::CreateFragmentShader(renderer->Device, "shaders/Scene.frag");
	fragmentShaderAlphaTest = ShaderManager::CreateFragmentShader(renderer->Device, "shaders/Scene.frag", "#define ALPHATEST");
}

ShaderManager::~ShaderManager()
{
	ShaderBuilder::deinit();
}

std::unique_ptr<VulkanShader> ShaderManager::CreateVertexShader(VulkanDevice* device, const std::string& name, const std::string& defines)
{
	ShaderBuilder builder;
	builder.setVertexShader(LoadShaderCode(name, defines));
	return builder.create(device);
}

std::unique_ptr<VulkanShader> ShaderManager::CreateFragmentShader(VulkanDevice* device, const std::string& name, const std::string& defines)
{
	ShaderBuilder builder;
	builder.setFragmentShader(LoadShaderCode(name, defines));
	return builder.create(device);
}

std::unique_ptr<VulkanShader> ShaderManager::CreateComputeShader(VulkanDevice* device, const std::string& name, const std::string& defines)
{
	ShaderBuilder builder;
	builder.setComputeShader(LoadShaderCode(name, defines));
	return builder.create(device);
}

std::string ShaderManager::LoadShaderCode(const std::string& filename, const std::string& defines)
{
	const char* shaderversion = R"(
		#version 450
		#extension GL_ARB_separate_shader_objects : enable
	)";
	return shaderversion + defines + "\r\n#line 1\r\n" + FileResource::readAllText(filename);
}
