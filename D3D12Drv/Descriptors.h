#pragma once

class DescriptorSet;

class DescriptorHeap
{
public:
	DescriptorHeap(ID3D12Device* device, int maxDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags);

	DescriptorSet Alloc(int count);

	int GetUsedCount() const;
	int GetHeapSize() const { return HeapSize; }

	ID3D12DescriptorHeap* GetHeap() { return Heap; }

private:
	ComPtr<ID3D12DescriptorHeap> Heap;
	UINT HandleSize = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE CPUStart = {};
	D3D12_GPU_DESCRIPTOR_HANDLE GPUStart = {};
	int HeapSize = 0;
	int Next = 0;
	std::vector<std::vector<int>> FreeLists;

	friend class DescriptorSet;
};

class DescriptorSet
{
public:
	D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle(int index = 0) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle(int index = 0) const;

	void reset();

	operator bool() const { return Count != 0; }

private:
	DescriptorHeap* Allocator = nullptr;
	int BaseIndex = 0;
	int Count = 0;

	friend class DescriptorHeap;
};

inline DescriptorHeap::DescriptorHeap(ID3D12Device* device, int maxDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = maxDescriptors;
	heapDesc.Type = type;
	heapDesc.Flags = flags;
	HRESULT result = device->CreateDescriptorHeap(&heapDesc, Heap.GetIID(), Heap.InitPtr());
	if (FAILED(result))
		throw std::runtime_error("CreateDescriptorHeap failed");

	HeapSize = maxDescriptors;
	HandleSize = device->GetDescriptorHandleIncrementSize(type);
}

inline int DescriptorHeap::GetUsedCount() const
{
	int used = Next;
	int i = 0;
	for (const auto& list : FreeLists)
	{
		used -= i * list.size();
		i++;
	}
	return used;
}

inline DescriptorSet DescriptorHeap::Alloc(int count)
{
	if (FreeLists.size() > (size_t)count && !FreeLists[count].empty())
	{
		int index = FreeLists[count].back();
		FreeLists[count].pop_back();

		DescriptorSet set;
		set.Allocator = this;
		set.BaseIndex = index;
		set.Count = count;
		return set;
	}
	if (Next + count > HeapSize)
		throw std::runtime_error("Heap allocator out of descriptors!");
	int index = Next;
	Next += count;

	DescriptorSet set;
	set.Allocator = this;
	set.BaseIndex = index;
	set.Count = count;
	return set;
}

inline void DescriptorSet::reset()
{
	if (!Allocator)
		return;

	if (Allocator->FreeLists.size() <= (size_t)Count)
		Allocator->FreeLists.resize(Count + 1);
	Allocator->FreeLists[Count].push_back(BaseIndex);

	Allocator = nullptr;
	BaseIndex = 0;
	Count = 0;
}

inline D3D12_CPU_DESCRIPTOR_HANDLE DescriptorSet::CPUHandle(int index) const
{
	if (Allocator->CPUStart.ptr == 0)
	{
		Allocator->CPUStart = Allocator->Heap->GetCPUDescriptorHandleForHeapStart();
	}

	D3D12_CPU_DESCRIPTOR_HANDLE handle = Allocator->CPUStart;
	handle.ptr += (BaseIndex + index) * (SIZE_T)Allocator->HandleSize;
	return handle;
}

inline D3D12_GPU_DESCRIPTOR_HANDLE DescriptorSet::GPUHandle(int index) const
{
	if (Allocator->GPUStart.ptr == 0)
	{
		Allocator->GPUStart = Allocator->Heap->GetGPUDescriptorHandleForHeapStart();
	}

	D3D12_GPU_DESCRIPTOR_HANDLE handle = Allocator->GPUStart;
	handle.ptr += (BaseIndex + index) * (UINT64)Allocator->HandleSize;
	return handle;
}
