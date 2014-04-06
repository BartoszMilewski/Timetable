#if !defined (HELPER_H)
#define HELPER_H
#include <vector>
#include "PureStream.h"

template<class T>
std::vector<T> concatAll(std::vector<std::vector<T>> const & in)
{
    unsigned total = std::accumulate(in.begin(), in.end(), 0u,
        [](unsigned sum, std::vector<T> const & v) { return sum + v.size(); });
    std::vector<T> res;
    res.reserve(total);
    std::for_each(in.begin(), in.end(), [&res](std::vector<T> const & v){
        std::copy(v.begin(), v.end(), std::back_inserter(res));
    });
    return res;
}

template<class T, class U>
std::ostream& operator<<(std::ostream& os, std::pair<T, U> const & p)
{
    os << "(" << p.first << ", " << p.second << ") ";
    return os;
}

#endif
