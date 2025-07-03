#include "multio/action/encode-mtg2/Rules.h"
#include "multio/action/encode-mtg2/rules/Matcher.h"
#include "multio/action/encode-mtg2/rules/Rule.h"
#include "multio/action/encode-mtg2/rules/Setter.h"


namespace multio::action::rules_gen {
using namespace rules;
using namespace datamod;

static auto matchChemical() {
    return all(Has<MarsKeys::CHEM>{}, Missing<MarsKeys::WAVELENGTH>{}, LessThan<MarsKeys::CHEM>{900});
}

static auto matchAerosol() {
    return all(Has<MarsKeys::CHEM>{}, Missing<MarsKeys::WAVELENGTH>{}, GreaterEqual<MarsKeys::CHEM>{900});
}

static auto matchOptical() {
    return all(Missing<MarsKeys::CHEM>{}, Has<MarsKeys::WAVELENGTH>{});
}
static auto matchChemicalOptical() {
    return all(Has<MarsKeys::CHEM>{}, Has<MarsKeys::WAVELENGTH>{});
}


auto pointInTimeCat() { 
    return SetKey<PDTCatDef::TimeExtent, EncoderSectionsDef::Product, EncoderProductDef::PDTCat>{TimeExtent::PointInTime};
}
auto timeRangeCat() { 
    return SetKey<PDTCatDef::TimeExtent, EncoderSectionsDef::Product, EncoderProductDef::PDTCat>{TimeExtent::TimeRange};
}
auto level(const std::string& lvl) {
    return SetKey<EncoderLevelDef::Type, EncoderSectionsDef::Product, EncoderProductDef::Level>{lvl};
}


static const ExclusiveRuleList<MarsKeySet>& allRules() {
    static auto all_ = exclusiveRuleList(
        // Branch for grids
        chainedRuleList(rule(all(Has<MarsKeys::GRID>{}, NoneOf<MarsKeys::LEVTYPE>{{"al"}}))),

        // Branch for spherical harmonics
        chainedRuleList(rule(all(Has<MarsKeys::TRUNCATION>{}, NoneOf<MarsKeys::LEVTYPE>{{"al"}}))),

        // Branch for abstract level
        chainedRuleList(rule(OneOf<MarsKeys::LEVTYPE>{{"al"}})));
    return all_;
}

}  // namespace multio::action::rules_gen
