#include <array>
#include <print>

/* List Operation */

template <typename... Elements>
struct list_t {
  static constexpr auto values()
      -> std::array<std::size_t, sizeof...(Elements)> {
    return {Elements::value...};
  }
};

// prepend
template <typename List, typename NewElement>
struct list_prepend_t;

template <typename NewElement, typename... Elements>
struct list_prepend_t<list_t<Elements...>, NewElement> {
  using value = list_t<NewElement, Elements...>;
};

template <typename List, typename NewElement>
using list_prepend_v = typename list_prepend_t<List, NewElement>::value;

// head
template <typename List>
struct head_t;

template <typename Head, typename... Rest>
struct head_t<list_t<Head, Rest...>> {
  using value = Head;
};

template <typename List>
using head_v = typename head_t<List>::value;

// drop
template <typename List>
struct drop_t;

template <typename Head, typename... Rest>
struct drop_t<list_t<Head, Rest...>> {
  using value = list_t<Rest...>;
};

template <typename List>
using drop_v = typename drop_t<List>::value;

/* Integer Wrapper */

template <std::size_t I>
struct integer_t {
  static constexpr std::size_t value = I;
};

/* Integer Range Generator */

template <std::size_t Begin, std::size_t End>
struct irange_t {
  using value = list_prepend_v<typename irange_t<Begin + 1, End>::value,
                               integer_t<Begin>>;
};

template <std::size_t I>
struct irange_t<I, I> {
  using value = list_t<integer_t<I>>;
};

template <std::size_t Begin, std::size_t End>
using irange_v = typename irange_t<Begin, End>::value;

/* If Else Statement */

template <bool Cond, typename TrueType, typename FalseType>
struct ifelse_t {
  using value = TrueType;
};

template <typename TrueType, typename FalseType>
struct ifelse_t<false, TrueType, FalseType> {
  using value = FalseType;
};

template <bool Cond, typename TrueType, typename FalseType>
using ifelse_v = typename ifelse_t<Cond, TrueType, FalseType>::value;

/* Sieve */

// sieve multiples
template <typename Prime, typename IRange>
struct sieve_multiples_t {
  using value = ifelse_v<
      head_v<IRange>::value % Prime::value == 0,
      typename sieve_multiples_t<Prime, drop_v<IRange>>::value,
      list_prepend_v<typename sieve_multiples_t<Prime, drop_v<IRange>>::value,
                     head_v<IRange>>>;
};

template <typename Prime>
struct sieve_multiples_t<Prime, list_t<>> {
  using value = list_t<>;
};

template <typename Prime, typename IRange>
using sieve_multiples_v = typename sieve_multiples_t<Prime, IRange>::value;

// sieve primes
template <typename IRange>
struct sieve_t;

template <typename IRange>
using sieve_v = typename sieve_t<IRange>::value;

template <typename Prime, typename... Rest>
struct sieve_t<list_t<Prime, Rest...>> {
  using value =
      list_prepend_v<sieve_v<sieve_multiples_v<Prime, list_t<Rest...>>>, Prime>;
};

template <>
struct sieve_t<list_t<>> {
  using value = list_t<>;
};

template <std::size_t N>
using sieve_primes_v = sieve_v<irange_v<2, N>>;

int main() {
  constexpr int LIMIT = 100;  // no more than 1024 or the compiler will complain
  auto primes = sieve_primes_v<LIMIT>::values();
  std::println("{}", primes);
}
