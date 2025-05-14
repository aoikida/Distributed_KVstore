#ifndef INDEX_INTERFACE_HPP
#define INDEX_INTERFACE_HPP

#include <unordered_map>
#include <string>
#include <utility>
#include <vector>
#include "../merklecpp/merklecpp.h"

class IndexInterface {
public:
    using KeyValueData = std::unordered_map<std::string, std::pair<std::string, uint64_t>>;
    
    virtual ~IndexInterface() = default;
    virtual void rebuild(const KeyValueData& kv_data) = 0;
    virtual std::unordered_map<std::string, uint64_t> get_key_timestamps() const = 0;
    virtual merkle::Hash get_root_hash() const { return merkle::Hash(); }
    virtual std::vector<merkle::Path> get_paths(const std::vector<std::string>&) const { return {}; }
    virtual size_t size() const { return 0; }
    virtual bool empty() const { return true; }
};

#endif // INDEX_INTERFACE_HPP
