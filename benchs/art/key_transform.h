#ifndef ART_KEY_TRANSFORM_H
#define ART_KEY_TRANSFORM_H

#include <stdint.h>
#include <cstring>
#include <array>
#include <utility>


namespace art {
    inline uint16_t byte_swap(uint16_t __val) {
        return (__val << 8) | (__val >> 8);
    }

    inline int16_t byte_swap(int16_t __val) {
        return (__val << 8) | ((__val >> 8) & 0xFF);
    }

    inline uint32_t byte_swap(uint32_t __val) {
        __val = ((__val << 8) & 0xFF00FF00) | ((__val >> 8) & 0xFF00FF);
        return (__val << 16) | (__val >> 16);
    }

    inline int32_t byte_swap(int32_t __val) {
        __val = ((__val << 8) & 0xFF00FF00) | ((__val >> 8) & 0xFF00FF);
        return (__val << 16) | ((__val >> 16) & 0xFFFF);
    }

    inline uint64_t byte_swap(uint64_t __val) {
        __val = ((__val << 8) & 0xFF00FF00FF00FF00ULL) | ((__val >> 8) & 0x00FF00FF00FF00FFULL);
        __val = ((__val << 16) & 0xFFFF0000FFFF0000ULL) | ((__val >> 16) & 0x0000FFFF0000FFFFULL);
        return (__val << 32) | (__val >> 32);
    }

    inline int64_t byte_swap(int64_t __val) {
        __val = ((__val << 8) & 0xFF00FF00FF00FF00ULL) | ((__val >> 8) & 0x00FF00FF00FF00FFULL);
        __val = ((__val << 16) & 0xFFFF0000FFFF0000ULL) | ((__val >> 16) & 0x0000FFFF0000FFFFULL);
        return (__val << 32) | ((__val >> 32) & 0xFFFFFFFFULL);
    }

    inline bool is_big_endian() {
        int num = 1;
        return !(*(char *) &num == 1);
    }

    template<typename _Key, size_t MAX_SIZE = 256>
    struct key_transform;

    // Unsigned integers are already binary comparable.
    // Transform little to big endian if necessary.
    template<>
    struct key_transform<uint8_t> {
        uint8_t operator()(uint8_t __val) const noexcept {
            return __val;
        }
    };

    template<>
    struct key_transform<uint16_t> {
        uint16_t operator()(uint16_t __val) const noexcept {
            return is_big_endian() ? __val : byte_swap(__val);
        }
    };

    template<>
    struct key_transform<uint32_t> {
        uint32_t operator()(uint32_t __val) const noexcept {
            return is_big_endian() ? __val : byte_swap(__val);
        }
    };

    template<>
    struct key_transform<uint64_t> {
        uint64_t operator()(uint64_t __val) const noexcept {
            return is_big_endian() ? __val : byte_swap(__val);
        }
    };

    // For signed integers, we need to flip the MSB as it is
    // 1 for negatives, 0 for positive values (incl. 0)
    // Binary comparison would determine negative values to
    // be larger than positive ones.
    // Transform little to big endian if necessary.
    template<>
    struct key_transform<int8_t> {
        int8_t operator()(int8_t __val) const noexcept {
            return __val ^ (int8_t(1) << (8 - 1));
        }
    };

    template<>
    struct key_transform<int16_t> {
        int16_t operator()(int16_t __val) const noexcept {
            __val ^= int16_t(1) << (16 - 1);
            return is_big_endian() ? __val : byte_swap(__val);
        }
    };

    template<>
    struct key_transform<int32_t> {
        int32_t operator()(int32_t __val) const noexcept {
            __val ^= int32_t(1) << (32 - 1);
            return is_big_endian() ? __val : byte_swap(__val);
        }
    };

    template<>
    struct key_transform<int64_t> {
        int64_t operator()(int64_t __val) const noexcept {
            __val ^= int64_t(1) << (64 - 1);
            return is_big_endian() ? __val : byte_swap(__val);
        }
    };

    // Pair of supported keys
    template<typename T1, typename T2>
    struct key_transform<std::pair<T1, T2> > {
    private:
        key_transform<T1> first;
        key_transform<T2> second;

        typedef std::array<uint8_t,
                sizeof(decltype(key_transform<T1>()(T1()))) +
                sizeof(decltype(key_transform<T2>()(T2())))> transformed_type;
    public:
        key_transform() : first(), second() {}

        transformed_type operator()(const std::pair<T1, T2> &key) {
            transformed_type transformed_key;
            auto transformed1 = first(key.first);
            auto transformed2 = second(key.second);

            std::move(static_cast<const char *>(static_cast<const void *>(&transformed1)),
                      static_cast<const char *>(static_cast<const void *>(&transformed1)) + sizeof(transformed1),
                      transformed_key.begin());
            std::move(static_cast<const char *>(static_cast<const void *>(&transformed2)),
                      static_cast<const char *>(static_cast<const void *>(&transformed2)) + sizeof(transformed2),
                      transformed_key.begin() + sizeof(transformed1));
            return transformed_key;
        }
    };

    // fixed size ascii string transform
    template<size_t MAX_SIZE>
    struct key_transform<std::string, MAX_SIZE> {
        std::array<char, MAX_SIZE> operator()(const std::string &key) const noexcept {
            // make sure to initialize the whole array, otherwise the suffix of
            // identical keys could be different
            std::array<char, MAX_SIZE> transformed{};
            std::memcpy(&transformed, key.c_str(), key.size() + 1);

            return transformed;
        }
    };
}

#endif //ART_KEY_TRANSFORM_H
