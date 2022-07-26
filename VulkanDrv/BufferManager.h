#pragma once

#include "VulkanObjects.h"
#include "ShaderManager.h"

class UVulkanRenderDevice;
struct SceneVertex;

class BufferManager
{
public:
	BufferManager(UVulkanRenderDevice* renderer);
	~BufferManager();

	VulkanBuffer* SceneVertexBuffer = nullptr;
	SceneVertex* SceneVertices = nullptr;

private:
	void CreateSceneVertexBuffer();

	UVulkanRenderDevice* renderer = nullptr;
};
