#pragma once

#include "acl/core/error.h"

#include <cstdint>

namespace acl
{
	////////////////////////////////////////////////////////////////////////////////
	// A bit set description holds the required information to ensure type and memory safety
	// with the various bit set functions.
	////////////////////////////////////////////////////////////////////////////////
	class BitSetDescription
	{
	public:
		////////////////////////////////////////////////////////////////////////////////
		// Creates an invalid bit set description.
		constexpr BitSetDescription() : m_size(0) {}

		////////////////////////////////////////////////////////////////////////////////
		// Creates a bit set description from a compile time known number of bits.
		template<uint32_t num_bits>
		static constexpr BitSetDescription make_from_num_bits()
		{
			static_assert(num_bits == int32_t(num_bits), "Number of bits exceeds the maximum number allowed");
			return BitSetDescription((num_bits + 32 - 1) / 32);
		}

		////////////////////////////////////////////////////////////////////////////////
		// Creates a bit set description from a runtime known number of bits.
		inline static BitSetDescription make_from_num_bits(int32_t num_bits)
		{
			ACL_ASSERT(num_bits >= 0, "Cannot create a bit set with a negative number of bits: %d", num_bits);
			return BitSetDescription((num_bits + 32 - 1) / 32);
		}

		////////////////////////////////////////////////////////////////////////////////
		// Returns the number of 32 bit words used to represent the bitset.
		// 1 == 32 bits, 2 == 64 bits, etc.
		constexpr int32_t get_size() const { return m_size; }

		////////////////////////////////////////////////////////////////////////////////
		// Returns the number of bits contained within the bit set.
		constexpr int32_t get_num_bits() const { return m_size * 32; }

		////////////////////////////////////////////////////////////////////////////////
		// Returns the number of bytes used by the bit set.
		constexpr int32_t get_num_bytes() const { return m_size * sizeof(int32_t); }

		////////////////////////////////////////////////////////////////////////////////
		// Returns true if the index is valid within the bit set.
		constexpr bool is_bit_index_valid(int32_t index) const { return index >= 0 && index < get_num_bits(); }

	private:
		////////////////////////////////////////////////////////////////////////////////
		// Creates a bit set description from a specified size.
		explicit constexpr BitSetDescription(int32_t size) : m_size(size) {}

		// Number of words required to hold the bit set
		// 1 == 32 bits, 2 == 64 bits, etc.
		int32_t		m_size;
	};

	////////////////////////////////////////////////////////////////////////////////
	// Resets the entire bit set to the provided value.
	inline void bitset_reset(uint32_t* bitset, BitSetDescription desc, bool value)
	{
		const int32_t mask = value ? 0xFFFFFFFF : 0x00000000;
		const int32_t size = desc.get_size();

		for (int32_t offset = 0; offset < size; ++offset)
			bitset[offset] = mask;
	}

	////////////////////////////////////////////////////////////////////////////////
	// Sets a specific bit to its desired value.
	inline void bitset_set(uint32_t* bitset, BitSetDescription desc, int32_t bit_index, bool value)
	{
		ACL_ASSERT(desc.is_bit_index_valid(bit_index), "Invalid bit index: %d", bit_index);
		(void)desc;

		const int32_t offset = bit_index / 32;
		const int32_t mask = 1 << (31 - (bit_index % 32));

		if (value)
			bitset[offset] |= mask;
		else
			bitset[offset] &= ~mask;
	}

	////////////////////////////////////////////////////////////////////////////////
	// Sets a specified range of bits to a specified value.
	inline void bitset_set_range(uint32_t* bitset, BitSetDescription desc, int32_t start_bit_index, int32_t num_bits, bool value)
	{
		ACL_ASSERT(desc.is_bit_index_valid(start_bit_index), "Invalid start bit index: %d", start_bit_index);
		ACL_ASSERT(num_bits >= 0, "Invalid num bits: %d", num_bits);
		ACL_ASSERT(start_bit_index + num_bits <= desc.get_num_bits(), "Invalid num bits: %d > %d", start_bit_index + num_bits, desc.get_num_bits());

		const int32_t end_bit_offset = start_bit_index + num_bits;
		for (int32_t offset = start_bit_index; offset < end_bit_offset; ++offset)
			bitset_set(bitset, desc, offset, value);
	}

	////////////////////////////////////////////////////////////////////////////////
	// Returns the bit value as a specific index.
	inline bool bitset_test(const uint32_t* bitset, BitSetDescription desc, int32_t bit_index)
	{
		ACL_ASSERT(desc.is_bit_index_valid(bit_index), "Invalid bit index: %d", bit_index);
		(void)desc;

		const int32_t offset = bit_index / 32;
		const int32_t mask = 1 << (31 - (bit_index % 32));

		return (bitset[offset] & mask) != 0;
	}

	////////////////////////////////////////////////////////////////////////////////
	// Counts the total number of set (true) bits within the bit set.
	inline int32_t bitset_count_set_bits(const uint32_t* bitset, BitSetDescription desc)
	{
		const int32_t size = desc.get_size();

		// TODO: Use popcount instruction if available
		int32_t num_set_bits = 0;
		for (int32_t offset = 0; offset < size; ++offset)
		{
			uint32_t value = bitset[offset];
			value = value - ((value >> 1) & 0x55555555);
			value = (value & 0x33333333) + ((value >> 2) & 0x33333333);
			num_set_bits += (((value + (value >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
		}

		return num_set_bits;
	}
}
