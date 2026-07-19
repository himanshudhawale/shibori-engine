#include <shibori/engine/checked_arithmetic.hpp>
#include <shibori/engine/resource.hpp>

#include <cstdint>
#include <iostream>
#include <limits>
#include <utility>

namespace {

bool expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

bool arithmetic_tests() {
  using namespace shibori::engine;
  constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();

  const auto sum = checked_add(maximum - 1, 1, Operation::parse, "record size");
  const auto sum_overflow =
      checked_add(maximum, 1, Operation::parse, "record size");
  const auto product =
      checked_multiply(maximum / 2, 2, Operation::parse, "column bytes");
  const auto product_overflow = checked_multiply(
      (maximum / 2) + 1,
      2,
      Operation::parse,
      "column bytes");
  const auto cast = checked_cast<std::uint8_t>(
      std::uint16_t{255},
      Operation::parse,
      "field count");
  const auto cast_overflow = checked_cast<std::uint8_t>(
      std::uint16_t{256},
      Operation::parse,
      "field count");
  const auto signed_cast = checked_cast<std::uint32_t>(
      std::int32_t{-1},
      Operation::parse,
      "row count");

  return expect(sum && *sum == maximum, "Checked add rejected its boundary") &&
         expect(
             !sum_overflow &&
                 sum_overflow.error().code() ==
                     ErrorCode::arithmetic_overflow,
             "Checked add accepted overflow") &&
         expect(
             product && *product == maximum - 1,
             "Checked multiply rejected its boundary") &&
         expect(
             !product_overflow,
             "Checked multiply accepted overflow") &&
         expect(cast && *cast == 255, "Checked cast rejected its boundary") &&
         expect(!cast_overflow, "Checked cast accepted a wide value") &&
         expect(!signed_cast, "Checked cast accepted a negative value") &&
         expect(
             checked_range(4, 6, 10, Operation::parse, "chunk").has_value(),
             "Checked range rejected its boundary") &&
         expect(
             !checked_range(4, 7, 10, Operation::parse, "chunk"),
             "Checked range accepted an oversized length") &&
         expect(
             !checked_range(maximum, 1, maximum, Operation::parse, "chunk"),
             "Checked range accepted overflowing end");
}

bool limit_tests() {
  using namespace shibori::engine;

  const auto defaults = default_resource_limits();
  if (!expect(
          validate_resource_limits(defaults).has_value(),
          "Default resource limits are invalid")) {
    return false;
  }

  auto limits = defaults;
  limits.maximum_resident_bytes = 0;
  if (!expect(
          !validate_resource_limits(limits),
          "Zero resident limit was treated as unlimited")) {
    return false;
  }

  limits = defaults;
  limits.maximum_block_rows = 0;
  if (!expect(!validate_resource_limits(limits), "Zero row limit was accepted")) {
    return false;
  }

  limits = defaults;
  limits.maximum_decoded_block_bytes = 0;
  if (!expect(
          !validate_resource_limits(limits),
          "Zero decoded-block limit was accepted")) {
    return false;
  }

  limits = defaults;
  limits.maximum_record_bytes = 0;
  if (!expect(
          !validate_resource_limits(limits),
          "Zero record limit was accepted")) {
    return false;
  }

  limits = defaults;
  limits.maximum_metadata_bytes = 0;
  if (!expect(
          !validate_resource_limits(limits),
          "Zero metadata limit was accepted")) {
    return false;
  }

  limits = defaults;
  limits.maximum_fields = 0;
  if (!expect(!validate_resource_limits(limits), "Zero field limit was accepted")) {
    return false;
  }

  limits = defaults;
  limits.maximum_encoding_depth = 0;
  if (!expect(
          !validate_resource_limits(limits),
          "Zero encoding depth was accepted")) {
    return false;
  }

  limits = defaults;
  limits.maximum_workers = 0;
  if (!expect(
          !validate_resource_limits(limits),
          "Zero worker limit was accepted")) {
    return false;
  }

  limits = defaults;
  limits.maximum_queued_blocks = 0;
  return expect(
      !validate_resource_limits(limits),
      "Zero queue limit was accepted");
}

bool reservation_tests() {
  using namespace shibori::engine;

  auto root_result =
      ResourceBudget::create_root(ResourceKind::resident_memory, 200);
  if (!expect(root_result.has_value(), "Root budget creation failed")) {
    return false;
  }
  auto root = std::move(*root_result);

  {
    auto operation_result = root.create_child(120, Operation::configure);
    if (!expect(operation_result.has_value(), "Child budget creation failed")) {
      return false;
    }
    auto operation = std::move(*operation_result);

    if (!expect(root.used() == 120, "Parent did not fund child exactly once")) {
      return false;
    }

    auto failed_sibling = root.create_child(100, Operation::configure);
    if (!expect(
            !failed_sibling && root.used() == 120,
            "Failed child changed parent usage")) {
      return false;
    }

    auto block_result = operation.create_child(70, Operation::validate_block);
    if (!expect(block_result.has_value(), "Nested child creation failed")) {
      return false;
    }
    auto block = std::move(*block_result);

    auto reservation_result = block.reserve(70, Operation::decode);
    if (!expect(
            reservation_result.has_value() && block.used() == 70,
            "Boundary reservation failed")) {
      return false;
    }

    auto reservation = std::move(*reservation_result);
    auto moved = std::move(reservation);
    if (!expect(
            !reservation.active() && moved.active() && block.used() == 70,
            "Moving a reservation changed accounting")) {
      return false;
    }

    moved.release();
    moved.release();
    if (!expect(block.used() == 0, "Reservation did not release exactly once")) {
      return false;
    }

    auto failed = block.reserve(71, Operation::decode);
    if (!expect(
            !failed && block.used() == 0,
            "Failed reservation consumed capacity")) {
      return false;
    }
  }

  if (!expect(root.used() == 0, "Scope exit did not release child capacity")) {
    return false;
  }

  {
    auto cancelled = root.reserve(25, Operation::decode);
    if (!expect(cancelled.has_value(), "Cancellation reservation failed")) {
      return false;
    }
  }

  return expect(
      root.used() == 0,
      "Cancellation-style scope exit leaked a reservation");
}

}  // namespace

int main() {
  return arithmetic_tests() && limit_tests() && reservation_tests() ? 0 : 1;
}
