/*
   Memory allocator.

   D. Robins, 20141010
*/

#include <cstring>
#include <algorithm>
#include <cassert>
#ifdef UNITTEST
#include <vector>
#endif


#define Assert(f) assert(f)
#define ShipAssert(f) assert(f)
#define NotReachedReturn(...) \
	do \
	{ \
		Assert(false); \
		return __VA_ARGS__; \
	} while(0)

static inline bool is_word_aligned(void *pv)
	{ return ((uintptr_t)pv & 0x03) == 0; }

class CriticalSectionState
{
};


// can't cross byte boundary
static inline void SetBits(uint8_t *pb, size_t ibit, uint8_t mask, uint8_t val)
{
	uint8_t ib = ibit >> 3;
	ibit = ibit % 8;
	pb[ib] = (pb[ib] & ~(mask << ibit)) | (val << ibit);
}

static inline uint8_t BitsGet(const uint8_t *pb, size_t ibit, uint8_t mask)
{
	uint8_t ib = ibit >> 3;
	ibit = ibit % 8;
	return (pb[ib] >> ibit) & mask;
}


// This is a buddy allocator, for increased determinism (due to controlled fragmentation) over a "first found" allocator.

template <uint8_t Tcb2Min, uint8_t Tcb2Heap>
Allocator<Tcb2Min, Tcb2Heap>::Allocator()
{
	ShipAssert(is_word_aligned(m_rgbHeap));
	memset(m_rgbTrack, 0, sizeof(m_rgbTrack));
	SetBits(m_rgbTrack, (1 << (Tcb2Heap - Tcb2Min + 1)) - 2, 0b01, 0b01);
}

template <uint8_t Tcb2Min, uint8_t Tcb2Heap>
void *Allocator<Tcb2Min, Tcb2Heap>::PvAlloc(size_t cb)
{
	if (cb > CbHeap())
		return nullptr;

	// how many blocks are needed? (1 block = cb2Min)
	size_t cblkReq = 1;
	while (cb > CbMin() * cblkReq)
		cblkReq <<= 1;

	// find the smallest free block that will fit
	CriticalSectionState css;
	size_t iblkStart = 0;
	size_t iblkMin = 0;
	size_t cblkMin = 0;
	for (size_t iblk = 0; cblkMin != cblkReq && iblk < (1U << (Tcb2Heap - Tcb2Min)); )
	{
		switch (BitsGet(m_rgbTrack, iblk << 1, 0b11))
		{
		case 0b00:
			++iblk;
			break;
		case 0b01:  // end of free block; accept if better
			if (cblkMin == 0 || (cblkMin > (iblk - iblkStart + 1) && cblkMin >= cblkReq))
			{
				cblkMin = iblk - iblkStart + 1;
				iblkMin = iblkStart;
			}
			// fall through
		case 0b10:  // in-use block
		case 0b11:  // end of in-use region
			iblkStart = iblk = (iblk & ~(cblkReq - 1)) + cblkReq;
			break;
		}
	}

	if (cblkMin == 0)
		return nullptr;

	// split it until it's the required size
	while (cblkMin > cblkReq)
	{
		cblkMin >>= 1;
		SetBits(m_rgbTrack, (iblkMin + cblkMin - 1) << 1, 0b01, 0b01);
	}

	// mark in-use
	for (size_t iblk = iblkMin; iblk < iblkMin + cblkReq; ++iblk)
		SetBits(m_rgbTrack, iblk << 1, 0b10, 0b10);
	
	return m_rgbHeap + (iblkMin << Tcb2Min);
}

template <typename T>
AllocatorBase::P<T> AllocatorBase::PtAlloc()
{
	return PtAttach<T>(static_cast<T *>(PvAlloc(sizeof(T))));
}

template <typename T>
AllocatorBase::P<T []> AllocatorBase::PtAlloc(size_t c)
{
	return PtAttachRg<T>(static_cast<T *>(PvAlloc(c * sizeof(T))));
}

template <uint8_t Tcb2Min, uint8_t Tcb2Heap>
void Allocator<Tcb2Min, Tcb2Heap>::FreePv(void const *pv)
{
	// find the block
	if (pv < m_rgbHeap || pv >= m_rgbHeap + CbHeap())
		NotReachedReturn();
	size_t iblk = ((uint8_t *)pv - m_rgbHeap) >> Tcb2Min;
	
	// mark the block as free
	CriticalSectionState css;
	size_t cblk = 0;
	for (size_t iblkMark = iblk; iblkMark < (1U << (Tcb2Heap - Tcb2Min)); ++iblkMark)
	{
		Assert(BitsGet(m_rgbTrack, (iblkMark << 1), 0b10) == 0b10);
		++cblk;
		SetBits(m_rgbTrack, iblkMark << 1, 0b10, 0b00);
		if (BitsGet(m_rgbTrack, iblkMark << 1, 0b01) == 0b01)
			break;
	}

	// coalesce with free buddies
	for ( ; cblk < 1U << (Tcb2Heap - Tcb2Min); cblk <<= 1)
	{
		// is the buddy free?
		size_t iblkBuddy = iblk ^ cblk;
		for (size_t iblkChk = iblkBuddy; iblkChk < iblkBuddy + cblk; ++iblkChk)
			if (BitsGet(m_rgbTrack, (iblkChk << 1), 0b10) == 0b10)
				return;  // we're done

		// coalesce
		iblk = std::min(iblk, iblkBuddy);
		SetBits(m_rgbTrack, (iblk + cblk - 1) << 1, 0b01, 0b00);
	}
}

#ifdef UNITTEST

template <uint8_t Tcb2Min, uint8_t Tcb2Heap>
const void *Allocator<Tcb2Min, Tcb2Heap>::PvStart(const void *pv) const
{
	if (pv < m_rgbHeap || pv >= m_rgbHeap + (1U << Tcb2Heap))
		return nullptr;

	size_t iblk = ((uint8_t *)pv - m_rgbHeap) >> Tcb2Min;
	for ( ; iblk > 0 && BitsGet(m_rgbTrack, (iblk - 1) << 1, 0b01) != 0b01; --iblk)
		;

	return m_rgbHeap + (iblk << Tcb2Min);
}

template <uint8_t Tcb2Min, uint8_t Tcb2Heap>
size_t Allocator<Tcb2Min, Tcb2Heap>::CbUsed() const
{
	size_t cblkUsed = 0;
	for (size_t iblk = 0; iblk < (1U << (Tcb2Heap - Tcb2Min)); ++iblk)
		if (BitsGet(m_rgbTrack, iblk << 1, 0b10) == 0b10)
			++cblkUsed;
	return cblkUsed * CbMin();
}

template <uint8_t Tcb2Min, uint8_t Tcb2Heap>
void Allocator<Tcb2Min, Tcb2Heap>::PrintInfo(std::ostream &os) const
{
#ifdef TEST_OUTPUT
	os << "Heap size: " << CbHeap() << " (2^" << int(Tcb2Heap) << "); minimum allocation: " << CbMin() << " (2^" << int(Tcb2Min) << ")\n";
#endif
}

template <uint8_t Tcb2Min, uint8_t Tcb2Heap>
void Allocator<Tcb2Min, Tcb2Heap>::PrintState(std::ostream &os) const
{
#ifdef TEST_OUTPUT
	for (size_t iblk = 0; iblk < (1U << (Tcb2Heap - Tcb2Min)); ++iblk)
		os << (BitsGet(m_rgbTrack, iblk << 1, 0b10) ? 'X' : ' ');
	os << "\n";

	for (size_t iblk = 0; iblk < (1U << (Tcb2Heap - Tcb2Min)); ++iblk)
		os << (BitsGet(m_rgbTrack, iblk << 1, 0b01) ? '|' : ' ');
	os << "\n";
#endif
}

#endif

