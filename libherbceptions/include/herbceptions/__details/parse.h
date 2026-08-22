#pragma once
namespace std
{
enum class parse_errc:
	::std::uint_least32_t
{
ok=0,
end_of_file=1,
partial=2,
invalid=3,
overflow=4
};
}
