/*
   Memory allocator.

   D. Robins, 20141010
*/

#include "Alloc.h"

#ifdef UNITTEST

#include "gtest/gtest.h"


class AllocTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		m_alloc.PrintInfo(std::cout);
		m_alloc.PrintState(std::cout);
	}

	void TearDown() override
	{
		m_alloc.PrintState(std::cout);
		ASSERT_EQ(0, m_alloc.CbUsed());
	}

	void Fill()
	{
		auto pv = m_alloc.PvAlloc(m_alloc.CbHeap());
		ASSERT_NE(nullptr, pv);
		memset(pv, bFill, m_alloc.CbHeap());
		m_alloc.FreePv(pv);
	}

	static bool FFilled(const void *pv, uint8_t b, size_t cb)
	{
		for (size_t i = 0; i < cb; ++i)
			if (((uint8_t *)pv)[i] != b)
				return false;
		return true;
	}

	static const uint8_t bFill = 0xcd;
	Allocator<4, 10> m_alloc;
};

TEST(Alloc, Create)
{
	Allocator<4, 10> alloc{};
	EXPECT_EQ(0, alloc.CbUsed());
}

// linear minimal allocations, then free
TEST_F(AllocTest, LinearFill)
{
	for (auto cb : { 16, 1, 15, 0 })
	{
		for (auto i = 0; i < m_alloc.CbHeap() / 16; ++i)
			ASSERT_EQ(m_alloc.PbHeap() + i * 16, m_alloc.PvAlloc(cb));
		EXPECT_EQ(m_alloc.CbHeap(), m_alloc.CbUsed());
		m_alloc.PrintState(std::cout);
		EXPECT_EQ(nullptr, m_alloc.PvAlloc(0));
		for (auto i = 0; i < m_alloc.CbHeap() / 16; ++i)
			m_alloc.FreePv(m_alloc.PbHeap() + i * 16);
	}
}

// fills of various size
TEST_F(AllocTest, FillVarious)
{
	for (auto cb = m_alloc.CbHeap(), cbMin = m_alloc.CbMin(); cb >= cbMin; cb >>= 1)
	{
		m_alloc.PrintState(std::cout);
		for (auto i = 0; i < m_alloc.CbHeap() / cb; ++i)
			EXPECT_EQ(m_alloc.PvAlloc(cb), m_alloc.PbHeap() + i * cb);
		EXPECT_EQ(m_alloc.CbHeap(), m_alloc.CbUsed());
		m_alloc.PrintState(std::cout);
		EXPECT_EQ(nullptr, m_alloc.PvAlloc(0));
		for (auto i = 0; i < m_alloc.CbHeap() / cb; ++i)
			m_alloc.FreePv(m_alloc.PbHeap() + i * cb);
		EXPECT_EQ(0, m_alloc.CbUsed());
		m_alloc.PrintState(std::cout);
	}
}

// basic alignment test
TEST_F(AllocTest, BasicAlign)
{
	auto pv1 = m_alloc.PvAlloc(m_alloc.CbMin());
	EXPECT_EQ(m_alloc.PbHeap(), pv1);
	auto cb = m_alloc.CbMin() * 8;
	auto pv2 = m_alloc.PvAlloc(cb);
	m_alloc.PrintState(std::cout);
	EXPECT_EQ(0, ((const uint8_t *)pv2 - m_alloc.PbHeap()) % cb);
	m_alloc.FreePv(pv1);
	m_alloc.FreePv(pv2);
}

// check coalescing
TEST_F(AllocTest, CheckCoalesce)
{
	auto cb = m_alloc.CbMin();
	for (int i = 0; i < m_alloc.CbHeap() / cb; ++i)
		m_alloc.PvAlloc(cb);
	for (int i = m_alloc.CbHeap() / cb - 1; i >= 0; --i)
		m_alloc.FreePv(m_alloc.PbHeap() + cb * i);
	EXPECT_NE(nullptr, m_alloc.PvAlloc(m_alloc.CbHeap()));
	m_alloc.FreePv(m_alloc.PbHeap());
}

// non-uniform allocations
// NOTE: not "random"; tests must be repeatable
TEST_F(AllocTest, NonUniform)
{
	Fill();
	std::vector<void *> vcpv;
	size_t rgcb[] = { 1, 17, 33, 2, 58, 14, 500, 120, 15, 3, 40 };
	uint8_t i = 17;
	for (auto cb : rgcb)
	{
		auto pv = m_alloc.PvAlloc(cb);
		EXPECT_TRUE(FFilled(pv, bFill, cb));
		memset(pv, i, cb);
		vcpv.push_back(pv);
		++i;
	}
	ASSERT_EQ(nullptr, m_alloc.PvAlloc(m_alloc.CbHeap() / 4));
	m_alloc.PrintState(std::cout);
	i = 17;
	for (auto pv : vcpv)
	{
		EXPECT_TRUE(FFilled(pv, i, rgcb[i - 17]));
		m_alloc.FreePv(pv);
		++i;
	}
}

TEST_F(AllocTest, SmallToLarge)
{
	std::vector<void *> vcpv;
	for (auto cb = m_alloc.CbMin(); cb < m_alloc.CbHeap(); cb <<= 1)
	{
		auto pv = m_alloc.PvAlloc(cb);
		EXPECT_NE(nullptr, pv);
		EXPECT_EQ(0, ((uint8_t *)pv - m_alloc.PbHeap()) % cb);
		vcpv.push_back(pv);	
	}
	m_alloc.PrintState(std::cout);
	EXPECT_EQ(m_alloc.CbUsed(), m_alloc.CbHeap() - m_alloc.CbMin());
		
	for (auto cb = m_alloc.CbMin() << 1; cb <= m_alloc.CbHeap(); cb <<= 1)
		EXPECT_EQ(nullptr, m_alloc.PvAlloc(cb));
	
	for (auto pv : vcpv)
		m_alloc.FreePv(pv);
}

TEST_F(AllocTest, LargeToSmall)
{
	std::vector<void *> vcpv;
	for (auto cb = m_alloc.CbHeap() >> 1; cb >= m_alloc.CbMin(); cb >>= 1)
	{
		auto pv = m_alloc.PvAlloc(cb);
		EXPECT_NE(nullptr, pv);
		EXPECT_EQ(0, ((uint8_t *)pv - m_alloc.PbHeap()) % cb);
		vcpv.push_back(pv); 
	}
	m_alloc.PrintState(std::cout);
	EXPECT_EQ(m_alloc.CbUsed(), m_alloc.CbHeap() - m_alloc.CbMin());
		
	for (auto cb = m_alloc.CbMin() << 1; cb <= m_alloc.CbHeap(); cb <<= 1)
		EXPECT_EQ(nullptr, m_alloc.PvAlloc(cb));
	
	for (auto pv : vcpv)
		m_alloc.FreePv(pv);
}

TEST_F(AllocTest, PvStart)
{
	size_t vccb[] = { 511, 17, 14, 99, 32 };
	std::vector<void *> vcpv;
	for (auto cb : vccb)
		vcpv.push_back(m_alloc.PvAlloc(cb));
	m_alloc.PrintState(std::cout);

	// we're interested that all addresses within an allocation report the correct start;
	// but we don't care at all what points outside report (i.e., results there are undefined)
	auto i = vcpv.cbegin();
	for (auto cb : vccb)
	{
		auto pv = *i++;
		for (auto i = 0; i < cb; ++i)
			EXPECT_EQ(pv, m_alloc.PvStart((uint8_t *)pv + i));
	}
	
	for (auto pv : vcpv)
		m_alloc.FreePv(pv);
}

TEST_F(AllocTest, SmartPtr)
{
	{
		std::vector<AllocatorBase::P<uint8_t []>> vcpb;
		for (auto i = 0; i < m_alloc.CbHeap() / 16; ++i)
		{
			auto pb = m_alloc.PtAlloc<uint8_t>(16);
			ASSERT_EQ(m_alloc.PbHeap() + i * 16, pb.get());
			vcpb.push_back(std::move(pb));
		}
		EXPECT_EQ(m_alloc.CbHeap(), m_alloc.CbUsed());
		m_alloc.PrintState(std::cout);
		EXPECT_EQ(nullptr, m_alloc.PtAlloc<uint8_t>(0));
	}
	EXPECT_EQ(0, m_alloc.CbUsed());
}

#endif

