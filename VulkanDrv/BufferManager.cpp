
#include "Precomp.h"
#include "BufferManager.h"
#include "UVulkanRenderDevice.h"
#include "VulkanBuilders.h"

BufferManager::BufferManager(UVulkanRenderDevice* renderer) : renderer(renderer)
{
	CreateSceneVertexBuffer();
}

BufferManager::~BufferManager()
{
	if (SceneVertices) { SceneVertexBuffer->Unmap(); SceneVertices = nullptr; }
	delete SceneVertexBuffer; SceneVertexBuffer = nullptr; SceneVertices = nullptr;
}

void BufferManager::CreateSceneVertexBuffer()
{
	size_t size = sizeof(SceneVertex) * 1'000'000;

	BufferBuilder builder;
	builder.setUsage(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_UNKNOWN, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT);
	builder.setMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	builder.setSize(size);

	SceneVertexBuffer = builder.create(renderer->Device).release();
	SceneVertices = (SceneVertex*)SceneVertexBuffer->Map(0, size);
}
