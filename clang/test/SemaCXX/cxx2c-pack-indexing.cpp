// RUN: %clang_cc1 -std=c++2c -verify %s

struct NotAPack;
template <typename T, auto V, template<typename> typename Tp>
void not_pack() {
    int i = 0;
    i...[0]; // expected-error {{'i' does not refer to the name of a parameter pack}}
    V...[0]; // expected-error {{'V' does not refer to the name of a parameter pack}}
    NotAPack...[0] a; // expected-error{{'NotAPack' does not refer to the name of a parameter pack}}
    T...[0] b;   // expected-error{{'T' does not refer to the name of a parameter pack}}
    Tp...[0] c; // expected-error{{'Tp' does not refer to the name of a parameter pack}}
}

template <typename T, auto V, template<typename> typename Tp>
void not_pack_arrays() {
    NotAPack...[0] a[1]; // expected-error{{'NotAPack' does not refer to the name of a parameter pack}}
    T...[0] b[1];   // expected-error{{'T' does not refer to the name of a parameter pack}}
    Tp...[0] c[1]; // expected-error{{'Tp' does not refer to the name of a parameter pack}}
}

template <typename T>
struct TTP;

void test_errors() {
    not_pack<int, 0, TTP>();
    not_pack_arrays<int, 0, TTP>();
}

namespace invalid_indexes {

int non_constant_index(); // expected-note 2{{declared here}}

template <int idx>
int params(auto... p) {
    return p...[idx]; // #error-param-size
}

template <auto N, typename...T>
int test_types() {
    T...[N] a; // #error-type-size
}

void test() {
    params<0>();   // expected-note{{here}} \
                   // expected-error@#error-param-size {{invalid index 0 for pack 'p' of size 0}}
    params<1>(0);  // expected-note{{here}} \
                   // expected-error@#error-param-size {{invalid index 1 for pack 'p' of size 1}}
    params<-1>(0); // expected-note{{here}} \
                   // expected-error@#error-param-size {{invalid index -1 for pack 'p' of size 1}}

    test_types<-1>(); //expected-note {{in instantiation}} \
                      // expected-error@#error-type-size {{invalid index -1 for pack 'T' of size 0}}
    test_types<-1, int>(); //expected-note {{in instantiation}} \
                      // expected-error@#error-type-size {{invalid index -1 for pack 'T' of size 1}}
    test_types<0>(); //expected-note {{in instantiation}} \
                    // expected-error@#error-type-size {{invalid index 0 for pack 'T' of size 0}}
    test_types<1, int>(); //expected-note {{in instantiation}}  \
                         // expected-error@#error-type-size {{invalid index 1 for pack 'T' of size 1}}
}

void invalid_indexes(auto... p) {
    p...[non_constant_index()]; // expected-error {{array size is not a constant expression}}\
                                // expected-note {{cannot be used in a constant expression}}

    const char* no_index = "";
    p...[no_index]; // expected-error {{value of type 'const char *' is not implicitly convertible}}
}

void invalid_index_types() {
    []<typename... T> {
        T...[non_constant_index()] a;  // expected-error {{array size is not a constant expression}}\
                                       // expected-note {{cannot be used in a constant expression}}
    }(); //expected-note {{in instantiation}}
}

}

template <typename T, typename U>
constexpr bool is_same = false;

template <typename T>
constexpr bool is_same<T, T> = true;

template <typename T>
constexpr bool f(auto&&... p) {
    return is_same<T, decltype(p...[0])>;
}

void g() {
    int a = 0;
    const int b = 0;
    static_assert(f<int&&>(0));
    static_assert(f<int&>(a));
    static_assert(f<const int&>(b));
}

template <auto... p>
struct check_ice {
    enum e {
        x = p...[0]
    };
};

static_assert(check_ice<42>::x == 42);

struct S{};
template <auto... p>
constexpr auto constant_initializer = p...[0];
constexpr auto InitOk = constant_initializer<S{}>;

consteval int evaluate(auto... p) {
    return p...[0];
}
constexpr int x = evaluate(42, S{});
static_assert(x == 42);


namespace splice {
template <auto ... Is>
struct IL{};

template <typename ... Ts>
struct TL{};

template <typename Tl, typename Il>
struct SpliceImpl;

template <typename ... Ts, auto ...Is>
struct SpliceImpl<TL<Ts...>, IL<Is...>>{
    using type = TL<Ts...[Is]...>;
};

template <typename Tl, typename Il>
using Splice = typename SpliceImpl<Tl, Il>::type;
using type = Splice<TL<char, short, long, double>, IL<1, 2>>;
static_assert(is_same<type, TL<short, long>>);
}


namespace GH81697 {

template<class... Ts> struct tuple {
    int __x0;
};

template<auto I, class... Ts>
Ts...[I]& get(tuple<Ts...>& t) {
  return t.__x0;
}

void f() {
  tuple<int> x;
  get<0>(x);
}

}

namespace GH88929 {
    bool b = a...[0];  // expected-error {{use of undeclared identifier 'a'}}
    using E = P...[0]; // expected-error {{unknown type name 'P'}} \
                       // expected-error {{expected ';' after alias declaration}}
}

namespace GH88925 {
template <typename...> struct S {};

template <auto...> struct W {};

template <int...> struct sequence {};

template <typename... args, int... indices> auto f(sequence<indices...>) {
  return S<args...[indices]...>(); // #use
}

template <auto... args, int... indices> auto g(sequence<indices...>) {
  return W<args...[indices]...>(); // #nttp-use
}

void h() {
  static_assert(__is_same(decltype(f<int>(sequence<0, 0>())), S<int, int>));
  static_assert(__is_same(decltype(f<int, long>(sequence<0, 0>())), S<int, int>));
  static_assert(__is_same(decltype(f<int, long>(sequence<0, 1>())), S<int, long>));
  f<int, long>(sequence<3>());
  // expected-error@#use {{invalid index 3 for pack 'args' of size 2}}}
  // expected-note-re@-2 {{function template specialization '{{.*}}' requested here}}

  struct foo {};
  struct bar {};
  struct baz {};

  static_assert(__is_same(decltype(g<foo{}, bar{}, baz{}>(sequence<0, 2, 1>())), W<foo{}, baz{}, bar{}>));
  g<foo{}>(sequence<4>());
  // expected-error@#nttp-use {{invalid index 4 for pack 'args' of size 1}}
  // expected-note-re@-2 {{function template specialization '{{.*}}' requested here}}
}
}

namespace GH91885 {

void test(auto...args){
    [&]<int idx>(){
        using R = decltype( args...[idx] ) ;
    }.template operator()<0>();
}

template<int... args>
void test2(){
  [&]<int idx>(){
    using R = decltype( args...[idx] ) ; // #test2-R
  }.template operator()<0>(); // #test2-call
}

void f( ) {
  test(1);
  test2<1>();
  test2();
  // expected-error@#test2-R {{invalid index 0 for pack 'args' of size 0}}
  // expected-note@#test2-call {{requested here}}
  // expected-note@-3 {{requested here}}
}


}

namespace std {
struct type_info {
  const char *name;
};
} // namespace std

namespace GH93650 {
auto func(auto... inputArgs) { return typeid(inputArgs...[0]); }
} // namespace GH93650


namespace GH105900 {

template <typename... opts>
struct types  {
    template <unsigned idx>
    static constexpr __SIZE_TYPE__ get_index() { return idx; }

    template <unsigned s>
    static auto x() -> opts...[get_index<s>()] {}
};

template <auto... opts>
struct vars  {
    template <unsigned idx>
    static constexpr __SIZE_TYPE__ get_index() { return idx; }

    template <unsigned s>
    static auto x() -> decltype(opts...[get_index<s>()]) {return 0;}
};

void f() {
    types<void>::x<0>();
    vars<0>::x<0>();
}

} // namespace GH105900

namespace GH105903 {

template <typename... opts> struct temp {
  template <unsigned s> static auto x() -> opts... [s] {} // expected-note {{invalid index 0 for pack 'opts' of size 0}}
};

void f() {
  temp<>::x<0>(); // expected-error {{no matching}}
}

} // namespace GH105903

namespace GH116105 {

template <unsigned long Np, class... Ts> using pack_type = Ts...[Np];

template <unsigned long Np, auto... Ts> using pack_expr = decltype(Ts...[Np]);

template <class...> struct types;

template <class, long... Is> struct indices;

template <class> struct repack;

template <long... Idx> struct repack<indices<long, Idx...>> {
  template <class... Ts>
  using pack_type_alias = types<pack_type<Idx, Ts...>...>;

  template <class... Ts>
  using pack_expr_alias = types<pack_expr<Idx, Ts{}...>...>;
};

template <class... Args> struct mdispatch_ {
  using Idx = __make_integer_seq<indices, long, sizeof...(Args)>;

  static_assert(__is_same(
      typename repack<Idx>::template pack_type_alias<Args...>, types<Args...>));

  static_assert(__is_same(
      typename repack<Idx>::template pack_expr_alias<Args...>, types<Args...>));
};

mdispatch_<int, int> d;

} // namespace GH116105

namespace GH121242 {
    // Non-dependent type pack access
    template <int...x>
    int y = x...[0];

    struct X {};

    template <X...x>
    X z = x...[0];

    void foo() {
        (void)y<0>;
        (void)z<X{}>;
    }
} // namespace GH121242

namespace GH123033 {
  template <class... Types>
  requires __is_same_as(Types...[0], int)
  void print(double d);

  template <class... Types>
  requires  __is_same_as(Types...[0], int)
  void print(double d);

  template <class... Types>
  Types...[0] convert(double d);

  template <class... Types>
  Types...[0] convert(double d) {
      return static_cast<Types...[0]>(d);
  }

  void f() {
      print<int, int>(12.34);
      convert<int, int>(12.34);
  }
}
