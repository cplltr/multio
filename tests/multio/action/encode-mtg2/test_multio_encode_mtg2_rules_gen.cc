/*
 * (C) Copyright 1996- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */


/// @author Philipp Geier

#include "eckit/testing/Test.h"
#include "multio/action/encode-mtg2/EncodeMtg2Exception.h"
#include "multio/action/encode-mtg2/EncoderConf.h"
#include "multio/action/encode-mtg2/Rules.h"
#include "multio/action/encode-mtg2/generated/InferPDT.h"
#include "multio/action/encode-mtg2/rules/Rule.h"
#include "multio/datamod/ContainerInterop.h"
#include "multio/datamod/DataModelling.h"

#include "multio/datamod/MarsMiscGeo.h"
#include "multio/message/Metadata.h"


namespace multio::test {

auto mkMd() {
    return message::Metadata{
        {"class", "ai"},  {"model", "aifs-single"}, {"expver", "0001"}, {"type", "fc"},   {"stream", "oper"},
        {"repres", "gg"}, {"packing", "ccsds"},     {"levelist", 700},  {"grid", "N320"}, {"step", 6},
        {"time", 0},      {"date", 20230901},       {"levtype", "pl"},  {"param", 133}};
}


CASE("Test rules gen matchers") {
    using namespace multio::action::rules;
    using namespace multio::action;
    using namespace multio::datamod;

    static auto ruleSet = exclusiveRuleList(
        // Branch for grids
        chainedRuleList(rule(all(Has<MarsKeys::GRID>{}, NoneOf<MarsKeys::LEVTYPE>{{"al"}})),
                        rule(OneOf<MarsKeys::PARAM>{{1, 3, 4}},
                             setAll(
                                 SetKey<PDTCatDef::TimeExtent, EncoderSectionsDef::Product, EncoderProductDef::PDTCat>{
                                     TimeExtent::PointInTime},  //
                                 SetKey<EncoderLevelDef::Type, EncoderSectionsDef::Product, EncoderProductDef::Level>{
                                     "heightAboveGround"}  //
                                 ))),

        // Branch for spherical harmonics
        chainedRuleList(rule(all(Has<MarsKeys::TRUNCATION>{}, NoneOf<MarsKeys::LEVTYPE>{{"al"}}))),

        // Branch for abstract level
        chainedRuleList(rule(OneOf<MarsKeys::LEVTYPE>{{"al"}})));

    {
        auto mars = MarsKeyValueSet{};
        EncoderSections sections;

        // Nothing should match the outer rule, which is allowed to not match
        EXPECT(ruleSet(mars, sections) != true);
    }
    
    {
        auto md = mkMd();
        md.set("param", 1);

        auto mars = read(MarsKeySet{}, md);
        EncoderSections sections;

        EXPECT(ruleSet(mars, sections));
        EXPECT_EQUAL((keyPath<EncoderSectionsDef::Product, EncoderProductDef::Level, EncoderLevelDef::Type>(sections).get()),
                     "heightAboveGround");
        EXPECT_EQUAL((keyPath<EncoderSectionsDef::Product, EncoderProductDef::PDTCat, PDTCatDef::TimeExtent>(sections).get()),
                     TimeExtent::PointInTime);
    }
    
    {
        auto md = mkMd();
        md.set("param", 42); // Not included in rule

        auto mars = read(MarsKeySet{}, md);
        EncoderSections sections;

        // First rule matches because grid is given, but then no param matches - the rule is not fully determined and throws
        EXPECT_THROWS(ruleSet(mars, sections));
    }
};

}  // namespace multio::test

int main(int argc, char** argv) {
    return eckit::testing::run_tests(argc, argv);
}
