#include <meta>
#include <print>
#include <string>
#include <vector>
#include <ranges>
#include <optional>
#include <spanstream>

namespace clap {

struct ShortArg {};
inline constexpr ShortArg Short{};

struct LongArg {};
inline constexpr LongArg Long{};

struct HelpArg { const char* desc; };
consteval auto Help(std::string_view desc) -> HelpArg {
  return {std::define_static_string(desc)};
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

template <typename Args>
  requires std::is_aggregate_v<Args>
void print_options() {
  std::println("options:");
  template for (constexpr auto member : nsdm_of(^^Args)) {
    constexpr auto name = std::meta::identifier_of(member);
    constexpr auto has_long = has_annotation<LongArg>(member);
    constexpr auto has_short = has_annotation<ShortArg>(member);
    constexpr auto has_help = has_annotation<HelpArg>(member);

    if constexpr (has_long and has_short) {
      std::print("  -{} --{}", name[0], name);
    } else if constexpr (has_long) {
      std::print("  --{}", name);
    } else if constexpr (has_short) {
      std::print("  -{}", name[0]);
    } else {
      continue;
    }

    if constexpr (has_help) {
      constexpr auto help = fetch_annotation<HelpArg>(member);
      if constexpr (help) std::println(" {}", help->desc);
    } else {
      std::println();
    }
  }
}

template <typename Args>
  requires std::is_aggregate_v<Args>
auto parse_args(int argc, char* argv[]) -> Args {
  Args args{};
  std::string_view cmd{argv[0]};
  std::vector<std::string_view> cmdline{argv + 1, argv + argc};

  if (std::ranges::find_if(cmdline, [](std::string_view arg) {
    return arg == "--help" or arg == "-h";
  }) != cmdline.end()) {
    std::println("Usage: {} [options]", cmd);
    print_options<Args>();
    std::exit(EXIT_SUCCESS);
  }

  template for (constexpr auto member : nsdm_of(^^Args)) {
    constexpr auto type = std::meta::type_of(member);
    constexpr auto name = std::meta::identifier_of(member);
    constexpr auto has_long = has_annotation<LongArg>(member);
    constexpr auto has_short = has_annotation<ShortArg>(member);

    auto it = cmdline.end();
    if constexpr (has_long) {
      if (it == cmdline.end()) {
        it = std::ranges::find_if(cmdline, [&](std::string_view arg) {
          return arg.starts_with("--") and arg.substr(2) == name;
        });
      } else {
        std::println(stderr, "Duplicate option --{} and {}", name, *it);
        std::exit(EXIT_FAILURE);
      }
    }
    if constexpr (has_short) {
      if (it == cmdline.end()) {
        it = std::ranges::find_if(cmdline, [&](std::string_view arg) {
          return arg[0] == '-' and arg[1] == name[0];
        });
      } else {
        std::println(stderr, "Duplicate option -{} and {}", name[0], *it);
        std::exit(EXIT_FAILURE);
      }
    }

    if constexpr (type == ^^bool) {
      if (it != cmdline.end()) {
        args.[:member:] = true;
      }
    } else {
      if (it == cmdline.end()) {
        continue;
      } else if (it + 1 == cmdline.end()) {
        std::println(stderr, "Option {} need an argument", *it);
        std::exit(EXIT_FAILURE);
      }

      std::ispanstream iss{it[1]};
      if (iss >> args.[:member:]; !iss) {
        std::println(stderr, "Failed to parse {} into option {} of type {}",
          it[1], it[0], std::meta::display_string_of(type));
        std::exit(EXIT_FAILURE);
      }
    }
  }

  return args;
}

}  // namespace clap

// clang-format off
struct Args {
  [[=clap::Long, =clap::Help("LLM endpoint of openai format")]]
  std::string base_url;

  [[=clap::Long, =clap::Help("Api key for your provider")]]
  std::string api_key;

  [[=clap::Long, =clap::Help("Max iteration")]]
  int max_iter{5};

  [[=clap::Short, =clap::Long, =clap::Help("Timeout in seconds")]]
  double timeout{30.0};

  [[=clap::Short, =clap::Long, =clap::Help("Enable stream mode")]]
  bool stream{};
};

// clang-format on
int main(int argc, char* argv[]) {
  auto args = clap::parse_args<Args>(argc, argv);
  std::println("base_url = {}", args.base_url);
  std::println("api_key = {}", args.api_key);
  std::println("max_iter = {}", args.max_iter);
  std::println("timeout = {}", args.timeout);
  std::println("stream = {}", args.stream);
}