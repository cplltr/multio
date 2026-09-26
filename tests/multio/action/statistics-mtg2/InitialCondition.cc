/*
 * (C) Copyright 1996- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */


#include "eckit/testing/Test.h"

#include "multio/message/Message.h"
#include "multio/message/Metadata.h"

#include "../../MultioTestEnvironment.h"


namespace multio::test::statistics_mtg2 {

using multio::message::Message;
using multio::message::Metadata;
using multio::test::MultioTestEnvironment;


// Daily average of an instantaneous field sent hourly from firstStep to step 24.
// Whether or not the solver sends the initial condition (step 0), the window must
// cover the full day: created at 00:00, spanning 24h, averaging steps 1 to 24.
void testDailyAverage(bool initialConditionPresent, int64_t firstStep) {
    const std::string plan = std::string(R"json({
        "name": "MULTIO_TEST",
        "actions": [
            {
                "type": "statistics-mtg2",
                "output-frequency": "1d",
                "operations": [ "average" ],
                "options": {
                    "initial-condition-present": )json")
                           + (initialConditionPresent ? "true" : "false") + R"json(
                }
            },
            {
                "type": "debug-sink"
            }
        ]
    })json";
    auto env = MultioTestEnvironment(plan);

    for (int64_t step = firstStep; step <= 24; ++step) {
        const double val = static_cast<double>(step);
        Metadata md{{{"param", 130},
                     {"levtype", "sfc"},
                     {"grid", "custom"},
                     {"stream", "clte"},
                     {"date", 19880201},
                     {"time", 0000},
                     {"step", step},
                     {"misc-precision", "double"}}};
        eckit::Buffer pl{&val, sizeof(double)};
        Message msg{{Message::Tag::Field, {}, {}, std::move(md)}, std::move(pl)};
        EXPECT_NO_THROW(env.process(std::move(msg)));
    }
    EXPECT_EQUAL(env.debugSink().size(), 0);

    Metadata flushMd{{{"flushKind", "last-step"}, {"step", 24}}};
    Message flush{{Message::Tag::Flush, {}, {}, std::move(flushMd)}, eckit::Buffer{}};
    EXPECT_NO_THROW(env.process(std::move(flush)));

    EXPECT_EQUAL(env.debugSink().size(), 2);
    const auto& field = env.debugSink().front();
    EXPECT(field.tag() == Message::Tag::Field);
    EXPECT_EQUAL(field.metadata().get<std::int64_t>("date"), 19880201);
    EXPECT_EQUAL(field.metadata().get<std::int64_t>("time"), 0);
    EXPECT_EQUAL(field.metadata().get<std::int64_t>("timespan"), 24);
    EXPECT_EQUAL(static_cast<const double*>(field.payload().data())[0], 12.5);  // mean of 1..24
}

CASE("initial condition present") {
    testDailyAverage(true, 0);
}

// e.g. a restarted run: the first message is one time increment into the window
CASE("initial condition not present") {
    testDailyAverage(false, 1);
}


}  // namespace multio::test::statistics_mtg2

int main(int argc, char** argv) {
    return eckit::testing::run_tests(argc, argv);
}
