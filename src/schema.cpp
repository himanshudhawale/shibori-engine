#include <shibori/engine/schema.hpp>

#include <shibori/engine/checked_arithmetic.hpp>

#include <algorithm>
#include <bit>
#include <map>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

namespace shibori::engine {
namespace {

using Metadata = std::map<std::string, std::vector<std::byte>, std::less<>>;

bool valid_utf8(std::string_view text) noexcept {
  std::size_t index = 0;
  while (index < text.size()) {
    const auto first = static_cast<unsigned char>(text[index]);
    std::size_t count = 0;
    std::uint32_t codepoint = 0;
    if (first <= 0x7fU) {
      ++index;
      continue;
    }
    if ((first & 0xe0U) == 0xc0U) {
      count = 2;
      codepoint = first & 0x1fU;
    } else if ((first & 0xf0U) == 0xe0U) {
      count = 3;
      codepoint = first & 0x0fU;
    } else if ((first & 0xf8U) == 0xf0U) {
      count = 4;
      codepoint = first & 0x07U;
    } else {
      return false;
    }
    if (index + count > text.size()) {
      return false;
    }
    for (std::size_t offset = 1; offset < count; ++offset) {
      const auto byte = static_cast<unsigned char>(text[index + offset]);
      if ((byte & 0xc0U) != 0x80U) {
        return false;
      }
      codepoint = (codepoint << 6U) | (byte & 0x3fU);
    }
    const bool overlong =
        (count == 2 && codepoint < 0x80U) ||
        (count == 3 && codepoint < 0x800U) ||
        (count == 4 && codepoint < 0x10000U);
    if (overlong || codepoint > 0x10ffffU ||
        (codepoint >= 0xd800U && codepoint <= 0xdfffU)) {
      return false;
    }
    index += count;
  }
  return true;
}

Status validate_key(std::string_view key) {
  const auto slash = key.find('/');
  if (key.empty() || !valid_utf8(key) || slash == std::string_view::npos ||
      slash == 0 || slash + 1 == key.size()) {
    return fail(
        ErrorCode::invalid_schema,
        Operation::validate_schema,
        "metadata key must be a qualified, strict UTF-8 name");
  }
  if (key.starts_with("shibori.") && !key.starts_with("shibori.engine/")) {
    return fail(
        ErrorCode::invalid_schema,
        Operation::validate_schema,
        "metadata key uses a reserved namespace");
  }
  return {};
}

Result<std::uint64_t> metadata_size(const Metadata& metadata) {
  std::uint64_t total = 0;
  for (const auto& [key, value] : metadata) {
    auto entry = checked_add(
        static_cast<std::uint64_t>(key.size()),
        static_cast<std::uint64_t>(value.size()),
        Operation::validate_schema,
        "metadata entry");
    if (!entry) {
      return std::unexpected(std::move(entry.error()));
    }
    auto next = checked_add(
        total,
        *entry,
        Operation::validate_schema,
        "metadata bytes");
    if (!next) {
      return std::unexpected(std::move(next.error()));
    }
    total = *next;
  }
  return total;
}

Status validate_metadata(
    const Metadata& metadata,
    const ResourceLimits& limits,
    std::uint64_t& aggregate) {
  auto size = metadata_size(metadata);
  if (!size) {
    return std::unexpected(std::move(size.error()));
  }
  auto next = checked_add(
      aggregate,
      *size,
      Operation::validate_schema,
      "schema metadata bytes");
  if (!next) {
    return std::unexpected(std::move(next.error()));
  }
  aggregate = *next;
  if (aggregate > limits.maximum_metadata_bytes) {
    return fail(
        ErrorCode::limit_exceeded,
        Operation::validate_schema,
        "schema metadata exceeds the configured byte limit");
  }
  return {};
}

template <typename Integer>
void append_integer(std::vector<std::byte>& output, Integer value) {
  using Unsigned = std::make_unsigned_t<Integer>;
  auto bits = static_cast<Unsigned>(value);
  for (std::size_t index = 0; index < sizeof(Integer); ++index) {
    output.push_back(static_cast<std::byte>(bits & 0xffU));
    bits >>= 8U;
  }
}

void append_bytes(
    std::vector<std::byte>& output,
    std::span<const std::byte> bytes) {
  append_integer(output, static_cast<std::uint64_t>(bytes.size()));
  output.insert(output.end(), bytes.begin(), bytes.end());
}

void append_string(std::vector<std::byte>& output, std::string_view text) {
  append_bytes(
      output,
      std::as_bytes(std::span(text.data(), text.size())));
}

void append_metadata(std::vector<std::byte>& output, const Metadata& metadata) {
  append_integer(output, static_cast<std::uint64_t>(metadata.size()));
  for (const auto& [key, value] : metadata) {
    append_string(output, key);
    append_bytes(output, value);
  }
}

void append_type(std::vector<std::byte>& output, const LogicalType& type) {
  output.push_back(static_cast<std::byte>(type.kind()));
  const auto parameters = type.parameters();
  output.push_back(static_cast<std::byte>(parameters.index()));
  if (const auto* decimal = std::get_if<DecimalParameters>(&parameters)) {
    output.push_back(static_cast<std::byte>(decimal->precision));
    append_integer(output, decimal->scale);
  } else if (const auto* time = std::get_if<TimeParameters>(&parameters)) {
    output.push_back(static_cast<std::byte>(time->unit));
  } else if (
      const auto* timestamp = std::get_if<TimestampParameters>(&parameters)) {
    output.push_back(static_cast<std::byte>(timestamp->unit));
    output.push_back(static_cast<std::byte>(timestamp->timezone));
  } else if (
      const auto* string = std::get_if<StringParameters>(&parameters)) {
    output.push_back(static_cast<std::byte>(string->validation));
  } else if (
      const auto* fixed = std::get_if<FixedBinaryParameters>(&parameters)) {
    append_integer(output, fixed->byte_width);
  }
}

std::array<std::byte, 16> make_fingerprint(
    std::span<const std::byte> canonical) noexcept {
  std::uint64_t first = 14695981039346656037ULL;
  std::uint64_t second = 1099511628211ULL;
  for (const auto value : canonical) {
    const auto byte = std::to_integer<std::uint8_t>(value);
    first = (first ^ byte) * 1099511628211ULL;
    second = (second ^ static_cast<std::uint8_t>(byte + 0x9dU)) *
             14029467366897019727ULL;
  }
  std::array<std::byte, 16> result{};
  for (std::size_t index = 0; index < 8; ++index) {
    result[index] = static_cast<std::byte>(first & 0xffU);
    result[index + 8] = static_cast<std::byte>(second & 0xffU);
    first >>= 8U;
    second >>= 8U;
  }
  return result;
}

Status add_metadata_value(
    Metadata& metadata,
    std::string key,
    std::span<const std::byte> value) {
  if (auto status = validate_key(key); !status) {
    return status;
  }
  if (metadata.contains(key)) {
    return fail(
        ErrorCode::invalid_schema,
        Operation::validate_schema,
        "duplicate metadata key");
  }
  try {
    metadata.emplace(
        std::move(key),
        std::vector<std::byte>(value.begin(), value.end()));
    return {};
  } catch (const std::bad_alloc&) {
    return fail(
        ErrorCode::allocation_failed,
        Operation::validate_schema,
        "unable to copy metadata");
  }
}

}  // namespace

class Field::Impl {
 public:
  Impl(
      std::uint32_t id_value,
      std::string name_value,
      LogicalType type_value,
      bool nullable_value,
      Metadata metadata_value)
      : id(id_value),
        name(std::move(name_value)),
        type(std::move(type_value)),
        nullable(nullable_value),
        metadata(std::move(metadata_value)) {}

  std::uint32_t id;
  std::string name;
  LogicalType type;
  bool nullable;
  Metadata metadata;
};

class FieldBuilder::Impl {
 public:
  Impl(
      std::uint32_t id_value,
      std::string name_value,
      LogicalType type_value,
      bool nullable_value)
      : id(id_value),
        name(std::move(name_value)),
        type(std::move(type_value)),
        nullable(nullable_value) {}

  std::uint32_t id;
  std::string name;
  LogicalType type;
  bool nullable;
  Metadata metadata;
  bool finished = false;
};

Field::Field(std::shared_ptr<const Impl> impl) noexcept
    : impl_(std::move(impl)) {}
Field::~Field() = default;
Field::Field(const Field&) = default;
Field& Field::operator=(const Field&) = default;
Field::Field(Field&&) noexcept = default;
Field& Field::operator=(Field&&) noexcept = default;
std::uint32_t Field::id() const noexcept { return impl_->id; }
std::string_view Field::name() const noexcept { return impl_->name; }
const LogicalType& Field::type() const noexcept { return impl_->type; }
bool Field::nullable() const noexcept { return impl_->nullable; }
std::size_t Field::metadata_count() const noexcept {
  return impl_->metadata.size();
}
std::optional<std::span<const std::byte>> Field::metadata(
    std::string_view key) const noexcept {
  const auto found = impl_->metadata.find(key);
  if (found == impl_->metadata.end()) {
    return std::nullopt;
  }
  return std::span<const std::byte>(found->second);
}

FieldBuilder::FieldBuilder(
    std::uint32_t id,
    std::string name,
    LogicalType type,
    bool nullable)
    : impl_(std::make_unique<Impl>(
          id,
          std::move(name),
          std::move(type),
          nullable)) {}
FieldBuilder::~FieldBuilder() = default;
FieldBuilder::FieldBuilder(FieldBuilder&&) noexcept = default;
FieldBuilder& FieldBuilder::operator=(FieldBuilder&&) noexcept = default;

Status FieldBuilder::add_metadata(
    std::string key,
    std::span<const std::byte> value) {
  if (impl_->finished) {
    return fail(
        ErrorCode::invalid_state,
        Operation::validate_schema,
        "field builder is already finished");
  }
  return add_metadata_value(impl_->metadata, std::move(key), value);
}

Result<Field> FieldBuilder::finish(const ResourceLimits& limits) && {
  if (impl_->finished) {
    return fail(
        ErrorCode::invalid_state,
        Operation::validate_schema,
        "field builder is already finished");
  }
  impl_->finished = true;
  if (impl_->id == 0 || impl_->name.empty() || !valid_utf8(impl_->name)) {
    return fail(
        ErrorCode::invalid_schema,
        Operation::validate_schema,
        "field ID must be nonzero and name must be nonempty strict UTF-8");
  }
  std::uint64_t total = static_cast<std::uint64_t>(impl_->name.size());
  if (auto status = validate_metadata(impl_->metadata, limits, total);
      !status) {
    return std::unexpected(std::move(status.error()));
  }
  try {
    return Field(std::make_shared<const Field::Impl>(
        impl_->id,
        std::move(impl_->name),
        std::move(impl_->type),
        impl_->nullable,
        std::move(impl_->metadata)));
  } catch (const std::bad_alloc&) {
    return fail(
        ErrorCode::allocation_failed,
        Operation::validate_schema,
        "unable to allocate immutable field");
  }
}

class Schema::Impl {
 public:
  Impl(
      std::vector<Field> fields_value,
      Metadata metadata_value,
      std::vector<std::byte> canonical_value)
      : fields(std::move(fields_value)),
        metadata(std::move(metadata_value)),
        canonical(std::move(canonical_value)),
        fingerprint_value(make_fingerprint(canonical)) {}

  std::vector<Field> fields;
  Metadata metadata;
  std::vector<std::byte> canonical;
  std::array<std::byte, 16> fingerprint_value;
};

class SchemaBuilder::Impl {
 public:
  std::vector<Field> fields;
  Metadata metadata;
  bool finished = false;
};

Schema::Schema(std::shared_ptr<const Impl> impl) noexcept
    : impl_(std::move(impl)) {}
Schema::~Schema() = default;
Schema::Schema(const Schema&) = default;
Schema& Schema::operator=(const Schema&) = default;
Schema::Schema(Schema&&) noexcept = default;
Schema& Schema::operator=(Schema&&) noexcept = default;
std::size_t Schema::field_count() const noexcept { return impl_->fields.size(); }
const Field& Schema::field(std::size_t index) const {
  return impl_->fields.at(index);
}
const Field* Schema::find_field(std::uint32_t id) const noexcept {
  const auto found = std::ranges::find(
      impl_->fields,
      id,
      &Field::id);
  return found == impl_->fields.end() ? nullptr : &*found;
}
std::size_t Schema::metadata_count() const noexcept {
  return impl_->metadata.size();
}
std::optional<std::span<const std::byte>> Schema::metadata(
    std::string_view key) const noexcept {
  const auto found = impl_->metadata.find(key);
  if (found == impl_->metadata.end()) {
    return std::nullopt;
  }
  return std::span<const std::byte>(found->second);
}
std::span<const std::byte> Schema::canonical_bytes() const noexcept {
  return impl_->canonical;
}
std::array<std::byte, 16> Schema::fingerprint() const noexcept {
  return impl_->fingerprint_value;
}

SchemaBuilder::SchemaBuilder() : impl_(std::make_unique<Impl>()) {}
SchemaBuilder::~SchemaBuilder() = default;
SchemaBuilder::SchemaBuilder(SchemaBuilder&&) noexcept = default;
SchemaBuilder& SchemaBuilder::operator=(SchemaBuilder&&) noexcept = default;

Status SchemaBuilder::add_field(Field field) {
  if (impl_->finished) {
    return fail(
        ErrorCode::invalid_state,
        Operation::validate_schema,
        "schema builder is already finished");
  }
  try {
    impl_->fields.push_back(std::move(field));
    return {};
  } catch (const std::bad_alloc&) {
    return fail(
        ErrorCode::allocation_failed,
        Operation::validate_schema,
        "unable to retain field");
  }
}

Status SchemaBuilder::add_metadata(
    std::string key,
    std::span<const std::byte> value) {
  if (impl_->finished) {
    return fail(
        ErrorCode::invalid_state,
        Operation::validate_schema,
        "schema builder is already finished");
  }
  return add_metadata_value(impl_->metadata, std::move(key), value);
}

Result<Schema> SchemaBuilder::finish(const ResourceLimits& limits) && {
  if (impl_->finished) {
    return fail(
        ErrorCode::invalid_state,
        Operation::validate_schema,
        "schema builder is already finished");
  }
  impl_->finished = true;
  if (auto status = validate_resource_limits(limits); !status) {
    return std::unexpected(std::move(status.error()));
  }
  if (impl_->fields.empty() ||
      impl_->fields.size() > limits.maximum_fields) {
    return fail(
        impl_->fields.empty() ? ErrorCode::invalid_schema
                              : ErrorCode::limit_exceeded,
        Operation::validate_schema,
        "schema field count is outside the configured bounds");
  }

  std::uint64_t metadata_bytes = 0;
  if (auto status =
          validate_metadata(impl_->metadata, limits, metadata_bytes);
      !status) {
    return std::unexpected(std::move(status.error()));
  }
  for (std::size_t index = 0; index < impl_->fields.size(); ++index) {
    const auto& field = impl_->fields[index];
    if (std::ranges::any_of(
            impl_->fields.begin(),
            impl_->fields.begin() + static_cast<std::ptrdiff_t>(index),
            [&](const Field& prior) {
              return prior.id() == field.id() ||
                     prior.name() == field.name();
            })) {
      return fail(
          ErrorCode::invalid_schema,
          Operation::validate_schema,
          "schema field IDs and top-level names must be unique");
    }
    auto field_size = checked_add(
        metadata_bytes,
        static_cast<std::uint64_t>(field.name().size()),
        Operation::validate_schema,
        "schema metadata bytes");
    if (!field_size) {
      return std::unexpected(std::move(field_size.error()));
    }
    metadata_bytes = *field_size;
    auto own_metadata = metadata_size(field.impl_->metadata);
    if (!own_metadata) {
      return std::unexpected(std::move(own_metadata.error()));
    }
    auto total = checked_add(
        metadata_bytes,
        *own_metadata,
        Operation::validate_schema,
        "schema metadata bytes");
    if (!total) {
      return std::unexpected(std::move(total.error()));
    }
    metadata_bytes = *total;
    if (metadata_bytes > limits.maximum_metadata_bytes) {
      return fail(
          ErrorCode::limit_exceeded,
          Operation::validate_schema,
          "schema metadata exceeds the configured byte limit");
    }
  }

  try {
    std::vector<std::byte> canonical;
    canonical.insert(
        canonical.end(),
        {std::byte{'S'}, std::byte{'C'}, std::byte{'H'}, std::byte{1}});
    append_integer(
        canonical,
        static_cast<std::uint32_t>(impl_->fields.size()));
    for (const auto& field : impl_->fields) {
      append_integer(canonical, field.id());
      append_string(canonical, field.name());
      append_type(canonical, field.type());
      canonical.push_back(
          field.nullable() ? std::byte{1} : std::byte{0});
      append_metadata(canonical, field.impl_->metadata);
    }
    append_metadata(canonical, impl_->metadata);
    return Schema(std::make_shared<const Schema::Impl>(
        std::move(impl_->fields),
        std::move(impl_->metadata),
        std::move(canonical)));
  } catch (const std::bad_alloc&) {
    return fail(
        ErrorCode::allocation_failed,
        Operation::validate_schema,
        "unable to allocate immutable schema");
  }
}

}  // namespace shibori::engine
