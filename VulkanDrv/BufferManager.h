#pragma once

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
	std::unique_ptr<VulkanBuffer> UploadBuffer;

	SceneVertex* SceneVertices = nullptr;
	uint32_t* SceneIndexes = nullptr;
	uint8_t* UploadData = nullptr;

	static const int SceneVertexBufferSize = 1 * 1024 * 1024;
	static const int SceneIndexBufferSize = 1 * 1024 * 1024;

	static const int UploadBufferSize = 64 * 1024 * 1024;

private:
	void CreateSceneVertexBuffer();
	void CreateSceneIndexBuffer();
	void CreateUploadBuffer();

	UVulkanRenderDevice* renderer = nullptr;
};
