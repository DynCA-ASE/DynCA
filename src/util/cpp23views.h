#ifndef _CPP23VIEWS_INCPP20_HPP
#define _CPP23VIEWS_INCPP20_HPP
#include <algorithm>
#include <concepts>
#include <functional>
#include <ranges>
#include <tuple>
#include <type_traits>
#ifndef _CPP23VIEWSIN20_NAMESPACE
  #define _CPP23VIEWSIN20_NAMESPACE cpp23
#endif
#ifndef _CPP23VIEWSIN20__RANGES_MACROS_H
#define _CPP23VIEWSIN20__RANGES_MACROS_H
#ifdef _CPP23VIEWSIN20_NAMESPACE
  #define _CPP23VIEWSIN20_NAMESPACE_BEGIN namespace _CPP23VIEWSIN20_NAMESPACE {
  #define _CPP23VIEWSIN20_NAMESPACE_END }
#else
  #define _CPP23VIEWSIN20_NAMESPACE
  #define _CPP23VIEWSIN20_NAMESPACE_BEGIN
  #define _CPP23VIEWSIN20_NAMESPACE_END
#endif
#if defined(__clang__)
  #if __clang_major__ < 16
    #warning "Prefer to use Clang >= 16"
  #endif
#elif defined(__GNUC__)
  #if __GNUC__ < 12
    #warning "Prefer to use GCC >= 12"
  #endif
#elif defined(_MSC_VER)
  #if _MSC_VER < 1931
    #warning "Prefer to use MSVC >= 19.31"
  #endif
#endif
#ifdef _MSC_VER
  #define _CPP23VIEWSIN20_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
  #define _CPP23VIEWSIN20_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif
#endif /* _CPP23VIEWSIN20__RANGES_MACROS_H */
_CPP23VIEWSIN20_NAMESPACE_BEGIN
namespace ranges {
namespace details {
template <class Range>
concept simple_view = std::ranges::view<Range> && std::ranges::range<const Range> &&
                      std::same_as<std::ranges::iterator_t<Range>, std::ranges::iterator_t<const Range>> &&
                      std::same_as<std::ranges::sentinel_t<Range>, std::ranges::sentinel_t<const Range>>;
template <class Range>
concept range_with_movable_references =
    std::ranges::input_range<Range> && std::move_constructible<std::ranges::range_reference_t<Range>> &&
    std::move_constructible<std::ranges::range_rvalue_reference_t<Range>>;
namespace __tuple_like {
template <class T> struct is_array : std::false_type {};
template <class T, size_t N> struct is_array<std::array<T, N>> : std::true_type {};
template <class T> struct is_pair : std::false_type {};
template <class T, class U> struct is_pair<std::pair<T, U>> : std::true_type {};
template <class T> struct is_tuple : std::false_type {};
template <class... Ts> struct is_tuple<std::tuple<Ts...>> : std::true_type {};
template <class T> struct is_subrange : std::false_type {};
template <class It, class St, std::ranges::subrange_kind Ki>
struct is_subrange<std::ranges::subrange<It, St, Ki>> : std::true_type {};
template <class T>
concept tuple_like_impl = is_tuple<T>::value || is_pair<T>::value || is_array<T>::value || is_subrange<T>::value;
}
template <class T>
concept tuple_like = __tuple_like::tuple_like_impl<std::remove_cvref_t<T>>;
template <class T>
concept pair_like = tuple_like<T> && std::tuple_size_v<std::remove_cvref_t<T>> == 2;
template <bool Const, class T> using maybe_const = std::conditional_t<Const, const T, T>;
struct __empty {};
template <class Fn, class Tuple> constexpr auto tuple_transform(Fn &&f, Tuple &&tuple) {
  return std::apply(
      [&]<class... Ts>(Ts &&...args) {
        return std::tuple<std::invoke_result_t<Fn &, Ts>...>(std::invoke(f, std::forward<Ts>(args))...);
      },
      std::forward<Tuple>(tuple));
}
template <class Fn, class Tuple> constexpr auto tuple_for_each(Fn &&f, Tuple &&tuple) {
  return std::apply(
      [&]<class... Ts>(Ts &&...args) { (static_cast<void>(std::invoke(f, std::forward<Ts>(args))), ...); },
      std::forward<Tuple>(tuple));
}
template <std::move_constructible F> struct __apply_fn : private F {
  __apply_fn() = default;
  constexpr explicit __apply_fn(F f) : F(std::move(f)) {}
  constexpr auto operator()(auto &&tuple) const
      -> decltype(std::apply(std::declval<const F &>(), std::declval<decltype(tuple)>())) {
    return std::apply(static_cast<const F &>(*this), std::forward<decltype(tuple)>(tuple));
  }
  constexpr auto operator()(auto &&tuple)
      -> decltype(std::apply(std::declval<F &>(), std::declval<decltype(tuple)>())) {
    return std::apply(static_cast<F &>(*this), std::forward<decltype(tuple)>(tuple));
  }
};
}
}
_CPP23VIEWSIN20_NAMESPACE_END
template <class T1, class T2, class U1, class U2>
  requires requires { typename std::pair<std::common_type_t<T1, U1>, std::common_type_t<T2, U2>>; }
struct std::common_type<std::pair<T1, T2>, std::pair<U1, U2>> {
  using type = std::pair<std::common_type_t<T1, U1>, std::common_type_t<T2, U2>>;
};
template <class... Ts, class... Us>
  requires(sizeof...(Ts) == sizeof...(Us)) && requires { typename std::tuple<std::common_type_t<Ts, Us>...>; }
struct std::common_type<std::tuple<Ts...>, std::tuple<Us...>> {
  using type = std::tuple<std::common_type_t<Ts, Us>...>;
};
_CPP23VIEWSIN20_NAMESPACE_BEGIN
namespace ranges {
template <class D>
  requires std::is_object_v<D> && std::same_as<D, std::remove_cv_t<D>>
class range_adaptor_closure {};
namespace details {
template <class Fn> struct pipeable : Fn, range_adaptor_closure<pipeable<Fn>> {
  constexpr explicit pipeable(Fn &&fn) : Fn(std::move(fn)) {}
};
template <class T> T derived_from_range_adaptor_closure(range_adaptor_closure<T> *);
template <class T>
concept RangeAdaptorClosure = !std::ranges::range<std::remove_cvref_t<T>> && requires {
  { derived_from_range_adaptor_closure((std::remove_cvref_t<T> *)nullptr) } -> std::same_as<std::remove_cvref_t<T>>;
};
}
template <std::ranges::range Range, details::RangeAdaptorClosure Closure>
  requires std::invocable<Closure, Range>
[[nodiscard]] constexpr decltype(auto)
operator|(Range &&range, Closure &&closure) noexcept(std::is_nothrow_invocable_v<Closure, Range>) {
  return std::invoke(std::forward<Closure>(closure), std::forward<Range>(range));
}
template <details::RangeAdaptorClosure Closure, class OtherClosure>
  requires(!std::ranges::range<OtherClosure>) && std::constructible_from<std::decay_t<Closure>, Closure> &&
          std::constructible_from<std::decay_t<OtherClosure>, OtherClosure>
[[nodiscard]] constexpr auto operator|(Closure &&c1, OtherClosure &&c2) noexcept(
    std::is_nothrow_constructible_v<std::decay_t<Closure>, Closure> &&
    std::is_nothrow_constructible_v<std::decay_t<OtherClosure>, OtherClosure>) {
  return details::pipeable(
      [c1 = std::forward<Closure>(c1), c2 = std::forward<OtherClosure>(c2)](std::ranges::range auto &&range) {
        return std::invoke(c2, std::invoke(c1, std::forward<decltype(range)>(range)));
      });
}
template <class OtherClosure, details::RangeAdaptorClosure Closure>
  requires(!std::ranges::range<OtherClosure>) &&
          (!details::RangeAdaptorClosure<OtherClosure>) && std::constructible_from<std::decay_t<Closure>, Closure> &&
          std::constructible_from<std::decay_t<OtherClosure>, OtherClosure>
[[nodiscard]] constexpr auto operator|(OtherClosure &&c1, Closure &&c2) noexcept(
    std::is_nothrow_constructible_v<std::decay_t<Closure>, Closure> &&
    std::is_nothrow_constructible_v<std::decay_t<OtherClosure>, OtherClosure>) {
  return details::pipeable(
      [c1 = std::forward<OtherClosure>(c1), c2 = std::forward<Closure>(c2)](std::ranges::range auto &&range) {
        return std::invoke(c2, std::invoke(c1, std::forward<decltype(range)>(range)));
      });
}
}
namespace ranges {
template <std::ranges::view V>
  requires details::range_with_movable_references<V>
class enumerate_view : public std::ranges::view_interface<enumerate_view<V>> {
  _CPP23VIEWSIN20_NO_UNIQUE_ADDRESS V base_;
public:
  template <bool Const> class iterator;
  template <bool Const> class sentinel;
  enumerate_view()
    requires std::default_initializable<V>
  = default;
  constexpr explicit enumerate_view(V v) : base_(std::move(v)) {}
  constexpr V base() const &
    requires std::copy_constructible<V>
  {
    return base_;
  }
  constexpr V base() && { return std::move(base_); }
  constexpr auto begin()
    requires(!details::simple_view<V>)
  {
    return iterator<false>(std::ranges::begin(base_), 0);
  }
  constexpr auto begin() const
    requires details::range_with_movable_references<const V>
  {
    return iterator<true>(std::ranges::begin(base_), 0);
  }
  constexpr auto end()
    requires(!details::simple_view<V>)
  {
    if constexpr (std::ranges::forward_range<V> && std::ranges::common_range<V> && std::ranges::sized_range<V>) {
      return iterator<false>(std::ranges::end(base_), std::ranges::distance(base_));
    } else {
      return sentinel<false>(std::ranges::end(base_));
    }
  }
  constexpr auto end() const
    requires details::range_with_movable_references<const V>
  {
    if constexpr (std::ranges::forward_range<V> && std::ranges::common_range<V> && std::ranges::sized_range<V>) {
      return iterator<true>(std::ranges::end(base_), std::ranges::distance(base_));
    } else {
      return sentinel<true>(std::ranges::end(base_));
    }
  }
  constexpr auto size()
    requires std::ranges::sized_range<V>
  {
    return std::ranges::size(base_);
  }
  constexpr auto size() const
    requires std::ranges::sized_range<const V>
  {
    return std::ranges::size(base_);
  }
};
enumerate_view(std::ranges::viewable_range auto &&r) -> enumerate_view<std::views::all_t<decltype(r)>>;
template <std::ranges::view V>
  requires details::range_with_movable_references<V>
template <bool Const>
class enumerate_view<V>::iterator {
  template <bool> friend class enumerate_view::sentinel;
  using Base = details::maybe_const<Const, V>;
public:
  using iterator_category = std::input_iterator_tag;
  using iterator_concept =
      std::conditional_t<std::ranges::random_access_range<Base>, std::random_access_iterator_tag,
                         std::conditional_t<std::ranges::bidirectional_range<Base>, std::bidirectional_iterator_tag,
                                            std::conditional_t<std::ranges::forward_range<Base>,
                                                               std::forward_iterator_tag, std::input_iterator_tag>>>;
  using difference_type = std::ranges::range_difference_t<Base>;
  using value_type = std::tuple<difference_type, std::ranges::range_value_t<Base>>;
private:
  using reference_type = std::tuple<difference_type, std::ranges::range_reference_t<Base>>;
  using BaseIter = std::ranges::iterator_t<Base>;
  BaseIter it_;
  difference_type pos_;
public:
  iterator() = default;
  constexpr iterator(BaseIter it, difference_type pos) : it_(std::move(it)), pos_(pos) {}
  constexpr iterator(iterator<!Const> i)
    requires Const && std::convertible_to<std::ranges::iterator_t<V>, std::ranges::iterator_t<const V>>
      : it_(std::move(i.it_)), pos_(i.pos_) {}
  constexpr const BaseIter &base() const & noexcept { return it_; }
  constexpr BaseIter base() && { return std::move(it_); }
  constexpr difference_type index() const noexcept { return pos_; }
  constexpr reference_type operator*() const { return {pos_, *it_}; }
  constexpr iterator &operator++() {
    ++it_, ++pos_;
    return *this;
  }
  constexpr void operator++(int)
    requires std::same_as<iterator_concept, std::input_iterator_tag>
  {
    ++*this;
  }
  constexpr iterator operator++(int)
    requires std::derived_from<iterator_concept, std::forward_iterator_tag>
  {
    auto tmp = *this;
    ++*this;
    return tmp;
  }
  constexpr iterator &operator--()
    requires std::derived_from<iterator_concept, std::bidirectional_iterator_tag>
  {
    --it_, --pos_;
    return *this;
  }
  constexpr iterator operator--(int)
    requires std::derived_from<iterator_concept, std::bidirectional_iterator_tag>
  {
    auto tmp = *this;
    --*this;
    return tmp;
  }
  constexpr reference_type operator[](difference_type n) const
    requires std::derived_from<iterator_concept, std::random_access_iterator_tag>
  {
    return {pos_ + n, it_[n]};
  }
  constexpr iterator &operator+=(difference_type n)
    requires std::derived_from<iterator_concept, std::random_access_iterator_tag>
  {
    it_ += n, pos_ += n;
    return *this;
  }
  constexpr iterator &operator-=(difference_type n)
    requires std::derived_from<iterator_concept, std::random_access_iterator_tag>
  {
    it_ -= n, pos_ -= n;
    return *this;
  }
  friend constexpr iterator operator+(const iterator &i, difference_type n)
    requires std::derived_from<iterator_concept, std::random_access_iterator_tag>
  {
    auto tmp = i;
    tmp += n;
    return tmp;
  }
  friend constexpr iterator operator+(difference_type n, const iterator &i)
    requires std::derived_from<iterator_concept, std::random_access_iterator_tag>
  {
    return i + n;
  }
  friend constexpr iterator operator-(const iterator &i, difference_type n)
    requires std::derived_from<iterator_concept, std::random_access_iterator_tag>
  {
    auto tmp = i;
    tmp -= n;
    return tmp;
  }
  friend constexpr difference_type operator-(const iterator &x, const iterator &y)
    requires std::derived_from<iterator_concept, std::random_access_iterator_tag>
  {
    return x.pos_ - y.pos_;
  }
  friend constexpr bool operator==(const iterator &x, const iterator &y) { return x.pos_ == y.pos_; }
  friend constexpr auto operator<=>(const iterator &x, const iterator &y) { return x.pos_ <=> y.pos_; }
  friend constexpr auto iter_move(const iterator &i) noexcept(
      noexcept(std::ranges::iter_move(i.it_)) &&
      std::is_nothrow_move_constructible_v<std::ranges::range_rvalue_reference_t<Base>>) {
    return std::tuple<difference_type, std::ranges::range_rvalue_reference_t<Base>>(
        i.pos_, std::ranges::iter_move(i.it_));
  }
};
template <std::ranges::view V>
  requires details::range_with_movable_references<V>
template <bool Const>
class enumerate_view<V>::sentinel {
  template <bool> friend class enumerate_view::iterator;
  using Base = details::maybe_const<Const, V>;
  using BaseSent = std::ranges::sentinel_t<Base>;
  BaseSent st_;
public:
  sentinel() = default;
  constexpr explicit sentinel(BaseSent s) : st_(std::move(s)) {}
  constexpr sentinel(sentinel<!Const> s)
    requires Const && std::convertible_to<std::ranges::sentinel_t<V>, std::ranges::sentinel_t<const V>>
      : st_(std::move(s.st_)) {}
  template <bool OtherConst>
  friend constexpr bool operator==(const iterator<OtherConst> &i, const sentinel &s)
    requires std::sentinel_for<BaseSent, typename iterator<OtherConst>::BaseIter>
  {
    return i.it_ == s.st_;
  }
  template <bool OtherConst>
  friend constexpr auto operator-(const iterator<OtherConst> &i, const sentinel &s)
    requires std::sized_sentinel_for<BaseSent, typename iterator<OtherConst>::BaseIter>
  {
    return i.it_ - s.st_;
  }
  template <bool OtherConst>
  friend constexpr auto operator-(const sentinel &s, const iterator<OtherConst> &i)
    requires std::sized_sentinel_for<BaseSent, typename iterator<OtherConst>::BaseIter>
  {
    return s.st_ - i.it_;
  }
};
namespace views {
namespace details::__enumerate {
struct __fn : range_adaptor_closure<__fn> {
  template <std::ranges::viewable_range Range> constexpr auto operator()(Range &&range) const {
    return enumerate_view(std::forward<Range>(range));
  }
};
}
inline constexpr auto enumerate = details::__enumerate::__fn{};
}
}
_CPP23VIEWSIN20_NAMESPACE_END
template <class V>
constexpr bool std::ranges::enable_borrowed_range<_CPP23VIEWSIN20_NAMESPACE::ranges::enumerate_view<V>> =
    std::ranges::enable_borrowed_range<V>;
_CPP23VIEWSIN20_NAMESPACE_BEGIN
namespace ranges {
namespace details {
template <class C>
constexpr bool reservable_container = std::ranges::sized_range<C> && requires(C &c, std::ranges::range_size_t<C> n) {
  c.reserve(n);
  { c.capacity() } -> std::same_as<decltype(n)>;
  { c.max_size() } -> std::same_as<decltype(n)>;
};
template <class C, class Ref>
constexpr bool container_appendable = requires(C &c, Ref ref) {
  requires(
      requires { c.emplace_back(std::forward<Ref>(ref)); } ||
      requires { c.push_back(std::forward<Ref>(ref)); } ||
      requires { c.emplace(c.end(), std::forward<Ref>(ref)); } ||
      requires { c.insert(c.end(), std::forward<Ref>(ref)); }
  );
};
template <class Ref, class C>
constexpr auto container_appender(C &c)
  requires container_appendable<C, Ref>
{
  if constexpr (requires(Ref ref) { c.emplace_back(std::forward<Ref>(ref)); }) {
    return [&](Ref ref) { c.emplace_back(std::forward<Ref>(ref)); };
  } else if constexpr (requires(Ref ref) { c.push_back(std::forward<Ref>(ref)); }) {
    return [&](Ref ref) { c.push_back(std::forward<Ref>(ref)); };
  } else if constexpr (requires(Ref ref) { c.emplace(c.end(), std::forward<Ref>(ref)); }) {
    return [&](Ref ref) { c.emplace(c.end(), std::forward<Ref>(ref)); };
  } else {
    return [&](Ref ref) { c.insert(c.end(), std::forward<Ref>(ref)); };
  }
}
template <class R, class T>
concept container_compatiable_range =
    std::ranges::input_range<R> && std::convertible_to<std::ranges::range_reference_t<R>, T>;
template <class C, std::ranges::input_range R, class... Args>
constexpr C __to(R &&r, Args &&...args)
  requires(!std::ranges::view<C>)
{
  if constexpr (!std::ranges::input_range<C> ||
                requires { requires(container_compatiable_range<R, std::ranges::range_value_t<C>>); }) {
    if constexpr (std::constructible_from<C, R, Args...>) {
      return C(std::forward<R>(r), std::forward<Args>(args)...);
    } else if constexpr (std::ranges::common_range<R> &&
                         requires {
                           requires(std::derived_from<
                                    typename std::iterator_traits<std::ranges::iterator_t<R>>::iterator_category,
                                    std::input_iterator_tag>);
                         } &&
                         std::constructible_from<C, std::ranges::iterator_t<R>, std::ranges::sentinel_t<R>, Args...>) {
      return C(std::ranges::begin(r), std::ranges::end(r), std::forward<Args>(args)...);
    } else {
      C c(std::forward<Args>(args)...);
      if constexpr (std::ranges::sized_range<R> && reservable_container<C>)
        c.reserve(static_cast<std::ranges::range_size_t<C>>(std::ranges::size(r)));
      std::ranges::for_each(r, container_appender<std::ranges::range_reference_t<R>>(c));
      return c;
    }
  } else if constexpr (__tuple_like::is_pair<std::ranges::range_value_t<C>>::value &&
                       pair_like<std::ranges::range_value_t<R>> && pair_like<std::ranges::range_reference_t<R>> &&
                       !(__tuple_like::is_pair<std::ranges::range_value_t<R>>::value &&
                         __tuple_like::is_pair<std::ranges::range_reference_t<R>>::value)) {
    return __to<C>(std::ranges::ref_view(r) | std::views::transform([](auto &&tuple) {
                     auto &&[x, y] = tuple;
                     return std::pair<std::tuple_element_t<0, std::ranges::range_value_t<R>>,
                                      std::tuple_element_t<1, std::ranges::range_value_t<R>>>{x, y};
                   }),
                   std::forward<Args>(args)...);
  } else {
    return __to<C>(std::ranges::ref_view(r) | std::views::transform([](auto &&elem) {
                     return __to<std::ranges::range_value_t<C>>(std::forward<decltype(elem)>(elem));
                   }),
                   std::forward<Args>(args)...);
  }
}
template <std::ranges::input_range R> struct __to_deduce_helper {
  using iterator_category = std::input_iterator_tag;
  using value_type = std::ranges::range_value_t<R>;
  using difference_type = std::ptrdiff_t;
  using pointer = std::add_pointer_t<std::ranges::range_reference_t<R>>;
  using reference = std::ranges::range_reference_t<R>;
  reference operator*() const;
  pointer operator->() const;
  __to_deduce_helper &operator++();
  __to_deduce_helper operator++(int);
  bool operator==(const __to_deduce_helper &) const;
};
template <template <class...> class C, std::ranges::input_range R, class... Args>
constexpr auto __to(R &&r, Args &&...args) {
  using input_iterator = __to_deduce_helper<R>;
  if constexpr (requires { C(std::declval<R>(), std::declval<Args>()...); }) {
    return __to<decltype(C(std::declval<R>(), std::declval<Args>()...))>(std::forward<R>(r),
                                                                         std::forward<Args>(args)...);
  } else if constexpr (requires {
                         C(std::declval<input_iterator>(), std::declval<input_iterator>(), std::declval<Args>()...);
                       }) {
    return __to<decltype(C(std::declval<input_iterator>(), std::declval<input_iterator>(), std::declval<Args>()...))>(
        std::forward<R>(r), std::forward<Args>(args)...);
  } else if constexpr (pair_like<typename input_iterator::value_type> &&
                       pair_like<typename input_iterator::reference> &&
                       !(__tuple_like::is_pair<typename input_iterator::value_type>::value &&
                         __tuple_like::is_pair<typename input_iterator::reference>::value)) {
    using tuple_t = typename input_iterator::value_type;
    return __to<C>(r | std::views::transform([](auto &&tuple) {
                     auto &&[x, y] = tuple;
                     return std::pair<std::tuple_element_t<0, tuple_t>, std::tuple_element_t<1, tuple_t>>{x, y};
                   }),
                   std::forward<Args>(args)...);
  } else {
    static_assert(sizeof(R) == 0, "Cannot deduce the return type of to<...>");
  }
}
}
template <class C, std::ranges::input_range R, class... Args>
constexpr C to(R &&r, Args &&...args)
  requires(!std::ranges::view<C>)
{
  return details::__to<C>(std::forward<R>(r), std::forward<Args>(args)...);
}
template <template <class...> class C, std::ranges::input_range R, class... Args>
constexpr auto to(R &&r, Args &&...args) {
  return details::__to<C>(std::forward<R>(r), std::forward<Args>(args)...);
}
template <class C, class... Args>
  requires(!std::ranges::view<C>)
constexpr auto to(Args &&...args) {
  return details::pipeable(
      [binded = std::forward_as_tuple(std::forward<Args>(args)...)](std::ranges::input_range auto &&r) {
        return [&]<size_t... Is>(std::index_sequence<Is...>) {
          return details::__to<C>(std::forward<decltype(r)>(r), std::forward<Args>(std::get<Is>(binded))...);
        }(std::make_index_sequence<sizeof...(Args)>{});
      });
}
template <template <class...> class C, class... Args> constexpr auto to(Args &&...args) {
  return details::pipeable(
      [binded = std::forward_as_tuple(std::forward<Args>(args)...)](std::ranges::input_range auto &&r) {
        return [&]<size_t... Is>(std::index_sequence<Is...>) {
          return details::__to<C>(std::forward<decltype(r)>(r), std::forward<Args>(std::get<Is>(binded))...);
        }(std::make_index_sequence<sizeof...(Args)>{});
      });
}
}
namespace ranges {
namespace details {
template <class... Rs>
static constexpr bool __is_common_zip_view =
    (sizeof...(Rs) == 1 && (std::ranges::common_range<Rs> && ...)) ||
    (!(std::ranges::bidirectional_range<Rs> && ...) && (std::ranges::common_range<Rs> && ...)) ||
    ((std::ranges::random_access_range<Rs> && ...) && (std::ranges::sized_range<Rs> && ...));
}
template <std::ranges::input_range... Rngs>
  requires(std::ranges::view<Rngs> && ...) && (sizeof...(Rngs) > 0)
class zip_view : public std::ranges::view_interface<zip_view<Rngs...>> {
  std::tuple<Rngs...> views_;
public:
  template <bool Const> class iterator;
  template <bool Const> class sentinel;
  constexpr explicit zip_view(Rngs... rngs) : views_{std::move(rngs)...} {}
  constexpr auto begin()
    requires(!(details::simple_view<Rngs> && ...))
  {
    return iterator<false>(details::tuple_transform(std::ranges::begin, views_));
  }
  constexpr auto begin() const
    requires(std::ranges::range<const Rngs> && ...)
  {
    return iterator<true>(details::tuple_transform(std::ranges::begin, views_));
  }
  constexpr auto end()
    requires(!(details::simple_view<Rngs> && ...))
  {
    if constexpr (!details::__is_common_zip_view<Rngs...>) {
      return sentinel<false>(details::tuple_transform(std::ranges::end, views_));
    } else if constexpr ((std::ranges::random_access_range<Rngs> && ...)) {
      return begin() + std::iter_difference_t<iterator<false>>(size());
    } else {
      return iterator<false>(details::tuple_transform(std::ranges::end, views_));
    }
  }
  constexpr auto end() const
    requires(std::ranges::range<const Rngs> && ...)
  {
    if constexpr (!details::__is_common_zip_view<const Rngs...>) {
      return sentinel<true>(details::tuple_transform(std::ranges::end, views_));
    } else if constexpr ((std::ranges::random_access_range<const Rngs> && ...)) {
      return begin() + std::iter_difference_t<iterator<true>>(size());
    } else {
      return iterator<true>(details::tuple_transform(std::ranges::end, views_));
    }
  }
  constexpr auto size()
    requires(std::ranges::sized_range<Rngs> && ...)
  {
    return std::apply(
        [](auto... sizes) {
          using CT = std::make_unsigned_t<std::common_type_t<decltype(sizes)...>>;
          return std::ranges::min({CT(sizes)...});
        },
        details::tuple_transform(std::ranges::size, views_));
  }
  constexpr auto size() const
    requires(std::ranges::sized_range<const Rngs> && ...)
  {
    return std::apply(
        [](auto... sizes) {
          using CT = std::make_unsigned_t<std::common_type_t<decltype(sizes)...>>;
          return std::ranges::min({CT(sizes)...});
        },
        details::tuple_transform(std::ranges::size, views_));
  }
};
zip_view(std::ranges::viewable_range auto &&...rngs) -> zip_view<std::views::all_t<decltype(rngs)>...>;
namespace details {
template <class... Rngs> struct __zip_view_iterator_category {};
template <class... Rngs>
  requires(std::ranges::forward_range<Rngs> && ...)
struct __zip_view_iterator_category<Rngs...> {
  using iterator_category = std::input_iterator_tag;
};
}
template <std::ranges::input_range... Rngs>
  requires(std::ranges::view<Rngs> && ...) && (sizeof...(Rngs) > 0)
template <bool Const>
class zip_view<Rngs...>::iterator : public details::__zip_view_iterator_category<details::maybe_const<Const, Rngs>...> {
  template <bool> friend class zip_view<Rngs...>::sentinel;
  using Iters = std::tuple<std::ranges::iterator_t<details::maybe_const<Const, Rngs>>...>;
  Iters iters_;
  static consteval auto determinate_iterator_concept() {
    if constexpr ((std::ranges::random_access_range<details::maybe_const<Const, Rngs>> && ...)) {
      return std::random_access_iterator_tag{};
    } else if constexpr ((std::ranges::bidirectional_range<details::maybe_const<Const, Rngs>> && ...)) {
      return std::bidirectional_iterator_tag{};
    } else if constexpr ((std::ranges::forward_range<details::maybe_const<Const, Rngs>> && ...)) {
      return std::forward_iterator_tag{};
    } else {
      return std::input_iterator_tag{};
    }
  }
public:
  iterator() = default;
  constexpr explicit iterator(Iters iters) : iters_(std::move(iters)) {}
  constexpr iterator(iterator<!Const> i)
    requires Const && (std::convertible_to<std::ranges::iterator_t<Rngs>, std::ranges::iterator_t<const Rngs>> && ...)
      : iters_(std::apply(
            [](std::ranges::iterator_t<Rngs> &&...is) {
              return std::make_tuple<std::ranges::iterator_t<const Rngs>...>(std::move(is)...);
            },
            std::move(i.iters_))) {}
  using iterator_concept = decltype(determinate_iterator_concept());
  using value_type = std::tuple<std::ranges::range_value_t<details::maybe_const<Const, Rngs>>...>;
  using difference_type = std::common_type_t<std::ranges::range_difference_t<details::maybe_const<Const, Rngs>>...>;
  constexpr auto operator*() const {
    return details::tuple_transform([](auto &it) -> decltype(auto) { return *it; }, iters_);
  }
  constexpr iterator &operator++() {
    details::tuple_for_each([](auto &it) { ++it; }, iters_);
    return *this;
  }
  constexpr void operator++(int)
    requires std::same_as<iterator_concept, std::input_iterator_tag>
  {
    ++*this;
  }
  constexpr iterator operator++(int)
    requires std::derived_from<iterator_concept, std::forward_iterator_tag>
  {
    auto tmp = *this;
    ++*this;
    return tmp;
  }
  constexpr iterator &operator--()
    requires std::derived_from<iterator_concept, std::bidirectional_iterator_tag>
  {
    details::tuple_for_each([](auto &it) { --it; }, iters_);
    return *this;
  }
  constexpr iterator operator--(int)
    requires std::derived_from<iterator_concept, std::bidirectional_iterator_tag>
  {
    auto tmp = *this;
    --*this;
    return tmp;
  }
  constexpr auto operator[](difference_type n) const
    requires std::derived_from<iterator_concept, std::random_access_iterator_tag>
  {
    return details::tuple_transform([&]<class I>(I &it) -> decltype(auto) { return it[std::iter_difference_t<I>(n)]; },
                                    iters_);
  }
  constexpr iterator &operator+=(difference_type n)
    requires std::derived_from<iterator_concept, std::random_access_iterator_tag>
  {
    details::tuple_for_each([&]<class I>(I &it) { it += std::iter_difference_t<I>(n); }, iters_);
    return *this;
  }
  constexpr iterator &operator-=(difference_type n)
    requires std::derived_from<iterator_concept, std::random_access_iterator_tag>
  {
    details::tuple_for_each([&]<class I>(I &it) { it -= std::iter_difference_t<I>(n); }, iters_);
    return *this;
  }
  friend constexpr iterator operator+(const iterator &i, difference_type n)
    requires std::derived_from<iterator_concept, std::random_access_iterator_tag>
  {
    auto tmp = i;
    tmp += n;
    return tmp;
  }
  friend constexpr iterator operator+(difference_type n, const iterator &i)
    requires std::derived_from<iterator_concept, std::random_access_iterator_tag>
  {
    return i + n;
  }
  friend constexpr iterator operator-(const iterator &i, difference_type n)
    requires std::derived_from<iterator_concept, std::random_access_iterator_tag>
  {
    auto tmp = i;
    tmp -= n;
    return tmp;
  }
  friend constexpr difference_type operator-(const iterator &x, const iterator &y)
    requires std::derived_from<iterator_concept, std::random_access_iterator_tag>
  {
    return [&]<size_t... Is>(std::index_sequence<Is...>) {
      return std::ranges::min({static_case<difference_type>(std::get<Is>(x.iters_) - std::get<Is>(y.iters_))...});
    }(std::make_index_sequence<sizeof...(Rngs)>{});
  }
  friend constexpr bool operator==(const iterator &x, const iterator &y)
    requires(std::equality_comparable<std::ranges::iterator_t<details::maybe_const<Const, Rngs>>> && ...)
  {
    if constexpr (std::derived_from<iterator_concept, std::bidirectional_iterator_tag>) {
      return x.iters_ == y.iters_;
    } else {
      return [&]<size_t... Is>(std::index_sequence<Is...>) -> bool {
        return ((std::get<Is>(x.iters_) == std::get<Is>(y.iters_)) || ...);
      }(std::make_index_sequence<sizeof...(Rngs)>{});
    }
  }
  friend constexpr auto operator<=>(const iterator &x, const iterator &y)
    requires std::derived_from<iterator_concept, std::random_access_iterator_tag>
  {
    return x.iters_ <=> y.iters_;
  }
  friend constexpr auto iter_move(const iterator &i) noexcept(
      (noexcept(std::ranges::iter_move(
           std::declval<const std::ranges::iterator_t<details::maybe_const<Const, Rngs>> &>())) &&
       ...) &&
      (std::is_nothrow_move_constructible_v<std::ranges::range_rvalue_reference_t<details::maybe_const<Const, Rngs>>> &&
       ...)) {
    return details::tuple_transform(std::ranges::iter_move, i.iters_);
  }
  friend constexpr void iter_swap(const iterator &x, const iterator &y) noexcept(
      (noexcept(std::ranges::iter_swap(
           std::declval<const std::ranges::iterator_t<details::maybe_const<Const, Rngs>> &>(),
           std::declval<const std::ranges::iterator_t<details::maybe_const<Const, Rngs>> &>())) &&
       ...)) {
    [&]<size_t... Is>(std::index_sequence<Is...>) {
      (std::ranges::iter_swap(std::get<Is>(x.iters_), std::get<Is>(y.iters_)), ...);
    }(std::make_index_sequence<sizeof...(Rngs)>{});
  }
};
template <std::ranges::input_range... Rngs>
  requires(std::ranges::view<Rngs> && ...) && (sizeof...(Rngs) > 0)
template <bool Const>
class zip_view<Rngs...>::sentinel {
  template <bool> friend class zip_view<Rngs...>::iterator;
  static_assert(!details::__is_common_zip_view<details::maybe_const<Const, Rngs>...>,
                "zip_view is common_range, please use iterator instead!");
  using Sents = std::tuple<std::ranges::sentinel_t<details::maybe_const<Const, Rngs>>...>;
  Sents sents_;
public:
  sentinel() = default;
  constexpr explicit sentinel(Sents sents) : sents_(std::move(sents)) {}
  constexpr sentinel(sentinel<!Const> s)
    requires Const && (std::convertible_to<std::ranges::sentinel_t<Rngs>, std::ranges::sentinel_t<const Rngs>> && ...)
      : sents_(std::apply(
            [](std::ranges::sentinel_t<Rngs> &&...ss) {
              return std::make_tuple<std::ranges::sentinel_t<const Rngs>...>(std::move(ss)...);
            },
            std::move(s.sents_))) {}
  template <bool OtherConst>
  friend constexpr bool operator==(const iterator<OtherConst> &i, const sentinel &s)
    requires(std::sentinel_for<std::ranges::sentinel_t<details::maybe_const<Const, Rngs>>,
                               std::ranges::iterator_t<details::maybe_const<OtherConst, Rngs>>> &&
             ...)
  {
    return [&]<size_t... Is>(std::index_sequence<Is...>) {
      return ((std::get<Is>(i.iters_) == std::get<Is>(s.sents_)) || ...);
    }(std::make_index_sequence<sizeof...(Rngs)>{});
  }
  template <bool OtherConst>
  friend constexpr auto operator-(const iterator<OtherConst> &i, const sentinel &s)
    requires(std::sized_sentinel_for<std::ranges::sentinel_t<details::maybe_const<Const, Rngs>>,
                                     std::ranges::iterator_t<details::maybe_const<OtherConst, Rngs>>> &&
             ...)
  {
    using CT = std::common_type_t<std::ranges::range_difference_t<details::maybe_const<OtherConst, Rngs>>...>;
    return [&]<size_t... Is>(std::index_sequence<Is...>) -> CT {
      return std::ranges::min({static_case<CT>(std::get<Is>(i.iters_) - std::get<Is>(s.sents_))...});
    }(std::make_index_sequence<sizeof...(Rngs)>{});
  }
  template <bool OtherConst>
  friend constexpr auto operator-(const sentinel &s, const iterator<OtherConst> &i)
    requires(std::sized_sentinel_for<std::ranges::sentinel_t<details::maybe_const<Const, Rngs>>,
                                     std::ranges::iterator_t<details::maybe_const<OtherConst, Rngs>>> &&
             ...)
  {
    using CT = std::common_type_t<std::ranges::range_difference_t<details::maybe_const<OtherConst, Rngs>>...>;
    return [&]<size_t... Is>(std::index_sequence<Is...>) -> CT {
      return std::ranges::min({static_case<CT>(std::get<Is>(s.sents_) - std::get<Is>(i.iters_))...});
    }(std::make_index_sequence<sizeof...(Rngs)>{});
  }
};
template <std::move_constructible F, std::ranges::input_range... Views>
  requires(std::ranges::view<Views> && ...) && (sizeof...(Views) > 0) && std::is_object_v<F> &&
          std::regular_invocable<F &, std::ranges::range_reference_t<Views>...> && requires {
            typename std::add_lvalue_reference_t<std::invoke_result_t<F &, std::ranges::range_reference_t<Views>...>>;
          }
class zip_transform_view : public std::ranges::transform_view<zip_view<Views...>, details::__apply_fn<F>> {
public:
  zip_transform_view() = default;
  constexpr zip_transform_view(F fun, Views... views)
      : std::ranges::transform_view<zip_view<Views...>, details::__apply_fn<F>>(
            zip_view<Views...>(std::move(views)...), details::__apply_fn<F>(std::move(fun))) {}
};
template <std::move_constructible F, std::ranges::viewable_range... Rs>
zip_transform_view(F &&, Rs &&...) -> zip_transform_view<std::decay_t<F>, std::views::all_t<Rs>...>;
namespace views {
namespace details::__zip {
struct __fn {
  [[nodiscard]] constexpr std::ranges::view auto operator()(std::ranges::viewable_range auto &&...rngs) const {
    if constexpr (sizeof...(rngs) == 0) {
      return std::views::empty<std::tuple<>>;
    } else {
      return zip_view(std::forward<decltype(rngs)>(rngs)...);
    }
  }
};
struct __transform_fn {
  template <class Fn, std::ranges::viewable_range... Rs>
    requires std::regular_invocable<Fn, std::ranges::range_reference_t<Rs>...>
  [[nodiscard]] constexpr std::ranges::view auto operator()(Fn &&fn, Rs &&...rngs) const {
    if constexpr (sizeof...(rngs) == 0) {
      return std::views::empty<std::invoke_result_t<std::decay_t<Fn> &>>;
    } else {
      return zip_transform_view(std::forward<Fn>(fn), std::forward<Rs>(rngs)...);
    }
  }
};
}
inline constexpr auto zip = details::__zip::__fn{};
inline constexpr auto zip_transform = details::__zip::__transform_fn{};
}
}
_CPP23VIEWSIN20_NAMESPACE_END
template <class... Vs>
constexpr bool std::ranges::enable_borrowed_range<_CPP23VIEWSIN20_NAMESPACE::ranges::zip_view<Vs...>> =
    (std::ranges::enable_borrowed_range<Vs> && ...);

_CPP23VIEWSIN20_NAMESPACE_BEGIN
namespace views = ranges::views;
_CPP23VIEWSIN20_NAMESPACE_END
#endif /* _CPP23VIEWS_INCPP20_HPP */