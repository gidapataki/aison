#include <aison/aison.h>
#include <doctest.h>
#include <json/json.h>

// A value whose JSON shape depends on schema config
struct VersionedValue {
    int raw = 0;
};

struct Config {
    int version = 1;
};

// Schema with runtime Config
struct SchemaWithConfig : aison::Schema<SchemaWithConfig, aison::EncodeDecode, Config> {};

template<>
struct SchemaWithConfig::CustomEncoder<VersionedValue>
    : aison::Encoder<SchemaWithConfig, VersionedValue> {
    void operator()(const VersionedValue& src, Json::Value& dst)
    {
        auto& cfg = config();

        if (cfg.version == 1) {
            // v1: encode as bare integer
            dst = src.raw;
        } else {
            // v2: encode as an object with metadata
            Json::Value obj(Json::objectValue);
            obj["raw"] = src.raw;
            obj["meta"] = Json::Value("v2");
            dst = std::move(obj);
        }
    }
};

template<>
struct SchemaWithConfig::CustomDecoder<VersionedValue>
    : aison::Decoder<SchemaWithConfig, VersionedValue> {
    void operator()(const Json::Value& src, VersionedValue& dst)
    {
        // Accept both v1 and v2 shapes
        if (src.isInt()) {
            dst.raw = src.asInt();
        } else if (src.isObject() && src.isMember("raw") && src["raw"].isInt()) {
            dst.raw = src["raw"].asInt();
        } else {
            addError("Unsupported JSON shape for VersionedValue");
        }
    }
};

TEST_SUITE("SchemaWithConfig – config-aware encode/decode")
{
    TEST_CASE("encode uses Schema::Config.version to choose shape")
    {
        VersionedValue v;
        v.raw = 42;

        // --- v1: integer encoding ---
        Config cfg_v1;
        cfg_v1.version = 1;

        Json::Value json_v1;
        auto res_v1 = aison::encode<SchemaWithConfig>(v, json_v1, cfg_v1);
        REQUIRE(res_v1);
        CHECK(res_v1.errors.empty());

        CHECK(json_v1.isInt());
        CHECK(json_v1.asInt() == 42);

        // --- v2: object encoding ---
        Config cfg_v2;
        cfg_v2.version = 2;

        Json::Value json_v2;
        auto res_v2 = aison::encode<SchemaWithConfig>(v, json_v2, cfg_v2);
        REQUIRE(res_v2);
        CHECK(res_v2.errors.empty());

        CHECK(json_v2.isObject());
        CHECK(json_v2["raw"].isInt());
        CHECK(json_v2["raw"].asInt() == 42);
        CHECK(json_v2["meta"].isString());
        CHECK(json_v2["meta"].asString() == "v2");

        // NOTE: this should NOT compile (by design):
        // auto bad = aison::encode<SchemaWithConfig>(v, json_v1);
    }

    TEST_CASE("decode accepts both v1 and v2 shapes")
    {
        Config cfg;
        cfg.version = 2;  // version doesn't really matter for decode here

        // v1 JSON: plain integer
        {
            Json::Value json = Json::Value(123);
            VersionedValue v{};
            auto res = aison::decode<SchemaWithConfig>(json, v, cfg);
            REQUIRE(res);
            CHECK(res.errors.empty());
            CHECK(v.raw == 123);
        }

        // v2 JSON: object with "raw" and "meta"
        {
            Json::Value json(Json::objectValue);
            json["raw"] = Json::Value(777);
            json["meta"] = Json::Value("v2");

            VersionedValue v{};
            auto res = aison::decode<SchemaWithConfig>(json, v, cfg);
            REQUIRE(res);
            CHECK(res.errors.empty());
            CHECK(v.raw == 777);
        }

        // invalid JSON shape -> error
        {
            Json::Value json = Json::Value("not-valid");
            VersionedValue v{};
            auto res = aison::decode<SchemaWithConfig>(json, v, cfg);
            CHECK_FALSE(res);
            REQUIRE_FALSE(res.errors.empty());
            CHECK(res.errors[0].path == "$");
            CHECK(res.errors[0].message.find("Unsupported JSON shape") != std::string::npos);
        }
    }

}  // TEST_SUITE
