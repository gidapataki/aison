#include <aison/aison.h>
#include <doctest.h>
#include <json/json.h>

#include <optional>
#include <string>
#include <vector>

namespace {

enum class Category { Utility, Core, Experimental };
enum class Importance { Low, Medium, High };

struct Stats {
    int count = 0;
    double mean = 0.0;
    std::vector<int> buckets;
    std::optional<std::vector<double>> deltas;
};

struct Item {
    std::string name;
    Category category = Category::Utility;
    Importance importance = Importance::Low;
    Stats stats;
    std::optional<std::string> note;
    std::vector<std::string> tags;
};

struct Document {
    int version = 0;
    float scale = 1.0f;
    std::vector<Item> items;
    std::optional<Item> featured;
};

struct BasicSchema : aison::Schema<BasicSchema> {
    template<typename T>
    struct Object;
    template<typename T>
    struct Enum;
};

template<>
struct BasicSchema::Enum<Category> : aison::Enum<BasicSchema, Category> {
    Enum()
    {
        add(Category::Utility, "utility");
        add(Category::Core, "core");
        add(Category::Experimental, "experimental");
        addAlias(Category::Experimental, "exp");
    }
};

template<>
struct BasicSchema::Enum<Importance> : aison::Enum<BasicSchema, Importance> {
    Enum()
    {
        add(Importance::Low, "low");
        add(Importance::Medium, "medium");
        add(Importance::High, "high");
        addAlias(Importance::Medium, "med");
    }
};

template<>
struct BasicSchema::Object<Stats> : aison::Object<BasicSchema, Stats> {
    Object()
    {
        add(&Stats::count, "count");
        add(&Stats::mean, "mean");
        add(&Stats::buckets, "buckets");
        add(&Stats::deltas, "deltas");
    }
};

template<>
struct BasicSchema::Object<Item> : aison::Object<BasicSchema, Item> {
    Object()
    {
        add(&Item::name, "name");
        add(&Item::category, "category");
        add(&Item::importance, "importance");
        add(&Item::stats, "stats");
        add(&Item::note, "note");
        add(&Item::tags, "tags");
    }
};

template<>
struct BasicSchema::Object<Document> : aison::Object<BasicSchema, Document> {
    Object()
    {
        add(&Document::version, "version");
        add(&Document::scale, "scale");
        add(&Document::items, "items");
        add(&Document::featured, "featured");
    }
};

TEST_SUITE("Basic types")
{
    TEST_CASE("Round-trip with engaged optionals and nested containers")
    {
        Stats s1;
        s1.count = 5;
        s1.mean = 2.5;
        s1.buckets = {1, 2, 2, 0, 0};
        s1.deltas = std::vector<double>{0.1, -0.2, 0.3};

        Stats s2;
        s2.count = 2;
        s2.mean = 10.0;
        s2.buckets = {10, 20};
        s2.deltas = std::vector<double>{1.5};

        Item itemA;
        itemA.name = "alpha";
        itemA.category = Category::Core;
        itemA.importance = Importance::High;
        itemA.stats = s1;
        itemA.note = std::string("primary");
        itemA.tags = {"stable", "fast"};

        Item itemB;
        itemB.name = "beta";
        itemB.category = Category::Experimental;
        itemB.importance = Importance::Medium;
        itemB.stats = s2;
        itemB.tags = {"experimental"};

        Document doc;
        doc.version = 3;
        doc.scale = 1.5f;
        doc.items = {itemA, itemB};
        doc.featured = itemB;

        Json::Value json;
        auto enc = aison::encode<BasicSchema>(doc, json);
        REQUIRE(enc);
        REQUIRE(enc.errors.empty());

        Document decoded;
        auto dec = aison::decode<BasicSchema>(json, decoded);
        REQUIRE(dec);
        REQUIRE(dec.errors.empty());

        CHECK(decoded.version == doc.version);
        CHECK(decoded.scale == doctest::Approx(doc.scale));
        REQUIRE(decoded.items.size() == doc.items.size());

        for (std::size_t i = 0; i < decoded.items.size(); ++i) {
            const auto& a = doc.items[i];
            const auto& b = decoded.items[i];
            CHECK(b.name == a.name);
            CHECK(b.category == a.category);
            CHECK(b.importance == a.importance);
            CHECK(b.stats.count == a.stats.count);
            CHECK(b.stats.mean == doctest::Approx(a.stats.mean));
            CHECK(b.stats.buckets == a.stats.buckets);
            REQUIRE(b.stats.deltas.has_value() == a.stats.deltas.has_value());
            if (b.stats.deltas && a.stats.deltas) {
                REQUIRE(b.stats.deltas->size() == a.stats.deltas->size());
                for (std::size_t j = 0; j < b.stats.deltas->size(); ++j) {
                    CHECK((*b.stats.deltas)[j] == doctest::Approx((*a.stats.deltas)[j]));
                }
            }
            CHECK(b.note == a.note);
            CHECK(b.tags == a.tags);
        }

        REQUIRE(decoded.featured.has_value());
        CHECK(decoded.featured->name == doc.featured->name);
        CHECK(decoded.featured->category == doc.featured->category);
    }

    TEST_CASE("Decode alias names and canonicalize on encode")
    {
        Json::Value json(Json::objectValue);
        json["version"] = 1;
        json["scale"] = 2.0;
        json["items"] = Json::arrayValue;

        Json::Value jItem(Json::objectValue);
        jItem["name"] = "gamma";
        jItem["category"] = "exp";      // alias
        jItem["importance"] = "med";    // alias
        jItem["stats"] = Json::objectValue;
        jItem["stats"]["count"] = 1;
        jItem["stats"]["mean"] = 5.0;
        jItem["stats"]["buckets"] = Json::arrayValue;
        jItem["stats"]["buckets"].append(5);
        // deltas omitted -> disengaged
        jItem["tags"] = Json::arrayValue;
        jItem["tags"].append("aliased");
        // note omitted -> disengaged

        json["items"].append(jItem);
        // featured omitted -> disengaged

        Document decoded;
        auto dec = aison::decode<BasicSchema>(json, decoded);
        REQUIRE(dec);
        REQUIRE(dec.errors.empty());

        CHECK(decoded.version == 1);
        CHECK(decoded.scale == doctest::Approx(2.0f));
        REQUIRE(decoded.items.size() == 1);
        const auto& item = decoded.items.front();
        CHECK(item.category == Category::Experimental);
        CHECK(item.importance == Importance::Medium);
        CHECK_FALSE(item.stats.deltas.has_value());
        CHECK_FALSE(item.note.has_value());
        CHECK(decoded.featured == std::nullopt);

        // Encode back and ensure canonical enum names are produced
        Json::Value reJson;
        auto enc = aison::encode<BasicSchema>(decoded, reJson);
        REQUIRE(enc);
        REQUIRE(enc.errors.empty());

        const auto& outItem = reJson["items"][0U];
        CHECK(outItem["category"].asString() == "experimental");
        CHECK(outItem["importance"].asString() == "medium");
        CHECK_FALSE(outItem.isMember("note"));
        CHECK_FALSE(outItem["stats"].isMember("deltas"));
        CHECK_FALSE(reJson.isMember("featured"));
    }
}

}  // namespace

