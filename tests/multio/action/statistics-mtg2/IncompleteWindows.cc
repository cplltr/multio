/*
 * (C) Copyright 1996- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */


#include <queue>

#include "eckit/testing/Test.h"

#include "multio/message/Message.h"
#include "multio/message/Metadata.h"

#include "../../MultioTestEnvironment.h"


namespace multio::test::statistics_mtg2 {

using multio::message::Message;
using multio::message::Metadata;
using multio::test::MultioTestEnvironment;


std::string makePlan(bool initialConditionPresent, bool emitIncompleteWindows) {
    std::string plan = std::string(R"json({
        "name": "MULTIO_TEST",
        "actions": [
            {
                "type": "statistics-mtg2",
                "output-frequency": "1m",
                "operations": [ "average" ],
                "options": {
                    "initial-condition-present": )json")
                           + (initialConditionPresent ? "true" : "false") + R"json(,
                    "emit-incomplete-windows": )json" + (emitIncompleteWindows ? "true" : "false")
                           + R"json(
                }
            },
            {
                "type": "debug-sink"
            }
        ]
    })json";
    return plan;
}

// Send hourly steps firstStep..lastStep of a forecast starting at date, then a last-step flush
void run(MultioTestEnvironment& env, int64_t date, int64_t firstStep, int64_t lastStep) {
    for (int64_t step = firstStep; step <= lastStep; ++step) {
        const double val = static_cast<double>(step);
        Metadata md{{{"param", 130},
                     {"levtype", "sfc"},
                     {"grid", "custom"},
                     {"stream", "clte"},
                     {"date", date},
                     {"time", 0000},
                     {"step", step},
                     {"misc-precision", "double"}}};
        eckit::Buffer pl{&val, sizeof(double)};
        Message msg{{Message::Tag::Field, {}, {}, std::move(md)}, std::move(pl)};
        EXPECT_NO_THROW(env.process(std::move(msg)));
    }

    Metadata flushMd{{{"flushKind", "last-step"}, {"step", lastStep}}};
    Message flush{{Message::Tag::Flush, {}, {}, std::move(flushMd)}, eckit::Buffer{}};
    EXPECT_NO_THROW(env.process(std::move(flush)));
}

// Drop the forwarded flush so that only emitted fields remain in the sink
std::size_t countFields(MultioTestEnvironment& env) {
    std::queue<Message> fields;
    auto& sink = env.debugSink();
    while (!sink.empty()) {
        if (sink.front().tag() == Message::Tag::Field) {
            fields.push(std::move(sink.front()));
        }
        sink.pop();
    }
    std::swap(sink, fields);
    return sink.size();
}


// A 2-day chunk only covers part of the month
CASE("partial month at end of run is emitted by default") {
    MultioTestEnvironment env{makePlan(true, true)};
    run(env, 19880101, 0, 48);
    EXPECT_EQUAL(countFields(env), 1);
    EXPECT_EQUAL(env.debugSink().front().metadata().get<std::int64_t>("timespan"), 48);
}

CASE("partial month at end of run is skipped") {
    MultioTestEnvironment env{makePlan(true, false)};
    run(env, 19880101, 0, 48);
    EXPECT_EQUAL(countFields(env), 0);
}

// A restarted chunk starting on the 3rd of January and running into February:
// January is incomplete at its start, February at its end
CASE("month started mid-way is skipped") {
    MultioTestEnvironment env{makePlan(false, false)};
    run(env, 19880103, 1, 24 * 31);  // 19880103 00 to 19880203 00
    EXPECT_EQUAL(countFields(env), 0);
}

// A full month is emitted, whether it is closed by the next message or by the flush
CASE("complete month is emitted") {
    MultioTestEnvironment env{makePlan(true, false)};
    run(env, 19880101, 0, 24 * 31 + 1);  // 19880101 00 to 19880201 01
    EXPECT_EQUAL(countFields(env), 1);
    const auto& field = env.debugSink().front();
    EXPECT_EQUAL(field.metadata().get<std::int64_t>("date"), 19880101);
    EXPECT_EQUAL(field.metadata().get<std::int64_t>("timespan"), 24 * 31);
}

CASE("complete month closed by the flush is emitted") {
    MultioTestEnvironment env{makePlan(true, false)};
    run(env, 19880101, 0, 24 * 31);  // 19880101 00 to 19880201 00
    EXPECT_EQUAL(countFields(env), 1);
    EXPECT_EQUAL(env.debugSink().front().metadata().get<std::int64_t>("timespan"), 24 * 31);
}


}  // namespace multio::test::statistics_mtg2

int main(int argc, char** argv) {
    return eckit::testing::run_tests(argc, argv);
}
