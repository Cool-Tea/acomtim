#include <meta>
#include <print>
#include <string>
#include <vector>
#include <ranges>
#include <spanstream>

namespace clap {

struct ShortArg {};
inline constexpr ShortArg Short{};

struct LongArg {};
inline constexpr LongArg Long{};

struct Help {
  const char* desc{};
  constexpr Help(std::string_view desc) : desc(desc.data()) {}
};

template <typename Args>
  requires std::is_aggregate_v<Args>
void print_options() {
  std::println("options:");
  constexpr auto ctx = std::meta::access_context::current();
  template for (constexpr auto member
    : std::define_static_array(std::meta::nonstatic_data_members_of(^^Args, ctx))) {
    constexpr auto name = std::meta::identifier_of(member);
    constexpr auto has_long =
      !std::meta::annotations_of_with_type(member, ^^LongArg).empty();
    constexpr auto has_short =
      !std::meta::annotations_of_with_type(member, ^^ShortArg).empty();
    constexpr auto has_help =
      !std::meta::annotations_of_with_type(member, ^^Help).empty();

    if constexpr (has_long and has_short) {
      std::print("  --{} -{}", name, name[0]);
    } else if constexpr (has_long) {
      std::print("  --{}", name);
    } else if constexpr (has_short) {
      std::print("  -{}", name[0]);
    }

    if constexpr (has_help) {
      constexpr auto anno = std::meta::annotations_of_with_type(member, ^^Help)[0];
      // constexpr auto type = std::meta::type_of(anno);
      // static_assert(type == ^^const Help);

      // Currently gcc 16.1.0 haven't fully support the extraction of annotations.
      // Or we can do the following:
      //   constexpr auto help = std::meta::extract<Help>(anno);
      //   std::println(" {}", help.desc);
      std::println();
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

  constexpr auto ctx = std::meta::access_context::current();
  template for (constexpr auto member
    : std::define_static_array(std::meta::nonstatic_data_members_of(^^Args, ctx))) {
    constexpr auto type = std::meta::type_of(member);
    constexpr auto name = std::meta::identifier_of(member);

    if constexpr (!std::meta::annotations_of_with_type(member, ^^LongArg).empty()) {
      auto it = std::ranges::find_if(cmdline, [&](std::string_view arg) {
        return arg.starts_with("--") && arg.substr(2) == name;
      });
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
    
    if constexpr (
      !std::meta::annotations_of_with_type(member, ^^ShortArg).empty()) {
      auto it = std::ranges::find_if(cmdline, [&](std::string_view arg) {
        return arg[0] == '-' && arg[1] == name[0];
      });
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

  [[=clap::Long, =clap::Help("Timeout in seconds")]]
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