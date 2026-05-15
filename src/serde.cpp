#include <map>
#include <meta>
#include <print>
#include <vector>
#include <string>
#include <ranges>
#include <optional>
#include <exception>
#include <spanstream>
#include <unordered_map>

namespace serde {

struct RenameTag { const char* name; };
consteval auto Rename(std::string_view name) -> RenameTag {
  return {std::define_static_string(name)};
}

consteval auto nsdm_of(std::meta::info info) {
  constexpr auto ctx = std::meta::access_context::current();
  return
    std::define_static_array(std::meta::nonstatic_data_members_of(info, ctx));
}

template <typename T>
consteval auto
fetch_annotation(std::meta::info info) -> std::optional<T> {
  for (auto anno : std::meta::annotations_of_with_type(info, ^^T)) {
    if (std::meta::remove_cvref(std::meta::type_of(anno))
        == std::meta::remove_cvref(^^T)) {
      return std::meta::extract<T>(anno);
    }
  }
  return {};
}

template <typename T>
consteval bool has_annotation(std::meta::info info) {
  return fetch_annotation<T>(info).has_value();
}

template <typename T>
struct is_strmap_t : std::false_type {};

template <typename V, typename... Args>
struct is_strmap_t<std::unordered_map<std::string, V, Args...>> : std::true_type {};

template <typename V, typename... Args>
struct is_strmap_t<std::map<std::string, V, Args...>> : std::true_type {};

template <typename T>
struct strmap_t;

template <typename T>
using strmap_value_t = typename strmap_t<T>::value_t;

template <typename V, typename... Args>
struct strmap_t<std::unordered_map<std::string, V, Args...>> {
  using value_t = V;
};

template <typename V, typename... Args>
struct strmap_t<std::map<std::string, V, Args...>> {
  using value_t = V;
};

template <typename T>
auto to_json(const T& value) -> std::string {
  using Ty = std::remove_cvref_t<T>;
  if constexpr (std::same_as<Ty, bool>) {
    bool val = static_cast<bool>(value);
    return val ? "true" : "false";
  } else if constexpr (std::integral<Ty> or std::floating_point<Ty>) {
    return std::format("{}", value);
  } else if constexpr (
    std::same_as<Ty, std::string> or std::same_as<Ty, std::string_view>) {
    std::string_view val = static_cast<std::string_view>(value);
    return std::format("\"{}\"", val);
  } else if constexpr (is_strmap_t<Ty>::value) {
    std::string builder{};
    builder.push_back('{');
    for (const auto [i, pair] : value | std::views::enumerate) {
      const auto& [key, val] = pair;
      if (i > 0) builder.push_back(',');
      builder.append(std::format("\"{}\":{}", key, to_json(val)));
    }
    builder.push_back('}');
    return builder;
  } else if constexpr (std::ranges::range<Ty>) {
    std::string builder{};
    builder.push_back('[');
    for (const auto [i, elem] : value | std::views::enumerate) {
      if (i > 0) builder.push_back(',');
      builder.append(to_json(elem));
    }
    builder.push_back(']');
    return builder;
  } else if constexpr (std::is_aggregate_v<Ty>) {
    std::string builder{};
    builder.push_back('{');
    template for (constexpr auto member : nsdm_of(^^Ty)) {
      constexpr auto name = std::meta::identifier_of(member);
      constexpr auto rename = fetch_annotation<RenameTag>(member);
      std::string_view key = rename ? rename->name : name;
      std::string val = to_json(value.[:member:]);
      if (builder.size() > 1) builder.push_back(',');
      builder.append(std::format("\"{}\":{}", key, val));
    }
    builder.push_back('}');
    return builder;
  } else {
    static_assert(false, "Unsupported type for JSON serialization");
  }
}

auto skip_whitespace(std::string_view buffer) -> std::string_view {
  std::size_t p = 0;
  for (; p < buffer.size() and std::isspace(buffer[p]); ++p);
  return buffer.substr(p);
}

template <typename T>
auto parse_json(std::string_view json) -> std::tuple<T, std::string_view>;

auto parse_bool(std::string_view buffer) -> std::tuple<bool, std::string_view> {
  if (buffer.starts_with("true")) return {true, buffer.substr(4)};
  else if (buffer.starts_with("false")) return {false, buffer.substr(5)};
  else throw std::invalid_argument{
    std::format("Expect true or false, but got '{:.5}'", buffer)};
}

template <typename T>
  requires std::integral<T> or std::floating_point<T>
auto parse_number(std::string_view buffer) -> std::tuple<T, std::string_view> {
  T value{};
  std::ispanstream iss{buffer};
  if (iss >> value; !iss)
    throw std::invalid_argument{
      std::format("Expect number, but got '{:.5}'", buffer)};
  return {value, buffer.substr(iss.tellg())};
}

auto parse_string(std::string_view buffer)
-> std::tuple<std::string, std::string_view> {
  std::string builder{};
  if (buffer[0] != '"') 
    throw std::invalid_argument{
      std::format("Expect \", but got '{}'", buffer[0])};
  buffer = buffer.substr(1);
  while (buffer.size() and buffer[0] != '"') {
    if (buffer[0] == '\\') {
      buffer = buffer.substr(1);
      if (buffer.empty())
        throw std::invalid_argument{std::format("Invalid backslash")};
      switch (buffer[0]) {
      case '0': builder.push_back('\0'); break;
      case 't': builder.push_back('\t'); break;
      case 'n': builder.push_back('\n'); break;
      case 'r': builder.push_back('\r'); break;
      case '\\': builder.push_back('\\'); break;
      default:
        throw std::invalid_argument{
          std::format("Unsupported escaped code '\\{}'", buffer[0])};
      }
    } else {
      builder.push_back(buffer[0]);
    }
    buffer = buffer.substr(1);
  }
  if (buffer.empty())
    throw std::invalid_argument{std::format("Unclosed string")};
  buffer = buffer.substr(1);
  return {builder, buffer};
}

template <typename R, typename T = std::ranges::range_value_t<R>>
  requires std::ranges::range<R> and
           std::constructible_from<R, std::initializer_list<T>> and
           std::default_initializable<std::vector<T>>
auto parse_array(std::string_view buffer) -> std::tuple<R, std::string_view> {
  std::vector<T> vals{};
  if (buffer[0] != '[') 
    throw std::invalid_argument{
      std::format("Expect [, but got '{}'", buffer[0])};
  buffer = buffer.substr(1);
  while (buffer.size() and buffer[0] != ']') {
    buffer = skip_whitespace(buffer);
    if (buffer.empty()) break;
    if (vals.size()) {
      if (buffer[0] != ',')
        throw std::invalid_argument{
          std::format("Expect ',', but got '{}'", buffer[0])};
      else
        buffer = buffer.substr(1);
    }
    buffer = skip_whitespace(buffer);
    if (buffer.empty()) break;
    auto [val, buf] = parse_json<T>(buffer);
    vals.push_back(val);
    buffer = buf;
  }
  if (buffer.empty()) 
    throw std::invalid_argument{std::format("Unclosed array")};
  buffer = buffer.substr(1);
  R value{vals};
  return {value, buffer};
}

template <typename T>
  requires (std::is_aggregate_v<T> and std::default_initializable<T>) or
           is_strmap_t<T>::value
auto parse_object(std::string_view buffer) -> std::tuple<T, std::string_view> {
  T value{};
  if (buffer[0] != '{') 
    throw std::invalid_argument{
      std::format("Expect {{, but got '{}'", buffer[0])};
  buffer = buffer.substr(1);
  bool is_first = true;
  while (buffer.size() && buffer[0] != '}') {
    buffer = skip_whitespace(buffer);
    if (buffer.empty()) break;
    if (!is_first) {
      if (buffer[0] != ',')
        throw std::invalid_argument{
          std::format("Expect ',', but got '{}'", buffer[0])};
      else
        buffer = buffer.substr(1);
    }
    buffer = skip_whitespace(buffer);
    if (buffer.empty()) break;
    
    auto [key, kbuf] = parse_string(buffer);
    buffer = kbuf;

    buffer = skip_whitespace(buffer);
    if (buffer.empty()) break;
    if (buffer[0] != ':')
      throw std::invalid_argument{
        std::format("Expect :, but got '{}'", buffer[0])};
    else
      buffer = buffer.substr(1);
    buffer = skip_whitespace(buffer);
    if (buffer.empty()) break;

    if constexpr (is_strmap_t<T>::value) {
      using V = strmap_value_t<T>;
      auto [val, vbuf] = parse_json<V>(buffer);
      buffer = vbuf;
      value.insert({key, val});
    } else {
      template for (constexpr auto member : nsdm_of(^^T)) {
        constexpr auto type = std::meta::type_of(member);
        constexpr auto name = std::meta::identifier_of(member);
        constexpr auto rename = fetch_annotation<RenameTag>(member);
        std::string_view fname = rename ? rename->name : name;
        using V = [:type:];
        if (fname == key) {
          auto [val, vbuf] = parse_json<V>(buffer);
          buffer = vbuf;
          value.[:member:] = val;
        }
      }
    }
    is_first = false;
  }
  if (buffer.empty()) 
    throw std::invalid_argument{std::format("Unclosed object")};
  buffer = buffer.substr(1);
  return {value, buffer};
}

template <typename T>
auto parse_json(std::string_view buffer) -> std::tuple<T, std::string_view> {
  using Ty = std::remove_cvref_t<T>;
  buffer = skip_whitespace(buffer);
  if constexpr (std::same_as<Ty, bool>) {
    return parse_bool(buffer);
  } else if constexpr (std::integral<Ty> or std::floating_point<Ty>) {
    return parse_number<Ty>(buffer);
  } else if constexpr (
    std::same_as<Ty, std::string> or std::same_as<Ty, std::string_view>) {
    return parse_string(buffer);
  } else if constexpr (is_strmap_t<Ty>::value) {
    return parse_object<Ty>(buffer);
  } else if constexpr (std::ranges::range<Ty>) {
    return parse_array<Ty>(buffer);
  } else if constexpr (std::is_aggregate_v<Ty>) {
    return parse_object<Ty>(buffer);
  } else {
    static_assert(false, "Unsupported type for JSON deserialization");
  }
}

template <typename T>
auto from_json(std::string_view json) -> T {
  auto [value, _] = parse_json<T>(json);
  return value;
}

}  // namespace serde

struct Vec2 {
  float x, y;
};

struct Entity {
  int id;
  std::string name;
  std::vector<std::string> tags;
  std::unordered_map<std::string, int> links;

  [[=serde::Rename("position")]]
  Vec2 pos;

  [[=serde::Rename("velocity")]]
  Vec2 vel;

  [[=serde::Rename("acceleration")]]
  Vec2 accel;
};

int main() {
  Entity entity{
    1, "Player", {"player", "character", "human"},
    {{ "link1", 10 }, { "link2", 20 }},
    {0.0f, 0.0f}, {1.0f, 1.0f}, {0.5f, 0.5f}
  };

  std::string json = serde::to_json(entity);
  std::println("Serialize:");
  std::println("{}", json);

  Entity dentity = serde::from_json<Entity>(json);
  std::println("Deserialize:");
  std::println("id = {}", dentity.id);
  std::println("name = {}", dentity.name);
  std::println("pos = ({}, {})", dentity.pos.x, dentity.pos.y);
  std::println("vel = ({}, {})", dentity.vel.x, dentity.vel.y);
  std::println("accel = ({}, {})", dentity.accel.x, dentity.accel.y);
}