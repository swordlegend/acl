#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2017 Nicholas Frechette & Animation Compression Library contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
////////////////////////////////////////////////////////////////////////////////

#include "acl/core/error.h"

#include <cstdint>
#include <cstring>
#include <type_traits>
#include <limits>
#include <memory>
#include <algorithm>

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// Allows static branching without any warnings

	template<bool expression_result>
	struct static_condition { static constexpr bool test() { return true; } };

	template<>
	struct static_condition<false> { static constexpr bool test() { return false; } };

	//////////////////////////////////////////////////////////////////////////
	// Various miscellaneous utilities related to alignment

	constexpr bool is_power_of_two(size_t input)
	{
		return input != 0 && (input & (input - 1)) == 0;
	}

	template<typename Type>
	constexpr bool is_alignment_valid(size_t alignment)
	{
		return is_power_of_two(alignment) && alignment >= alignof(Type);
	}

	template<typename PtrType>
	inline bool is_aligned_to(PtrType* value, size_t alignment)
	{
		ACL_ASSERT(is_power_of_two(alignment), "Alignment value must be a power of two");
		return (reinterpret_cast<intptr_t>(value) & (alignment - 1)) == 0;
	}

	template<typename IntegralType>
	inline bool is_aligned_to(IntegralType value, size_t alignment)
	{
		ACL_ASSERT(is_power_of_two(alignment), "Alignment value must be a power of two");
		return (static_cast<size_t>(value) & (alignment - 1)) == 0;
	}

	template<typename PtrType>
	constexpr bool is_aligned(PtrType* value)
	{
		return is_aligned_to(value, alignof(PtrType));
	}

	template<typename PtrType>
	inline PtrType* align_to(PtrType* value, size_t alignment)
	{
		ACL_ASSERT(is_power_of_two(alignment), "Alignment value must be a power of two");
		return reinterpret_cast<PtrType*>((reinterpret_cast<intptr_t>(value) + (alignment - 1)) & ~(alignment - 1));
	}

	template<typename IntegralType>
	inline IntegralType align_to(IntegralType value, size_t alignment)
	{
		ACL_ASSERT(is_power_of_two(alignment), "Alignment value must be a power of two");
		return static_cast<IntegralType>((static_cast<size_t>(value) + (alignment - 1)) & ~(alignment - 1));
	}

	template<typename PreviousMemberType, typename NextMemberType>
	constexpr size_t get_required_padding()
	{
		// align_to(sizeof(PreviousMemberType), alignof(NextMemberType)) - sizeof(PreviousMemberType)
		return ((sizeof(PreviousMemberType) + (alignof(NextMemberType) - 1)) & ~(alignof(NextMemberType)- 1)) - sizeof(PreviousMemberType);
	}

	template<typename ElementType, size_t num_elements>
	constexpr size_t get_array_size(ElementType const (&)[num_elements]) { return num_elements; }

	//////////////////////////////////////////////////////////////////////////
	// Type safe casting

	namespace memory_impl
	{
		template<typename DestPtrType, typename SrcType>
		struct safe_ptr_to_ptr_cast_impl
		{
			inline static DestPtrType* cast(SrcType* input)
			{
				ACL_ASSERT(is_aligned_to(input, alignof(DestPtrType)), "reinterpret_cast would result in an unaligned pointer");
				return reinterpret_cast<DestPtrType*>(input);
			}
		};

		template<typename SrcType>
		struct safe_ptr_to_ptr_cast_impl<void, SrcType>
		{
			static constexpr void* cast(SrcType* input) { return input; }
		};

		template<typename DestPtrType, typename SrcType>
		struct safe_int_to_ptr_cast_impl
		{
			inline static DestPtrType* cast(SrcType input)
			{
				ACL_ASSERT(is_aligned_to(input, alignof(DestPtrType)), "reinterpret_cast would result in an unaligned pointer");
				return reinterpret_cast<DestPtrType*>(input);
			}
		};

		template<typename SrcType>
		struct safe_int_to_ptr_cast_impl<void, SrcType>
		{
			static constexpr void* cast(SrcType input) { return reinterpret_cast<void*>(input); }
		};
	}

	template<typename DestPtrType, typename SrcType>
	inline DestPtrType* safe_ptr_cast(SrcType* input)
	{
		return memory_impl::safe_ptr_to_ptr_cast_impl<DestPtrType, SrcType>::cast(input);
	}

	template<typename DestPtrType, typename SrcType>
	inline DestPtrType* safe_ptr_cast(SrcType input)
	{
		return memory_impl::safe_int_to_ptr_cast_impl<DestPtrType, SrcType>::cast(input);
	}

	namespace memory_impl
	{
		template<bool is_enum = true>
		struct safe_static_cast_impl
		{
			template<typename DestIntegralType, typename SrcEnumType>
			static inline DestIntegralType cast(SrcEnumType input)
			{
#if defined(ACL_HAS_ASSERT_CHECKS)
				typedef typename std::underlying_type<SrcEnumType>::type SrcIntegralType;

				const SrcIntegralType integral_input = static_cast<SrcIntegralType>(input);

				constexpr DestIntegralType min = std::numeric_limits<DestIntegralType>::lowest();
				constexpr DestIntegralType max = std::numeric_limits<DestIntegralType>::max();

				if (static_condition<std::numeric_limits<SrcIntegralType>::is_signed && !std::numeric_limits<DestIntegralType>::is_signed>::test())
				{
					ACL_ASSERT(0 <= integral_input && integral_input <= max, "static_cast would result in truncation");
				}
				else if (static_condition<!std::numeric_limits<SrcIntegralType>::is_signed && std::numeric_limits<DestIntegralType>::is_signed>::test())
				{
					ACL_ASSERT(integral_input <= max, "static_cast would result in truncation");
				}
				else
				{
					ACL_ASSERT(min <= integral_input && integral_input <= max, "static_cast would result in truncation");
				}
#endif

				return static_cast<DestIntegralType>(input);
			}
		};

		template<>
		struct safe_static_cast_impl<false>
		{
			template<typename DestNumericType, typename SrcNumericType>
			static inline DestNumericType cast(SrcNumericType input)
			{
#if defined(ACL_HAS_ASSERT_CHECKS)
				constexpr DestNumericType min = std::numeric_limits<DestNumericType>::lowest();
				constexpr DestNumericType max = std::numeric_limits<DestNumericType>::max();

				if (static_condition<std::numeric_limits<SrcNumericType>::is_signed && !std::numeric_limits<DestNumericType>::is_signed>::test())
				{
					ACL_ASSERT(0 <= input && input <= max, "static_cast would result in truncation");
				}
				else if (static_condition<!std::numeric_limits<SrcNumericType>::is_signed && std::numeric_limits<DestNumericType>::is_signed>::test())
				{
					ACL_ASSERT(input <= max, "static_cast would result in truncation");
				}
				else
				{
					ACL_ASSERT(min <= input && input <= max, "static_cast would result in truncation");
				}
#endif

				return static_cast<DestNumericType>(input);
			}
		};
	}

	template<typename DestIntegralType, typename SrcType>
	inline DestIntegralType safe_static_cast(SrcType input)
	{
		return memory_impl::safe_static_cast_impl<std::is_enum<SrcType>::value>::template cast<DestIntegralType, SrcType>(input);
	}

	//////////////////////////////////////////////////////////////////////////
	// Endian and raw memory support

	template<typename OutputPtrType, typename InputPtrType, typename OffsetType>
	inline OutputPtrType* add_offset_to_ptr(InputPtrType* ptr, OffsetType offset)
	{
		return safe_ptr_cast<OutputPtrType>(reinterpret_cast<uintptr_t>(ptr) + offset);
	}

	constexpr uint16_t byte_swap(uint16_t value)
	{
		return (value & 0x00FF) << 8 | (value & 0xFF00) >> 8;
	}

	inline uint32_t byte_swap(uint32_t value)
	{
		value = (value & 0x0000FFFF) << 16 | (value & 0xFFFF0000) >> 16;
		value = (value & 0x00FF00FF) << 8 | (value & 0xFF00FF00) >> 8;
		return value;
	}

	inline uint64_t byte_swap(uint64_t value)
	{
		value = (value & 0x00000000FFFFFFFF) << 32 | (value & 0xFFFFFFFF00000000) >> 32;
		value = (value & 0x0000FFFF0000FFFF) << 16 | (value & 0xFFFF0000FFFF0000) >> 16;
		value = (value & 0x00FF00FF00FF00FF) << 8 | (value & 0xFF00FF00FF00FF00) >> 8;
		return value;
	}

	// We copy bits assuming big-endian ordering for 'dest' and 'src'
	inline void memcpy_bits(void* dest, uint64_t dest_bit_offset, const void* src, uint64_t src_bit_offset, uint64_t num_bits_to_copy)
	{
		while (true)
		{
			uint64_t src_byte_offset = src_bit_offset / 8;
			uint8_t src_byte_bit_offset = safe_static_cast<uint8_t>(src_bit_offset % 8);
			uint64_t dest_byte_offset = dest_bit_offset / 8;
			uint8_t dest_byte_bit_offset = safe_static_cast<uint8_t>(dest_bit_offset % 8);

			const uint8_t* src_bytes = add_offset_to_ptr<const uint8_t>(src, src_byte_offset);
			uint8_t* dest_byte = add_offset_to_ptr<uint8_t>(dest, dest_byte_offset);

			// We'll copy only as many bits as there fits within 'dest' or as there are left
			uint8_t num_bits_dest_remain_in_byte = 8 - dest_byte_bit_offset;
			uint8_t num_bits_src_remain_in_byte = 8 - src_byte_bit_offset;
			uint64_t num_bits_copied = std::min<uint64_t>(std::min<uint8_t>(num_bits_dest_remain_in_byte, num_bits_src_remain_in_byte), num_bits_to_copy);
			uint8_t num_bits_copied_u8 = safe_static_cast<uint8_t>(num_bits_copied);

			// We'll shift and mask to retain the 'dest' bits prior to our offset and whatever remains after the copy
			uint8_t dest_shift_offset = dest_byte_bit_offset;
			uint8_t dest_byte_mask = ~(0xFF >> dest_shift_offset) | ~(0xFF << (8 - num_bits_copied_u8 - dest_byte_bit_offset));

			uint8_t src_shift_offset = 8 - src_byte_bit_offset - num_bits_copied_u8;
			uint8_t src_byte_mask = 0xFF >> (8 - num_bits_copied_u8);
			uint8_t src_insert_shift_offset = 8 - num_bits_copied_u8 - dest_byte_bit_offset;

			uint8_t partial_dest_value = *dest_byte & dest_byte_mask;
			uint8_t partial_src_value = (*src_bytes >> src_shift_offset) & src_byte_mask;
			*dest_byte = partial_dest_value | (partial_src_value << src_insert_shift_offset);

			if (num_bits_to_copy <= num_bits_copied)
				break;	// Done

			num_bits_to_copy -= num_bits_copied;
			dest_bit_offset += num_bits_copied;
			src_bit_offset += num_bits_copied;
		}
	}

	template<typename DataType>
	inline DataType unaligned_load(const void* input)
	{
		DataType result;
		memcpy(&result, input, sizeof(DataType));
		return result;
	}

	template<typename DataType>
	inline DataType aligned_load(const void* input)
	{
		return *safe_ptr_cast<const DataType, const void*>(input);
	}

	template<typename DataType>
	inline void unaligned_write(DataType input, void* output)
	{
		memcpy(output, &input, sizeof(DataType));
	}
}
