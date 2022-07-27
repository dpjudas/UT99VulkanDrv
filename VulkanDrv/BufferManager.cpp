
#include "Precomp.h"
#include "BufferManager.h"
#include "UVulkanRenderDevice.h"
#include "VulkanBuilders.h"

BufferManager::BufferManager(UVulkanRenderDevice* renderer) : renderer(renderer)
{
	CreateSceneVertexBuffer();
	CreateSceneIndexBuffer();
}

BufferManager::~BufferManager()
{
	if (SceneVertices) { SceneVertexBuffer->Unmap(); SceneVertices = nullptr; }
	if (SceneIndexes) { SceneIndexBuffer->Unmap(); SceneIndexes = nullptr; }
}

void BufferManager::CreateSceneVertexBuffer()
{
	size_t size = sizeof(SceneVertex) * 1'000'000;

	SceneVertexBuffer = BufferBuilder()
		.Usage(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VMA_MEMORY_USAGE_UNKNOWN, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
		.MemoryType(
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		.Size(size)
		.DebugName("SceneVertexBuffer")
		.Create(renderer->Device);

	SceneVertices = (SceneVertex*)SceneVertexBuffer->Map(0, size);
}

void BufferManager::CreateSceneIndexBuffer()
{
	size_t size = sizeof(uint32_t) * 1'000'000;

	SceneIndexBuffer = BufferBuilder()
		.Usage(
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VMA_MEMORY_USAGE_UNKNOWN, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT)
		.MemoryType(
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		.Size(size)
		.DebugName("SceneIndexBuffer")
		.Create(renderer->Device);

	SceneIndexes = (uint32_t*)SceneIndexBuffer->Map(0, size);
}
