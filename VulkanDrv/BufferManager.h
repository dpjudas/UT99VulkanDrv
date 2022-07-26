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

	std::unique_ptr<VulkanBuffer> SceneVertexBuffer;
	std::unique_ptr<VulkanBuffer> SceneIndexBuffer;

	SceneVertex* SceneVertices = nullptr;
	uint32_t* SceneIndexes = nullptr;

private:
	void CreateSceneVertexBuffer();
	void CreateSceneIndexBuffer();

	UVulkanRenderDevice* renderer = nullptr;
};
