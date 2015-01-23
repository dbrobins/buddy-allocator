/*
   Memory allocator.

   D. Robins, 20141010
*/

#pragma once

#include <cstdint>
#include <memory>
#ifdef UNITTEST
#include <iostream>
#endif

#define CEIL_DIV(A, B) ((((A) - 1) / (B)) + 1)


class AllocatorBase
{
	struct Deleter
	{
		Deleter() : m_palloc(nullptr) {}
		Deleter(AllocatorBase &alloc) : m_palloc(&alloc) {}
		void operator()(void const *pv) { m_palloc->FreePv(pv); }

	private:
		AllocatorBase *m_palloc;
	};

public:
	template <typename T>
	using P = std::unique_ptr<T, Deleter>;

	template <typename T>
	P<T> PtAlloc();
	template <typename T>
	P<T []> PtAlloc(size_t c);

	template <typename T>
	P<T> PtAttach(T *pt) { return P<T>(pt, Deleter(*this)); }
	template <typename T>
	P<T []> PtAttachRg(T *pt) { return P<T[]>(pt, Deleter(*this)); }

private:
	virtual void *PvAlloc(size_t cb) = 0;
	virtual void FreePv(void const *pv) = 0;
};


// cb2 is a size in bytes as a power of 2 (e.g., 5 => 2^5 => 32)
template <uint8_t Tcb2Min, uint8_t Tcb2Heap>
class Allocator final : public AllocatorBase
{
public:
	static_assert(Tcb2Min < Tcb2Heap, "minimum block must be smaller than heap size");
	static constexpr size_t CbMin() { return 1 << Tcb2Min; }
	static constexpr size_t CbHeap() { return 1 << Tcb2Heap; }
	
	Allocator();

#ifdef UNITTEST
	const void *PvStart(const void *pv) const;
	void *PvStart(void *pv) const
		{ return const_cast<void *>(PvStart(const_cast<const void *>(pv))); }

	const uint8_t *PbHeap() const { return m_rgbHeap; }
	size_t CbUsed() const;
	void PrintInfo(std::ostream &os) const;
	void PrintState(std::ostream &os) const;
#endif

private:
	uint8_t m_rgbHeap[1 << Tcb2Heap];
	uint8_t m_rgbTrack[CEIL_DIV(1 << (Tcb2Heap - Tcb2Min + 1), 8)];

#ifdef UNITTEST
public:  // allow these functions for testing only
#endif
	void *PvAlloc(size_t cb) final;
	void FreePv(void const *pv) final;
};

#include "Alloc.inl"

