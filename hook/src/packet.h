#pragma once

#include <cstddef>
#include <cstring>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include "Tpp/TppTypes.h"

enum class PacketType : uint8_t {
    NET_NATIVE_PACKET,
    NET_CONNECTION_ESTABLISHED,
    NET_GAMEOBJECT_POSITION,
    NET_MISSION_START,
    NET_CONNECT
};

class Packet {
  public:
    Packet() = default;
    explicit Packet(size_t reserve) { m_buffer.reserve(reserve); }

    void Clear() {
        m_buffer.clear();
        m_cursor = 0;
    }

    void ResetCursor() { m_cursor = 0; }

    void Seek(size_t pos) {
        if (pos > m_buffer.size())
            throw std::out_of_range("Packet::Seek");

        m_cursor = pos;
    }

    [[nodiscard]] size_t Tell() const { return m_cursor; }

    [[nodiscard]] size_t Size() const { return m_buffer.size(); }

    [[nodiscard]] bool End() const { return m_cursor >= m_buffer.size(); }

    [[nodiscard]] const std::byte *Data() const { return m_buffer.data(); }

    [[nodiscard]] std::byte *Data() { return m_buffer.data(); }

    void Start(PacketType type) { 
        Write<uint8_t>(static_cast<uint8_t>(type));
    }

    void WriteVec3(Vector3& vec) { 
        Write<float>(vec.x);
        Write<float>(vec.y);
        Write<float>(vec.z);
    }

    Vector3 ReadVec3() {
        Vector3 vec;

        vec.x = Read<float>();
        vec.y = Read<float>();
        vec.z = Read<float>();

        return vec;
    }

    template <class T> void Write(const T &value) {
        static_assert(std::is_trivially_copyable_v<T>);

        WriteBytes(&value, sizeof(T));
    }

    template <class T> T Read() {
        static_assert(std::is_trivially_copyable_v<T>);

        T value;
        ReadBytes(&value, sizeof(T));
        return value;
    }

    void WriteBytes(const void *data, size_t size) {
        size_t oldSize = m_buffer.size();
        m_buffer.resize(oldSize + size);

        std::memcpy(m_buffer.data() + oldSize, data, size);
    }

    void ReadBytes(void *data, size_t size) {
        if (m_cursor + size > m_buffer.size())
            throw std::runtime_error("Packet overflow");

        std::memcpy(data, m_buffer.data() + m_cursor, size);
        m_cursor += size;
    }

    void WriteString(const char *str) {
        size_t len = std::strlen(str) + 1;
        WriteBytes(str, len);
    }

    void WriteString(const std::string &str) { WriteBytes(str.data(), str.size() + 1); }

    std::string ReadString() {
        const char *start = reinterpret_cast<const char *>(m_buffer.data() + m_cursor);
        size_t maxLen = m_buffer.size() - m_cursor;
        size_t len = strnlen(start, maxLen) + 1;
        if (m_cursor + len > m_buffer.size())
            throw std::runtime_error("Packet::ReadString overflow");
        std::string result(start, len - 1);
        m_cursor += len;
        return result;
    }

  private:
    std::vector<std::byte> m_buffer;
    size_t m_cursor = 0;
};