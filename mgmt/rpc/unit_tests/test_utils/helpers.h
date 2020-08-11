#pragma once

#define DEFINE_JSONRPC_PROTO_FUNCTION(fn) \
  void fn(std::string_view const &id, const YAML::Node &params, jsonrpc::RpcHandlerResponse &resp)

template <typename Iter, std::size_t N>
std::array<std::string, N>
chunk_impl(Iter from, Iter to)
{
  const std::size_t size = std::distance(from, to);
  if (size <= N) {
    return {std::string{from, to}};
  }
  std::size_t index{0};
  std::array<std::string, N> ret;
  const std::size_t each_part = size / N;
  const std::size_t remainder = size % N;

  for (auto it = from; it != to;) {
    if (std::size_t rem = std::distance(it, to); rem == (each_part + remainder)) {
      ret[index++] = std::string{it, it + rem};
      break;
    }
    ret[index++] = std::string{it, it + each_part};
    std::advance(it, each_part);
  }

  return ret;
}

template <std::size_t N>
auto
chunk(std::string_view v)
{
  return chunk_impl<std::string_view::const_iterator, N>(v.begin(), v.end());
}