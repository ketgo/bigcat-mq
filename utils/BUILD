# Copyright (c) 2022 Ketan Goyal
#
# This software is released under the MIT License.
# https://opensource.org/licenses/MIT

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "random",
    hdrs = [
        "random.hpp",
    ],
    deps = [],
)

cc_library(
    name = "threads",
    hdrs = [
        "threads.hpp",
    ],
    deps = [],
)

cc_library(
    name = "chrono",
    hdrs = [
        "chrono.hpp",
    ],
    deps = [
        ":random",
    ],
)

cc_library(
    name = "debug",
    hdrs = [
        "debug.hpp",
    ],
    deps = [],
)
