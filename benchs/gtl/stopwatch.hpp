#ifndef gtl_stopwatch_hpp_guard
#define gtl_stopwatch_hpp_guard

// ---------------------------------------------------------------------------
// Copyright (c) 2022, Gregory Popovitch - greg7mdp@gmail.com
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
// ---------------------------------------------------------------------------

#include <chrono>

namespace gtl {
// -------------------------------------------------------------------------------
template<typename time_unit = std::milli>
class stopwatch {
public:
    stopwatch(bool do_start = true) {
        if (do_start)
            start();
    }

    void start() { _start = _snap = clock::now(); }
    void snap() { _snap = clock::now(); }

    float since_start() const { return get_diff<float>(_start, clock::now()); }
    float since_snap() const { return get_diff<float>(_snap, clock::now()); }
    float start_to_snap() const { return get_diff<float>(_start, _snap); }

private:
    using clock = std::chrono::high_resolution_clock;
    using point = std::chrono::time_point<clock>;

    template<typename T>
    static T get_diff(const point& start, const point& end) {
        using duration_t = std::chrono::duration<T, time_unit>;
        return std::chrono::duration_cast<duration_t>(end - start).count();
    }

    point _start;
    point _snap;
};

// -------------------------------------------------------------------------------
template<typename StopWatch>
class start_snap {
public:
    start_snap(StopWatch& sw)
        : _sw(sw) {
        _sw.start();
    }
    ~start_snap() { _sw.snap(); }

private:
    StopWatch& _sw;
};

}

#endif // gtl_stopwatch_hpp_guard
