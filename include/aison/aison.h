#pragma once

#include <json/json.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace aison {

// ============================================================================
// Facet tags (types)
// ============================================================================
struct encodeOnly {};
struct decodeOnly {};
struct encodeDecode {};

// Forward declarations
template <typename Schema>
class Encoder;

template <typename Schema>
class Decoder;

template <typename Schema, typename Owner, typename Facet = encodeDecode>
struct Fields;

template <typename E, size_t N>
using EnumMap = std::array<std::pair<E, std::string_view>, N>;

namespace detail {

// ============================================================================
// Common context: error handling + JSON path tracking
// ============================================================================
template <typename Schema>
struct ContextBase {
  struct Error {
    std::string path;     // e.g. "$.nested.x" or "$.ls[2]"
    std::string message;  // e.g. "Expected integer"
  };

  std::vector<Error> errors;
  std::string currentPath;  // built up as we recurse

  void addError(const std::string& msg) {
    std::string p = currentPath.empty() ? "$" : "$." + currentPath;
    errors.push_back({p, msg});
  }

  struct PathScope {
    ContextBase& ctx;
    std::string oldPath;

    PathScope(ContextBase& c, const std::string& key)
        : ctx(c), oldPath(c.currentPath) {
      if (ctx.currentPath.empty())
        ctx.currentPath = key;
      else
        ctx.currentPath += "." + key;
    }

    PathScope(ContextBase& c, std::size_t index)
        : ctx(c), oldPath(c.currentPath) {
      std::string part = "[" + std::to_string(index) + "]";
      if (ctx.currentPath.empty())
        ctx.currentPath = part;
      else
        ctx.currentPath += part;
    }

    ~PathScope() { ctx.currentPath = oldPath; }
  };

  bool ok() const { return errors.empty(); }
};

}  // namespace detail

// ============================================================================
// Encoder<Schema>
// ============================================================================
template <typename Schema>
class Encoder : public detail::ContextBase<Schema> {
 public:
  using Base = detail::ContextBase<Schema>;
  using Error = typename Base::Error;
  using PathScope = typename Base::PathScope;

  struct Result {
    bool ok;
    const std::vector<Error>* errors;

    explicit operator bool() const { return ok; }
    const std::vector<Error>& getErrors() const { return *errors; }
  };

  Encoder() = default;

  template <typename T>
  Result encode(const T& value, Json::Value& dst);

  void addError(const std::string& msg) { Base::addError(msg); }

  const std::vector<Error>& getErrors() const { return this->errors; }
};

// ============================================================================
// Decoder<Schema>
// ============================================================================
template <typename Schema>
class Decoder : public detail::ContextBase<Schema> {
 public:
  using Base = detail::ContextBase<Schema>;
  using Error = typename Base::Error;
  using PathScope = typename Base::PathScope;

  struct Result {
    bool ok;
    const std::vector<Error>* errors;

    explicit operator bool() const { return ok; }
    const std::vector<Error>& getErrors() const { return *errors; }
  };

  Decoder() = default;

  template <typename T>
  Result decode(const Json::Value& src, T& value);

  void addError(const std::string& msg) { Base::addError(msg); }

  const std::vector<Error>& getErrors() const { return this->errors; }
};

namespace detail {

// ============================================================================
// Type traits for optional / vector
// ============================================================================
template <typename T>
struct is_optional : std::false_type {};

template <typename U>
struct is_optional<std::optional<U>> : std::true_type {};

template <typename T>
inline constexpr bool is_optional_v = is_optional<T>::value;

template <typename T>
struct is_vector : std::false_type {};

template <typename U, typename Alloc>
struct is_vector<std::vector<U, Alloc>> : std::true_type {};

template <typename T>
inline constexpr bool is_vector_v = is_vector<T>::value;

// Forward declarations for default encode/decode/dispatchers
template <typename Schema, typename T>
void encode_default(const T& value, Json::Value& dst, Encoder<Schema>& enc);

template <typename Schema, typename T>
void decode_default(const Json::Value& src, T& value, Decoder<Schema>& dec);

template <typename Schema, typename T>
void encode_value(const T& value, Json::Value& dst, Encoder<Schema>& enc);

template <typename Schema, typename T>
void decode_value(const Json::Value& src, T& value, Decoder<Schema>& dec);

// ============================================================================
// Detection of Schema::encodeValue / Schema::decodeValue
// ============================================================================
template <typename Schema, typename T, typename = void>
struct has_schema_encode : std::false_type {};

template <typename Schema, typename T>
struct has_schema_encode<
    Schema, T,
    std::void_t<decltype(Schema::encodeValue(
        std::declval<const T&>(), std::declval<Json::Value&>(),
        std::declval<Encoder<Schema>&>()))>> : std::true_type {};

template <typename Schema, typename T, typename = void>
struct has_schema_decode : std::false_type {};

template <typename Schema, typename T>
struct has_schema_decode<
    Schema, T,
    std::void_t<decltype(Schema::decodeValue(
        std::declval<const Json::Value&>(), std::declval<T&>(),
        std::declval<Decoder<Schema>&>()))>> : std::true_type {};

// ============================================================================
// Detection of Schema::Enum<E> mapping for enum classes
// ============================================================================
template <typename Schema, typename E, typename = void>
struct has_enum_mapping : std::false_type {};

template <typename Schema, typename E>
struct has_enum_mapping<Schema, E,
                        std::void_t<typename Schema::template Enum<E>>>
    : std::true_type {};

// ============================================================================
// Dispatcher: encode_value / decode_value
// ============================================================================
template <typename Schema, typename T>
void encode_value(const T& value, Json::Value& dst, Encoder<Schema>& enc) {
  if constexpr (has_schema_encode<Schema, T>::value) {
    Schema::encodeValue(value, dst, enc);
  } else {
    encode_default<Schema, T>(value, dst, enc);
  }
}

template <typename Schema, typename T>
void decode_value(const Json::Value& src, T& value, Decoder<Schema>& dec) {
  if constexpr (has_schema_decode<Schema, T>::value) {
    Schema::decodeValue(src, value, dec);
  } else {
    decode_default<Schema, T>(src, value, dec);
  }
}

// ============================================================================
// Default ENCODE implementation
// ============================================================================
template <typename Schema, typename T>
void encode_default(const T& value, Json::Value& dst, Encoder<Schema>& enc) {
  if constexpr (std::is_same_v<T, int>) {
    dst = value;
  } else if constexpr (std::is_same_v<T, double>) {
    dst = value;
  } else if constexpr (std::is_same_v<T, std::string>) {
    dst = value;
  } else if constexpr (std::is_same_v<T, bool>) {
    dst = value;
  } else if constexpr (std::is_enum_v<T> &&
                       has_enum_mapping<Schema, T>::value) {
    using EnumSpec = typename Schema::template Enum<T>;
    const auto& mapping = EnumSpec::mapping;
    for (const auto& entry : mapping) {
      if (entry.first == value) {
        dst = Json::Value(std::string(entry.second));
        return;
      }
    }
    enc.addError("Unhandled enum value during encode");
  } else if constexpr (is_optional_v<T>) {
    using U = typename T::value_type;
    if (!value) {
      dst = Json::nullValue;
    } else {
      encode_value<Schema, U>(*value, dst, enc);
    }
  } else if constexpr (is_vector_v<T>) {
    using U = typename T::value_type;
    dst = Json::arrayValue;
    std::size_t idx = 0;
    for (const auto& elem : value) {
      typename Encoder<Schema>::PathScope guard(enc, idx++);
      Json::Value v;
      encode_value<Schema, U>(elem, v, enc);
      dst.append(v);
    }
  } else {
    // Treat as reflected object via Schema::Fields<T>
    typename Schema::template Fields<T> fields;
    fields.encodeObject(value, dst, enc);
  }
}

// ============================================================================
// Default DECODE implementation
// ============================================================================
template <typename Schema, typename T>
void decode_default(const Json::Value& src, T& value, Decoder<Schema>& dec) {
  if constexpr (std::is_same_v<T, int>) {
    if (!src.isInt()) {
      dec.addError("Expected integer");
      return;
    }
    value = src.asInt();
  } else if constexpr (std::is_same_v<T, double>) {
    if (!src.isDouble() && !src.isInt()) {
      dec.addError("Expected double");
      return;
    }
    value = src.asDouble();
  } else if constexpr (std::is_same_v<T, std::string>) {
    if (!src.isString()) {
      dec.addError("Expected string");
      return;
    }
    value = src.asString();
  } else if constexpr (std::is_same_v<T, bool>) {
    if (!src.isBool()) {
      dec.addError("Expected bool");
      return;
    }
    value = src.asBool();
  } else if constexpr (std::is_enum_v<T> &&
                       has_enum_mapping<Schema, T>::value) {
    if (!src.isString()) {
      dec.addError("Expected string for enum");
      return;
    }
    const std::string s = src.asString();
    using EnumSpec = typename Schema::template Enum<T>;
    const auto& mapping = EnumSpec::mapping;
    for (const auto& entry : mapping) {
      if (s == entry.second) {
        value = entry.first;
        return;
      }
    }
    dec.addError("Unknown enum value: " + s);
  } else if constexpr (is_optional_v<T>) {
    using U = typename T::value_type;
    if (src.isNull()) {
      value.reset();
    } else {
      U tmp{};
      decode_value<Schema, U>(src, tmp, dec);
      value = std::move(tmp);
    }
  } else if constexpr (is_vector_v<T>) {
    using U = typename T::value_type;
    value.clear();
    if (!src.isArray()) {
      dec.addError("Expected array");
      return;
    }
    for (Json::ArrayIndex i = 0; i < src.size(); ++i) {
      typename Decoder<Schema>::PathScope guard(dec,
                                                static_cast<std::size_t>(i));
      U elem{};
      decode_value<Schema, U>(src[i], elem, dec);
      value.push_back(std::move(elem));
    }
  } else {
    if (!src.isObject()) {
      dec.addError("Expected object");
      return;
    }
    typename Schema::template Fields<T> fields;
    fields.decodeObject(src, value, dec);
  }
}

}  // namespace detail

// ============================================================================
// Encoder<Schema>::encode implementation
// ============================================================================
template <typename Schema>
template <typename T>
inline typename Encoder<Schema>::Result Encoder<Schema>::encode(
    const T& value, Json::Value& dst) {
  this->errors.clear();
  detail::encode_value<Schema, T>(value, dst, *this);
  return {this->ok(), &this->errors};
}

// ============================================================================
// Decoder<Schema>::decode implementation
// ============================================================================
template <typename Schema>
template <typename T>
inline typename Decoder<Schema>::Result Decoder<Schema>::decode(
    const Json::Value& src, T& value) {
  this->errors.clear();
  detail::decode_value<Schema, T>(src, value, *this);
  return {this->ok(), &this->errors};
}

// ============================================================================
// Facet-specific field interfaces + FieldsImpl specializations
// ============================================================================

// ----- Interfaces -----
template <typename Owner, typename Schema>
struct IEncodeFieldDesc {
  virtual ~IEncodeFieldDesc() = default;
  virtual const char* name() const = 0;
  virtual void encodeField(const Owner& srcOwner, Json::Value& dstJson,
                           Encoder<Schema>& enc) const = 0;
};

template <typename Owner, typename Schema>
struct IDecodeFieldDesc {
  virtual ~IDecodeFieldDesc() = default;
  virtual const char* name() const = 0;
  virtual void decodeField(const Json::Value& srcJson, Owner& dstOwner,
                           Decoder<Schema>& dec) const = 0;
};

// ED facet has its own base, no multiple inheritance
template <typename Owner, typename Schema>
struct IFieldDescED {
  virtual ~IFieldDescED() = default;
  virtual const char* name() const = 0;
  virtual void encodeField(const Owner& srcOwner, Json::Value& dstJson,
                           Encoder<Schema>& enc) const = 0;
  virtual void decodeField(const Json::Value& srcJson, Owner& dstOwner,
                           Decoder<Schema>& dec) const = 0;
};

// ----- FieldDesc implementations -----
template <typename Owner, typename T, typename Schema>
struct FieldDescE : IEncodeFieldDesc<Owner, Schema> {
  const char* fieldName;
  T Owner::* member;

  FieldDescE(const char* n, T Owner::* m) : fieldName(n), member(m) {}

  const char* name() const override { return fieldName; }

  void encodeField(const Owner& srcOwner, Json::Value& dstJson,
                   Encoder<Schema>& enc) const override {
    const T& ref = srcOwner.*member;
    detail::encode_value<Schema, T>(ref, dstJson, enc);
  }
};

template <typename Owner, typename T, typename Schema>
struct FieldDescD : IDecodeFieldDesc<Owner, Schema> {
  const char* fieldName;
  T Owner::* member;

  FieldDescD(const char* n, T Owner::* m) : fieldName(n), member(m) {}

  const char* name() const override { return fieldName; }

  void decodeField(const Json::Value& srcJson, Owner& dstOwner,
                   Decoder<Schema>& dec) const override {
    T& ref = dstOwner.*member;
    detail::decode_value<Schema, T>(srcJson, ref, dec);
  }
};

template <typename Owner, typename T, typename Schema>
struct FieldDescED : IFieldDescED<Owner, Schema> {
  const char* fieldName;
  T Owner::* member;

  FieldDescED(const char* n, T Owner::* m) : fieldName(n), member(m) {}

  const char* name() const override { return fieldName; }

  void encodeField(const Owner& srcOwner, Json::Value& dstJson,
                   Encoder<Schema>& enc) const override {
    const T& ref = srcOwner.*member;
    detail::encode_value<Schema, T>(ref, dstJson, enc);
  }

  void decodeField(const Json::Value& srcJson, Owner& dstOwner,
                   Decoder<Schema>& dec) const override {
    T& ref = dstOwner.*member;
    detail::decode_value<Schema, T>(srcJson, ref, dec);
  }
};

// ----- FieldsImpl specializations by facet -----
template <typename Schema, typename Owner, typename Facet>
class FieldsImpl;

// encodeOnly
template <typename Schema, typename Owner>
class FieldsImpl<Schema, Owner, encodeOnly> {
  using Enc = Encoder<Schema>;
  using Base = IEncodeFieldDesc<Owner, Schema>;
  std::vector<std::unique_ptr<Base>> data_;

 public:
  template <typename T>
  void add(T Owner::* member, const char* name) {
    data_.push_back(
        std::make_unique<FieldDescE<Owner, T, Schema>>(name, member));
  }

  std::size_t size() const { return data_.size(); }

  auto begin() { return data_.begin(); }
  auto end() { return data_.end(); }
  auto begin() const { return data_.begin(); }
  auto end() const { return data_.end(); }

  void encodeObject(const Owner& src, Json::Value& dst, Enc& enc) const {
    dst = Json::objectValue;
    for (const auto& f : data_) {
      typename Enc::PathScope guard(enc, f->name());
      Json::Value& slot = dst[f->name()];
      f->encodeField(src, slot, enc);
    }
  }

  // no decodeObject here
};

// decodeOnly
template <typename Schema, typename Owner>
class FieldsImpl<Schema, Owner, decodeOnly> {
  using Dec = Decoder<Schema>;
  using Base = IDecodeFieldDesc<Owner, Schema>;
  std::vector<std::unique_ptr<Base>> data_;

 public:
  template <typename T>
  void add(T Owner::* member, const char* name) {
    data_.push_back(
        std::make_unique<FieldDescD<Owner, T, Schema>>(name, member));
  }

  std::size_t size() const { return data_.size(); }

  auto begin() { return data_.begin(); }
  auto end() { return data_.end(); }
  auto begin() const { return data_.begin(); }
  auto end() const { return data_.end(); }

  // no encodeObject here

  void decodeObject(const Json::Value& src, Owner& dst, Dec& dec) const {
    for (const auto& f : data_) {
      const char* key = f->name();
      if (!src.isMember(key)) {
        dec.addError(std::string("Missing required field: ") + key);
        continue;
      }
      const Json::Value& slot = src[key];
      typename Dec::PathScope guard(dec, key);
      f->decodeField(slot, dst, dec);
    }
  }
};

// encodeDecode
template <typename Schema, typename Owner>
class FieldsImpl<Schema, Owner, encodeDecode> {
  using Enc = Encoder<Schema>;
  using Dec = Decoder<Schema>;
  using Base = IFieldDescED<Owner, Schema>;
  std::vector<std::unique_ptr<Base>> data_;

 public:
  template <typename T>
  void add(T Owner::* member, const char* name) {
    data_.push_back(
        std::make_unique<FieldDescED<Owner, T, Schema>>(name, member));
  }

  std::size_t size() const { return data_.size(); }

  auto begin() { return data_.begin(); }
  auto end() { return data_.end(); }
  auto begin() const { return data_.begin(); }
  auto end() const { return data_.end(); }

  void encodeObject(const Owner& src, Json::Value& dst, Enc& enc) const {
    dst = Json::objectValue;
    for (const auto& f : data_) {
      typename Enc::PathScope guard(enc, f->name());
      Json::Value& slot = dst[f->name()];
      f->encodeField(src, slot, enc);
    }
  }

  void decodeObject(const Json::Value& src, Owner& dst, Dec& dec) const {
    for (const auto& f : data_) {
      const char* key = f->name();
      if (!src.isMember(key)) {
        dec.addError(std::string("Missing required field: ") + key);
        continue;
      }
      const Json::Value& slot = src[key];
      typename Dec::PathScope guard(dec, key);
      f->decodeField(slot, dst, dec);
    }
  }
};

// ============================================================================
// Public base for schema field mappings
// ============================================================================
template <typename Schema, typename Owner, typename Facet>
struct Fields : FieldsImpl<Schema, Owner, Facet> {
  using Base = FieldsImpl<Schema, Owner, Facet>;
  using Base::add;
  using Base::Base;
};

}  // namespace aison
