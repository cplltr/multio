#include "multio/action/encode-mtg2/Rules.h"
#include <type_traits>
#include "multio/action/encode-mtg2/EncoderConf.h"
#include "multio/action/encode-mtg2/generated/InferPDT.h"
#include "multio/action/encode-mtg2/rules/Matcher.h"
#include "multio/action/encode-mtg2/rules/Rule.h"
#include "multio/action/encode-mtg2/rules/Setter.h"
#include "multio/datamod/DataModelling.h"
#include "multio/datamod/MarsMiscGeo.h"


namespace multio::action::rules_gen {
using namespace rules;
using namespace datamod;

//-----------------------------------------------------------------------------
// Matchers
//-----------------------------------------------------------------------------

auto matchChemical() {
    return all(Has<MarsKeys::CHEM>{}, Missing<MarsKeys::WAVELENGTH>{}, LessThan<MarsKeys::CHEM>{900});
}

auto matchAerosol() {
    return all(Has<MarsKeys::CHEM>{}, Missing<MarsKeys::WAVELENGTH>{}, GreaterEqual<MarsKeys::CHEM>{900});
}

auto matchOptical() {
    return all(Missing<MarsKeys::CHEM>{}, Has<MarsKeys::WAVELENGTH>{});
}
auto matchChemicalOptical() {
    return all(Has<MarsKeys::CHEM>{}, Has<MarsKeys::WAVELENGTH>{});
}


std::unordered_set<KeyDefValueType_t<MarsKeys::PARAM>> paramRange(KeyDefValueType_t<MarsKeys::PARAM> start,
                                                                  KeyDefValueType_t<MarsKeys::PARAM> end) {
    using T = KeyDefValueType_t<MarsKeys::PARAM>;
    std::unordered_set<T> res;
    for (T i = start; i <= end; ++i) {
        res.insert(i);
    }
    return res;
}

auto matchParams(std::unordered_set<KeyDefValueType_t<MarsKeys::PARAM>> params) {
    return OneOf<MarsKeys::PARAM>{std::move(params)};
}

auto matchParams(KeyDefValueType_t<MarsKeys::PARAM> param) {
    return OneOf<MarsKeys::PARAM>{{param}};
}

template <typename Arg, typename... More, std::enable_if_t<((sizeof...(More)) > 0), bool> = true>
auto matchParams(Arg&& arg, More&&... more) {
    auto res = matchParams(std::forward<Arg>(arg));
    (res.values.merge(matchParams(std::forward<More>(more)).values), ...);
    return res;
}

auto matchLevType(const std::string& lt) {
    return OneOf<MarsKeys::LEVTYPE>{{lt}};
}


//-----------------------------------------------------------------------------
// Setters
//-----------------------------------------------------------------------------

// Category setters
auto pointInTime() {
    return setAll(
        SetKey<PDTCatDef::TimeExtent, EncoderSectionsDef::Product, EncoderProductDef::PDTCat>{TimeExtent::PointInTime},
        SetKey<EncoderProductDef::PointInTime, EncoderSectionsDef::Product>{});
}
auto timeRange(const std::string& type, const std::string& typeOfStatisticalProcessing) {
    return setAll(
        SetKey<PDTCatDef::TimeExtent, EncoderSectionsDef::Product, EncoderProductDef::PDTCat>{TimeExtent::TimeRange},
        SetKey<EncoderTimeRangeDef::Type, EncoderSectionsDef::Product, EncoderProductDef::TimeRange>{type},
        SetKey<EncoderTimeRangeDef::TypeOfStatisticalProcessing, EncoderSectionsDef::Product,
               EncoderProductDef::TimeRange>{typeOfStatisticalProcessing});
}
auto overallLengthOfTimeRange(const std::string& l) {
    return SetKey<EncoderTimeRangeDef::OverallLengthOfTimeRange, EncoderSectionsDef::Product,
                  EncoderProductDef::TimeRange>{l};
}

auto ensemble() {
    return SetKey<PDTCatDef::ProcessSubType, EncoderSectionsDef::Product, EncoderProductDef::PDTCat>{
        ProcessSubType::Ensemble};
}
auto largeEnsemble() {
    return SetKey<PDTCatDef::ProcessSubType, EncoderSectionsDef::Product, EncoderProductDef::PDTCat>{
        ProcessSubType::LargeEnsemble};
}
auto reforecast() {
    return SetKey<PDTCatDef::ProcessType, EncoderSectionsDef::Product, EncoderProductDef::PDTCat>{
        ProcessType::Reforecast};
}

auto chemical() {
    return setAll(SetKey<EncoderProductDef::Chemical, EncoderSectionsDef::Product>{},
                  SetKey<PDTCatDef::ProductCategory, EncoderSectionsDef::Product, EncoderProductDef::PDTCat>{
                      ProductCategory::Chemical});
}

auto periodRange() {
    return setAll(SetKey<EncoderProductDef::PeriodRange, EncoderSectionsDef::Product>{},
                  SetKey<PDTCatDef::ProductCategory, EncoderSectionsDef::Product, EncoderProductDef::PDTCat>{
                      ProductCategory::Wave},
                  SetKey<PDTCatDef::ProductSubCategory, EncoderSectionsDef::Product, EncoderProductDef::PDTCat>{
                      ProductSubCategory::PeriodRange});
}

auto dirFreq() {
    return setAll(SetKey<EncoderProductDef::DirFreq, EncoderSectionsDef::Product>{},
                  SetKey<PDTCatDef::ProductCategory, EncoderSectionsDef::Product, EncoderProductDef::PDTCat>{
                      ProductCategory::Wave},
                  SetKey<PDTCatDef::ProductSubCategory, EncoderSectionsDef::Product, EncoderProductDef::PDTCat>{
                      ProductSubCategory::SpectraList});
}

auto satellite() {
    return setAll(SetKey<EncoderProductDef::Satellite, EncoderSectionsDef::Product>{},
                  SetKey<PDTCatDef::ProductCategory, EncoderSectionsDef::Product, EncoderProductDef::PDTCat>{
                      ProductCategory::Satellite});
}

auto randomPattern() {
    return setAll(SetKey<EncoderProductDef::RandomPatterns, EncoderSectionsDef::Product>{},
                  SetKey<PDTCatDef::SpatialExtent, EncoderSectionsDef::Product, EncoderProductDef::PDTCat>{
                      SpatialExtent::RandomPatterns});
}

// Other setters

auto typeOfLevel(const std::string& lvl) {
    return SetKey<EncoderLevelDef::Type, EncoderSectionsDef::Product, EncoderProductDef::Level>{lvl};
}
auto localUse(std::int64_t num) {
    return SetKey<EncoderLocalUseDef::TemplateNumber, EncoderSectionsDef::LocalUse>{num};
}

auto dataRepres(std::int64_t num) {
    return SetKey<EncoderDataRepresDef::TemplateNumber, EncoderSectionsDef::DataRepres>{num};
}


auto tablesConfig(const std::string& type) {
    return SetKey<EncoderTablesDef::Type, EncoderSectionsDef::Identification, EncoderIdentificationDef::Tables>{type};
}

auto tablesVersion(std::int64_t version) {
    return SetKey<EncoderTablesDef::TablesVersion, EncoderSectionsDef::Identification,
                  EncoderIdentificationDef::Tables>{version};
}

auto localTablesVersion(std::int64_t version) {
    return SetKey<EncoderTablesDef::LocalTablesVersion, EncoderSectionsDef::Identification,
                  EncoderIdentificationDef::Tables>{version};
}


//-----------------------------------------------------------------------------
// Composed rules
//-----------------------------------------------------------------------------

auto makeGridRule(datamod::Repres repres, std::int64_t num) {
    return rule(OneOf<MarsKeys::REPRES>{{repres}},
                SetKey<EncoderGridDef::TemplateNumber, EncoderSectionsDef::Grid>{num});
}


auto gridRules() {
    return exclusiveRuleList(makeGridRule(Repres::GG, 40));
}
auto shGridRules() {
    return exclusiveRuleList(makeGridRule(Repres::SH, 50));
}


auto localSectionRules() {
    return exclusiveRuleList(  //
        rule(all(Missing<MarsKeys::ANOFFSET>{}, NoneOf<MarsKeys::CLASS>{{"d1"}}), localUse(1)),
        rule(all(Has<MarsKeys::ANOFFSET>{}, NoneOf<MarsKeys::CLASS>{{"d1"}}), localUse(36)),
        rule(all(Missing<MarsKeys::ANOFFSET>{}, NoneOf<MarsKeys::CLASS>{{"d1"}}), localUse(1001)));
}

auto processTypesRules() {
    return exclusiveRuleList(  //
        rule(all(Missing<MarsKeys::NUMBER>{}, Missing<MarsKeys::HDATE>{})),
        rule(all(Has<MarsKeys::NUMBER>{}, Missing<MarsKeys::HDATE>{}), ensemble()),
        rule(all(Has<MarsKeys::NUMBER>{}, Has<MarsKeys::HDATE>{}), reforecast(), ensemble()));
}

auto processTypesAlRules() {
    return exclusiveRuleList(  //
        rule(all(Has<MarsKeys::NUMBER>{}), largeEnsemble()));
}


auto packingRules() {
    return exclusiveRuleList(  //
        rule(OneOf<MarsKeys::PACKING>{{"simple"}}, dataRepres(0)),
        rule(OneOf<MarsKeys::PACKING>{{"ccsds"}}, dataRepres(42)));
}

auto packingSHRules() {
    return exclusiveRuleList(  //
        rule(OneOf<MarsKeys::PACKING>{{"complex"}}, dataRepres(52)));
}

//-----------------------------------------------------------------------------
// Params
//-----------------------------------------------------------------------------

auto paramSFCRules() {
    return exclusiveRuleList(                                //
        rule(all(matchLevType("sfc"), matchParams(228023)),  //
             pointInTime(), typeOfLevel("cloudbase")),       //
        rule(all(matchLevType("sfc"),                        //
                 matchParams(59, 78, 79, 136, 137, 164, 206, paramRange(162059, 162063), 162071, 162072, 162093, 228044,
                             228050, 228052, 228088, 228089, 228090, 228164, 260132)),       //
             pointInTime(), typeOfLevel("entireAtmosphere")),                                //
        rule(all(matchLevType("sfc"), matchParams(228007, 228011)),                          //
             pointInTime(), typeOfLevel("entireLake")),                                      //
        rule(all(matchLevType("sfc"), matchParams(121)),                                     //
             timeRange("fixed-timerange", "max"), overallLengthOfTimeRange("6h"),            //
             typeOfLevel("heightAboveGroundAt2m")),                                          //
        rule(all(matchLevType("sfc"), matchParams(122)),                                     //
             timeRange("fixed-timerange", "min"), overallLengthOfTimeRange("6h"),            //
             typeOfLevel("heightAboveGroundAt2m")),                                          //
        rule(all(matchLevType("sfc"), matchParams(201)),                                     //
             timeRange("since-last-post-processing-step", "max"),                            //
             typeOfLevel("heightAboveGroundAt2m")),                                          //
        rule(all(matchLevType("sfc"), matchParams(202)),                                     //
             timeRange("since-last-post-processing-step", "min"),                            //
             typeOfLevel("heightAboveGroundAt2m")),                                          //
        rule(all(matchLevType("sfc"), matchParams(123)),                                     //
             timeRange("fixed-timerange", "max"), overallLengthOfTimeRange("6h"),            //
             typeOfLevel("heightAboveGroundAt10m")),                                         //
        rule(all(matchLevType("sfc"), matchParams(228028)),                                  //
             timeRange("fixed-timerange", "max"), overallLengthOfTimeRange("3h"),            //
             typeOfLevel("heightAboveGroundAt10m")),                                         //
        rule(all(matchLevType("sfc"), matchParams(49)),                                      //
             timeRange("since-last-post-processing-step", "max"),                            //
             typeOfLevel("heightAboveGroundAt10m")),                                         //
        rule(all(matchLevType("sfc"), matchParams(235087, 235088, 235136, 235137, 235288)),  //
             timeRange("since-last-post-processing-step", "average"),                        //
             typeOfLevel("entireAtmosphere")),                                               //
        rule(all(matchLevType("sfc"), matchParams(228005, 235165, 235166)),                  //
             timeRange("since-last-post-processing-step", "average"),
             typeOfLevel("heightAboveGroundAt10m")),                                         //
        rule(all(matchLevType("sfc"), matchParams(235151)),                                  //
             timeRange("since-last-post-processing-step", "average"),                        //
             typeOfLevel("meanSea")),                                                        //
        rule(all(matchLevType("sfc"), matchParams(235039, 235040, 235049, 235050, 235053)),  //
             timeRange("since-last-post-processing-step", "average"),                        //
             typeOfLevel("nominalTop")),                                                     //
        rule(all(matchLevType("sfc"),                                                        //
                 matchParams(235020, 235021, 235031, paramRange(235033, 235038), paramRange(235041, 235043), 235051,
                             235052, 235055, 235078, 235079, 235134)),                      //
             timeRange("since-last-post-processing-step", "average"),                       //
             typeOfLevel("surface")),                                                       //
        rule(all(matchLevType("sfc"), matchParams(129172)),                                 //
             pointInTime(),                                                                 //
             typeOfLevel("heightAboveGround")),                                             //
        rule(all(matchLevType("sfc"), matchParams(165, 166, 207, 228029, 228131, 228132)),  //
             pointInTime(),                                                                 //
             typeOfLevel("heightAboveGroundAt10m")),                                        //
        rule(all(matchLevType("sfc"), matchParams(167, 168, 174096, 228037, 260242)),       //
             pointInTime(),                                                                 //
             typeOfLevel("heightAboveGroundAt2m")),                                         //
        rule(all(matchLevType("sfc"), matchParams(140245, 140249, 140233)),                 //
             pointInTime(),                                                                 //
             typeOfLevel("heightAboveSeaAt10m")),                                           //
        rule(all(matchLevType("sfc"), matchParams(3075)),                                   //
             pointInTime(),                                                                 //
             typeOfLevel("highCloudLayer")),                                                //
        rule(all(matchLevType("sfc"), matchParams(3074)),                                   //
             pointInTime(),                                                                 //
             typeOfLevel("mediumCloudLayer")),                                              //
        rule(all(matchLevType("sfc"), matchParams(3073)),                                   //
             pointInTime(),                                                                 //
             typeOfLevel("lowCloudLayer")),                                                 //
        rule(all(matchLevType("sfc"), matchParams(228014)),                                 //
             pointInTime(),                                                                 //
             typeOfLevel("iceLayerOnWater")),                                               //
        rule(all(matchLevType("sfc"), matchParams(228013)),                                 //
             pointInTime(),                                                                 //
             typeOfLevel("iceTopOnWater")),                                                 //
        rule(all(matchLevType("sfc"), matchParams(228010)),                                 //
             pointInTime(),                                                                 //
             typeOfLevel("lakeBottom")),                                                    //
        rule(all(matchLevType("sfc"), matchParams(151)),                                    //
             pointInTime(),                                                                 //
             typeOfLevel("meanSea")),                                                       //
        rule(all(matchLevType("sfc"), matchParams(262118)),                                 //
             pointInTime(),                                                                 //
             typeOfLevel("depthBelowSeaLayer")),                                            //
        rule(all(matchLevType("sfc"), matchParams(228231, 228232, 228233, 228234)),         //
             pointInTime(),                                                                 //
             typeOfLevel("mixedLayerParcel")),                                              //
        rule(all(matchLevType("sfc"), matchParams(228008, 228009)),                         //
             pointInTime(),                                                                 //
             typeOfLevel("mixingLayer")),                                                   //
        rule(all(matchLevType("sfc"), matchParams(228235, 228236, 228237)),                 //
             pointInTime(),                                                                 //
             typeOfLevel("mostUnstableParcel")),                                            //
        rule(all(matchLevType("sfc"), matchParams(178, 179, 208, 209, 212)),                //
             timeRange("since-beginning-of-forecast", "accumul"),                           //
             typeOfLevel("nominalTop")),                                                    //
        rule(all(matchLevType("sfc"), matchParams(235039, 235040)),                         //
             timeRange("since-last-post-processing-step", "average"),                       //
             typeOfLevel("nominalTop")),                                                    //
        rule(all(matchLevType("sfc"), matchParams(228045)),                                 //
             pointInTime(),                                                                 //
             typeOfLevel("tropopause")),                                                    //
        rule(all(matchLevType("sfc"),                                                       //
                 matchParams(228080, 228081, 228082, paramRange(233032, 233035), 235062, 235063, 235064),
                 matchChemical()),                                               //
             timeRange("since-beginning-of-forecast", "accumul"),                //
             chemical(),                                                         //
             typeOfLevel("surface"),                                             //
             tablesConfig("custom"), localTablesVersion(0), tablesVersion(30)),  //
        rule(all(matchLevType("sfc"),                                            //
                 matchParams(8, 9, 20, 44, 45, 47, 50, 57, 58, paramRange(142, 147), 169, 175, 176, 177, 180, 181, 182,
                             189, 195, 196, 197, 205, 210, 211, 213, 228, 239, 240, 3062, 3099,
                             paramRange(162100, 162113), paramRange(222001, 222256), 228021, 228022, 228129, 228130,
                             228143, 228144, 228216, 228228, 228251, 231001, 231002, 231003, 231005, 231010, 231012,
                             231057, 231058, paramRange(233000, 233031), 260259)),                 //
             timeRange("since-beginning-of-forecast", "accumul"), overallLengthOfTimeRange("1h"),  //
             typeOfLevel("surface")),                                                              //
        rule(all(matchLevType("sfc"),                                                              //
                 matchParams(228051, 228053)),                                                     //
             timeRange("fixed-timerange", "average"), overallLengthOfTimeRange("1h"),              //
             typeOfLevel("surface")),                                                              //
        rule(all(matchLevType("sfc"),                                                              //
                 matchParams(228057, 228059)),                                                     //
             timeRange("fixed-timerange", "average"), overallLengthOfTimeRange("3h"),              //
             typeOfLevel("surface")),                                                              //
        rule(all(matchLevType("sfc"),                                                              //
                 matchParams(228058, 228060)),                                                     //
             timeRange("fixed-timerange", "average"), overallLengthOfTimeRange("6h"),              //
             typeOfLevel("surface")),                                                              //
        rule(all(matchLevType("sfc"),                                                              //
                 matchParams(228026, 228222)),                                                     //
             timeRange("fixed-timerange", "max"), overallLengthOfTimeRange("3h"),                  //
             typeOfLevel("surface")),                                                              //
        rule(all(matchLevType("sfc"),                                                              //
                 matchParams(228224, 228035, 228036)),                                             //
             timeRange("fixed-timerange", "max"), overallLengthOfTimeRange("6h"),                  //
             typeOfLevel("surface")),                                                              //
        rule(all(matchLevType("sfc"),                                                              //
                 matchParams(228027, 228223)),                                                     //
             timeRange("fixed-timerange", "min"), overallLengthOfTimeRange("3h"),                  //
             typeOfLevel("surface")),                                                              //
        rule(all(matchLevType("sfc"),                                                              //
                 matchParams(228225)),                                                             //
             timeRange("fixed-timerange", "min"), overallLengthOfTimeRange("6h"),                  //
             typeOfLevel("surface")),                                                              //
        rule(all(matchLevType("sfc"),                                                              //
                 matchParams(paramRange(235033, 235038), 235189)),                                 //
             timeRange("since-last-post-processing-step", "average"),                              //
             typeOfLevel("surface")),                                                              //
        rule(all(matchLevType("sfc"),                                                              //
                 matchParams(260320)),                                                             //
             timeRange("fixed-timerange", "mode"), overallLengthOfTimeRange("1h"),                 //
             typeOfLevel("surface")),                                                              //
        rule(all(matchLevType("sfc"),                                                              //
                 matchParams(260321)),                                                             //
             timeRange("fixed-timerange", "mode"), overallLengthOfTimeRange("3h"),                 //
             typeOfLevel("surface")),                                                              //
        rule(all(matchLevType("sfc"),                                                              //
                 matchParams(260339)),                                                             //
             timeRange("fixed-timerange", "mode"), overallLengthOfTimeRange("6h"),                 //
             typeOfLevel("surface")),                                                              //
        rule(all(matchLevType("sfc"),                                                              //
                 matchParams(260318)),                                                             //
             timeRange("fixed-timerange", "severity"), overallLengthOfTimeRange("1h"),             //
             typeOfLevel("surface")),                                                              //
        rule(all(matchLevType("sfc"),                                                              //
                 matchParams(260319)),                                                             //
             timeRange("fixed-timerange", "severity"), overallLengthOfTimeRange("3h"),             //
             typeOfLevel("surface")),                                                              //
        rule(all(matchLevType("sfc"),                                                              //
                 matchParams(260338)),                                                             //
             timeRange("fixed-timerange", "severity"), overallLengthOfTimeRange("6h"),             //
             typeOfLevel("surface")),                                                              //
        rule(all(matchLevType("sfc"),                                                              //
                 matchParams(paramRange(15, 18), paramRange(26, 32), 33, paramRange(34, 43), paramRange(66, 67), 74,
                             129, 134, 139, 141, 148, 159, paramRange(160, 163), 170, paramRange(172, 174),
                             paramRange(186, 188), 198, paramRange(229, 232), paramRange(234, 236), 238,
                             paramRange(243, 245), 3020, 3067, 160198, 200199, 210200, 210201, 210202, 228003, 228012,
                             paramRange(210186, 210191), 210262, 210263, 210264, paramRange(228015, 228020), 228024,
                             228032, paramRange(228046, 228048), 228141, paramRange(228217, 228221), 260004, 260005,
                             260015, 260048, 260109, 260121, 260123, 260255, 260289, 260509, 261001, 261002, 261014,
                             261015, 261016, 261018, 262000, 262100, 262139, 262140, 262144, 262124)),  //
             pointInTime(),                                                                             //
             typeOfLevel("surface")),                                                                   //
        rule(all(matchLevType("sfc"),                                                                   //
                 matchParams(paramRange(228083, 228085)),                                               //
                 matchChemical()),                                                                      //
             pointInTime(),                                                                             //
             chemical(),
             typeOfLevel("surface")),  //
        rule(all(matchLevType("sfc"),  //
                 matchParams(paramRange(140098, 140105), paramRange(140112, 140113), paramRange(140121, 140129),
                             paramRange(140207, 140209), paramRange(140211, 140212), paramRange(140214, 140232),
                             paramRange(140234, 140239), 140244, paramRange(140252, 140254))),  //
             pointInTime(),                                                                     //
             typeOfLevel("surface")),                                                           //
        rule(all(matchLevType("sfc"),                                                           //
                 matchParams(paramRange(140114, 140120))),                                      //
             pointInTime(),                                                                     //
             periodRange(),
             typeOfLevel("surface")),                              //
        rule(all(matchLevType("sfc"),                              //
                 matchParams(paramRange(228226, 237055))),         //
             timeRange("since-last-post-processing-step", "max"),  //
             typeOfLevel("surface")),                              //
        rule(all(matchLevType("sfc"),                              //
                 matchParams(paramRange(228227, 238055))),         //
             timeRange("since-last-post-processing-step", "min"),  //
             typeOfLevel("surface")),                              //
        rule(all(matchLevType("sfc"),                              //
                 matchParams(140251)),                             //
             pointInTime(),                                        //
             dirFreq(),                                            //
             typeOfLevel("surface")),                              //
        rule(all(matchLevType("sfc"),                              //
                 matchParams(262104)),                             //
             pointInTime(),                                        //
             typeOfLevel("isothermal"))                            //
    );
}

//-----------------------------------------------------------------------------
// Params for SH
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Params for AL
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Big rule tree...
//-----------------------------------------------------------------------------

static const ExclusiveRuleList<MarsKeySet>& allRules() {
    static auto all_ = exclusiveRuleList(
        // Branch for grids
        chainedRuleList(                                                          //
            rule(all(Has<MarsKeys::GRID>{}, NoneOf<MarsKeys::LEVTYPE>{{"al"}})),  //
            gridRules(),                                                          //
            localSectionRules(),                                                  //
            processTypesRules(),                                                  //
            // paramRules(), //
            packingRules()  //
            ),

        // Branch for spherical harmonics
        chainedRuleList(                                                                //
            rule(all(Has<MarsKeys::TRUNCATION>{}, NoneOf<MarsKeys::LEVTYPE>{{"al"}})),  //
            shGridRules(),                                                              //
            localSectionRules(),                                                        //
            processTypesRules(),                                                        //
            // paramSHRules(), //
            packingSHRules()  //
            ),

        // Branch for abstract level
        chainedRuleList(               //
            rule(matchLevType("al")),  //
            gridRules(),               //
            localSectionRules(),       //
            processTypesAlRules(),     //
            // paramAlRules(),//
            packingRules()  //
            ));
    return all_;
}

}  // namespace multio::action::rules_gen
