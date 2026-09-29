#include <gigante/Logger.h>

#include <algorithm>
#include <atomic>
#include <vector>

namespace gigante
{
namespace
{

[[nodiscard]] std::string sanitize(const std::string_view value)
{
    std::string result(value);
    std::replace_if(
        result.begin(),
        result.end(),
        [](const char character)
        {
            return character == ' ' || character == '\r' || character == '\n';
        },
        '_');
    return result;
}

} // namespace

std::uint64_t SessionIdSequence::next() noexcept
{
    return nextValue.fetch_add(1, std::memory_order_relaxed);
}

std::string Logger::formatLine(
    const std::uint64_t sessionId,
    const std::string_view event,
    const std::initializer_list<LogField> fields)
{
    return formatLine(
        sessionId,
        event,
        std::span<const LogField>(fields.begin(), fields.size()));
}

std::string Logger::formatLine(
    const std::uint64_t sessionId,
    const std::string_view event,
    const std::span<const LogField> fields)
{
    std::vector<LogField> sortedFields(fields.begin(), fields.end());
    std::stable_sort(
        sortedFields.begin(),
        sortedFields.end(),
        [](const LogField& left, const LogField& right)
        {
            return left.key < right.key;
        });

    auto line = std::string("session=") + std::to_string(sessionId)
        + " event=" + sanitize(event);
    for (const auto& field : sortedFields)
    {
        line += ' ';
        line += sanitize(field.key);
        line += '=';
        line += sanitize(field.value);
    }
    return line;
}

} // namespace gigante
