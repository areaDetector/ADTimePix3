TOP=..
include $(TOP)/configure/CONFIG

#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

SRC_DIRS += ../frozenSrc
SRC_DIRS += ../cprSrc
#SRC_DIRS += ../cprSrc/cpr/build/cpr_generated_includes
SRC_DIRS += ../cprSrc/cpr/include
SRC_DIRS += ../jsonSrc/json/include


LIBRARY=frozen

INC += frozen.h

# C++ request:  Curl for people
INC += cpr/cprver.h

INC += cpr/accept_encoding.h
INC += cpr/api.h
INC += cpr/async.h
INC += cpr/auth.h
INC += cpr/bearer.h
INC += cpr/body.h
INC += cpr/callback.h
INC += cpr/connect_timeout.h
INC += cpr/cookies.h
INC += cpr/cpr.h
INC += cpr/cprtypes.h
INC += cpr/curl_container.h
INC += cpr/curlholder.h
INC += cpr/error.h
INC += cpr/http_version.h
INC += cpr/interceptor.h
INC += cpr/interface.h
INC += cpr/limit_rate.h
INC += cpr/local_port.h
INC += cpr/local_port_range.h
INC += cpr/low_speed.h
INC += cpr/multipart.h
INC += cpr/parameters.h
INC += cpr/payload.h
INC += cpr/proxies.h
INC += cpr/proxyauth.h
INC += cpr/range.h
INC += cpr/redirect.h
INC += cpr/reserve_size.h
INC += cpr/response.h
INC += cpr/session.h
INC += cpr/singleton.h
INC += cpr/ssl_ctx.h
INC += cpr/ssl_options.h
INC += cpr/status_codes.h
INC += cpr/threadpool.h
INC += cpr/timeout.h
INC += cpr/unix_socket.h
INC += cpr/user_agent.h
INC += cpr/util.h
INC += cpr/verbose.h
# C++ request end

# json parser
INC += nlohmann/adl_serializer.hpp
INC += nlohmann/byte_container_with_subtype.hpp
INC += nlohmann/json_fwd.hpp
INC += nlohmann/json.hpp
INC += nlohmann/ordered_map.hpp

INC += nlohmann/detail/exceptions.hpp
INC += nlohmann/detail/hash.hpp
INC += nlohmann/detail/json_pointer.hpp
INC += nlohmann/detail/json_ref.hpp
INC += nlohmann/detail/macro_scope.hpp
INC += nlohmann/detail/macro_unscope.hpp
INC += nlohmann/detail/string_concat.hpp
INC += nlohmann/detail/string_escape.hpp
INC += nlohmann/detail/value_t.hpp

INC += nlohmann/detail/conversions/from_json.hpp
INC += nlohmann/detail/conversions/to_chars.hpp
INC += nlohmann/detail/conversions/to_json.hpp

INC += nlohmann/detail/input/binary_reader.hpp
INC += nlohmann/detail/input/input_adapters.hpp
INC += nlohmann/detail/input/json_sax.hpp
INC += nlohmann/detail/input/lexer.hpp
INC += nlohmann/detail/input/parser.hpp
INC += nlohmann/detail/input/position_t.hpp

INC += nlohmann/detail/iterators/internal_iterator.hpp
INC += nlohmann/detail/iterators/iteration_proxy.hpp
INC += nlohmann/detail/iterators/iterator_traits.hpp
INC += nlohmann/detail/iterators/iter_impl.hpp
INC += nlohmann/detail/iterators/json_reverse_iterator.hpp
INC += nlohmann/detail/iterators/primitive_iterator.hpp

INC += nlohmann/detail/meta/cpp_future.hpp
INC += nlohmann/detail/meta/detected.hpp
INC += nlohmann/detail/meta/identity_tag.hpp
INC += nlohmann/detail/meta/is_sax.hpp
INC += nlohmann/detail/meta/type_traits.hpp
INC += nlohmann/detail/meta/void_t.hpp

INC += nlohmann/detail/meta/call_std/begin.hpp
INC += nlohmann/detail/meta/call_std/end.hpp

INC += nlohmann/detail/output/binary_writer.hpp
INC += nlohmann/detail/output/output_adapters.hpp
INC += nlohmann/detail/output/serializer.hpp

INC += nlohmann/thirdparty/hedley/hedley.hpp
INC += nlohmann/thirdparty/hedley/hedley_undef.hpp
# json parser end

frozen_SRCS += frozen.c

LIB_INSTALLS_Linux +=  ../cprSrc/cpr/build/lib/libcpr.so
LIB_INSTALLS_Linux +=  ../cprSrc/cpr/build/lib/libcurl-d.so
LIB_INSTALLS_Linux +=  ../cprSrc/cpr/build/lib/libz.so

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE


