#pragma once

#include <cassert>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include "aison2/aison2.h"

namespace aison2 {

namespace runtime {

// Minimal backend-agnostic encoder/decoder concepts via templates ------------
template<class Value>
struct Encoder {
    // Backend-specific encoder must implement:
    //   void null();
    //   void boolean(bool);
    //   void number(double); // or separate int/uint if desired
    //   void string(std::string_view);
    //   void beginObject();
    //   void key(std::string_view);
    //   void endObject();
    //   void beginArray();
    //   void endArray();
    //   Value take(); // final value
};

template<class Value>
struct Decoder {
    // Backend-specific decoder must implement:
    //   bool isNull();
    //   bool isBoolean();
    //   bool isNumber();
    //   bool isString();
    //   bool isObject();
    //   bool isArray();
    //   bool boolean();
    //   double number(); // or separate int/uint if desired
    //   std::string_view string();
    //   void beginObject();
    //   bool nextObjectKey(std::string_view& outKey);
    //   void endObject();
    //   void beginArray();
    //   bool nextArrayElement();
    //   void endArray();
};

// Core encode/decode dispatcher interfaces -----------------------------------
template<class Schema>
struct EncoderDispatch;

template<class Schema>
struct DecoderDispatch;

// Custom helper --------------------------------------------------------------
template<class Schema, class T, class EncoderFn, class DecoderFn>
struct CustomAdapter {
    template<class Enc, class Ctx>
    static void encode(Enc& enc, const T& value, Ctx& ctx)
    {
        EncoderFn fn = ctx.template getCustomEncoder<T>();
        fn(enc, value, ctx);
    }

    template<class Dec, class Ctx>
    static T decode(Dec& dec, Ctx& ctx)
    {
        DecoderFn fn = ctx.template getCustomDecoder<T>();
        return fn(dec, ctx);
    }
};

}  // namespace runtime

}  // namespace aison2
