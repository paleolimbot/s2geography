
<!-- README.md is generated from README.Rmd. Please edit that file -->

# s2geography

<!-- badges: start -->
<!-- badges: end -->

## Installation

This is a brand new library, so build + install may not work everywhere
(PR fixes or open issues if this fails!). You can build and install
using `cmake`, and the project is structured such that the VSCode
`cmake` integeration “just works” (if it doesn’t, consider using
`CMakeUserPresets.json` to configure things like the install
directory/location of OpenSSL).

You can also configure manually using `cmake`:

``` bash
mkdir build
cd build

# you may need to specify the location of OpenSSL
# (e.g., `cmake .. -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@1.1` on M1 mac)
cmake ..
cmake --build .
cmake --install . --prefix ../dist
```

This will download, build, and install s2 to the install directory as
well.

``` cpp
#include "cpp11.hpp"
#include "s2geography.hpp"

using namespace s2geography;

[[cpp11::register]]
void test_s2geography() {
  PointGeography point1 = S2LatLng::FromDegrees(45, -64).ToPoint();
  PointGeography point2 = S2LatLng::FromDegrees(45, 0).ToPoint();
  
  ShapeIndexGeography point1_index(point1);
  ShapeIndexGeography point2_index(point2);
  
  double dist = s2_distance(point1_index, point2_index);
  
  printf("distance result is %g", dist);
}
```

``` r
test_s2geography()
```

You should then be able to `#include "s2geography.hpp"` and go! I
haven’t quite triaged the `-labsl_*` flags yet, but it is a subset of:

    CPPFLAGS = -I<install_prefix>/include -I<openssl_root_dir>/include
    LDFLAGS = -L<install_prefix>/lib -ls2geography -ls2 -labsl_bad_any_cast_impl -labsl_bad_optional_access -labsl_bad_variant_access -labsl_base -labsl_city -labsl_civil_time -labsl_cord_internal -labsl_cord -labsl_cordz_functions -labsl_cordz_handle -labsl_cordz_info -labsl_cordz_sample_token -labsl_debugging_internal -labsl_demangle_internal -labsl_examine_stack -labsl_exponential_biased -labsl_failure_signal_handler -labsl_flags_commandlineflag_internal -labsl_flags_commandlineflag -labsl_flags_config -labsl_flags_internal -labsl_flags_marshalling -labsl_flags_parse -labsl_flags_private_handle_accessor -labsl_flags_program_name -labsl_flags_reflection -labsl_flags_usage_internal -labsl_flags_usage -labsl_flags -labsl_graphcycles_internal -labsl_hash -labsl_hashtablez_sampler -labsl_int128 -labsl_leak_check_disable -labsl_leak_check -labsl_log_severity -labsl_low_level_hash -labsl_malloc_internal -labsl_periodic_sampler -labsl_random_distributions -labsl_random_internal_distribution_test_util -labsl_random_internal_platform -labsl_random_internal_pool_urbg -labsl_random_internal_randen_hwaes_impl -labsl_random_internal_randen_hwaes -labsl_random_internal_randen_slow -labsl_random_internal_randen -labsl_random_internal_seed_material -labsl_random_seed_gen_exception -labsl_random_seed_sequences -labsl_raw_hash_set -labsl_raw_logging_internal -labsl_scoped_set_env -labsl_spinlock_wait -labsl_stacktrace -labsl_status -labsl_statusor -labsl_str_format_internal -labsl_strerror -labsl_strings_internal -labsl_strings -labsl_symbolize -labsl_synchronization -labsl_throw_delegate -labsl_time_zone -labsl_time
