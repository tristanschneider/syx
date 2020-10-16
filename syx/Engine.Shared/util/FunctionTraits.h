#pragma once
//A template to deduce number and type of arguments and return type of callable types
//Used like FunctionTraits<decltype(&myFunc)>::argument<0>::type

template<class T>
struct FunctionTraits;

//Base helper
template<class Ret, class... Args>
struct FunctionTraits<Ret(Args...)> {
  using return_type = Ret;
  static constexpr size_t num_args = sizeof...(Args);

  template<size_t index>
  struct argument {
    using type = std::tuple_element_t<index, std::tuple<Args...>>;
  };
};

//Normal function
template<class Ret, class... Args>
struct FunctionTraits<Ret(*)(Args...)> : public FunctionTraits<Ret(Args...)> {
};

//Member function, note that 'this' parameter is not part of the deduced type
template<class Owner, class Ret, class... Args>
struct FunctionTraits<Ret(Owner::*)(Args...)> : public FunctionTraits<Ret(Args...)> {
};

//Const Member function, note that 'this' parameter is not part of the deduced type
template<class Owner, class Ret, class... Args>
struct FunctionTraits<Ret(Owner::*)(Args...) const> : public FunctionTraits<Ret(Args...)> {
};

//callables like std function, note that 'this' parameter is not part of the deduced type
template<class FN>
struct FunctionTraits : public FunctionTraits<decltype(&FN::operator())> {
};