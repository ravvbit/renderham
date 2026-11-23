// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/registry.hpp
//
// This is what makes "einfaches Wechseln des Dithering-Algorithmus"
// possible: every algorithm implementation registers itself under a
// string name in its own .cpp file (see dithering/floyd_steinberg.cpp
// for an example). The CLI, a future GUI, or the pipeline itself never
// need to know the concrete class - they just ask the registry by name.
// Adding a new algorithm never requires touching this file or the
// pipeline: drop in a new .cpp, add it to CMakeLists.txt, done.

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace aic {

template <typename Interface>
class Registry {
public:
    using Factory = std::function<std::unique_ptr<Interface>()>;

    static Registry& instance() {
        static Registry reg;
        return reg;
    }

    void register_factory(const std::string& name, Factory f) {
        factories_[name] = std::move(f);
    }

    std::unique_ptr<Interface> create(const std::string& name) const {
        auto it = factories_.find(name);
        if (it == factories_.end()) {
            throw std::invalid_argument("aic::Registry: unknown name '" + name + "'");
        }
        return it->second();
    }

    bool contains(const std::string& name) const { return factories_.count(name) != 0; }

    std::vector<std::string> names() const {
        std::vector<std::string> result;
        result.reserve(factories_.size());
        for (const auto& [k, v] : factories_) result.push_back(k);
        return result;
    }

private:
    Registry() = default;
    std::unordered_map<std::string, Factory> factories_;
};

// Place `static aic::AutoRegister<IFoo, MyFoo> _reg("my-foo");` at
// namespace scope in an implementation file to self-register MyFoo.
template <typename Interface, typename Impl>
struct AutoRegister {
    explicit AutoRegister(const std::string& name) {
        Registry<Interface>::instance().register_factory(name, [] { return std::make_unique<Impl>(); });
    }
};

}  // namespace aic
