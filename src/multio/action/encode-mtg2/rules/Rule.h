/*
 * (C) Copyright 1996- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation
 * nor does it submit to any jurisdiction.
 */

#pragma once

#include "multio/action/encode-mtg2/EncodeMtg2Exception.h"
#include "multio/action/encode-mtg2/rules/Matcher.h"
#include "multio/action/encode-mtg2/rules/Setter.h"

#include <memory>
#include <sstream>
#include <vector>


namespace multio::action::rules {


template <typename MatchKeySet_>
struct DynRule {
    using MatchKeySet = MatchKeySet_;

    virtual bool apply(const datamod::KeyValueSet<MatchKeySet_>& keys, EncoderSections&) const = 0;
};


template <typename MatchKeySet_, typename Derived>
struct DerivedRule : DynRule<MatchKeySet_> {
    using MatchKeySet = MatchKeySet_;
    bool apply(const datamod::KeyValueSet<MatchKeySet_>& keys, EncoderSections& conf) const override {
        return static_cast<const Derived&>(*this)(keys, conf);
    }
};


template <typename Matcher, typename Setter>
struct Rule : DerivedRule<typename Matcher::KeySet, Rule<Matcher, Setter>> {
    using MatchKeySet = typename Matcher::KeySet;

    Matcher matcher;
    Setter setter;

    // Match and set
    bool operator()(const datamod::KeyValueSet<MatchKeySet>& kvs, EncoderSections& conf) const {
        if (matcher(kvs)) {
            setter(conf);
            return true;
        }
        return false;
    }
};

template <typename Matcher_, typename Setter_>
auto rule(Matcher_&& matcher, Setter_&& setter) {
    Rule<std::decay_t<Matcher_>, std::decay_t<Setter_>> res;
    res.matcher = std::forward<Matcher_>(matcher);
    res.setter = std::forward<Setter_>(setter);
    return res;
}

template <typename Matcher_>
auto rule(Matcher_&& matcher) {
    Rule<std::decay_t<Matcher_>, NoOp> res;
    res.matcher = std::forward<Matcher_>(matcher);
    res.setter = NoOp{};
    return res;
    // return Rule<std::decay_t<Matcher_>, NoOp>(std::forward<Matcher_>(matcher), NoOp{});
}


template <typename MatchKeySet_>
struct ExclusiveRuleList : DerivedRule<MatchKeySet_, ExclusiveRuleList<MatchKeySet_>> {
    using MatchKeySet = MatchKeySet_;
    std::vector<std::unique_ptr<DynRule<MatchKeySet>>> rules;

    // Match and set
    bool operator()(const datamod::KeyValueSet<MatchKeySet>& kvs, EncoderSections& conf) const {
        DynRule<MatchKeySet>* appliedRule = nullptr;
        for (const auto& rule : rules) {
            if (rule->apply(kvs, conf)) {
                if (appliedRule != nullptr) {
                    std::ostringstream oss;
                    // using ReadWrite = typename std::decay_t<decltype(value)>::ReadWrite;
                    // TODO print rule
                    oss << "ExclusizeRuleList: Multiple rules apply although they should be exclusive.";
                    oss << " Keys: " << kvs;
                    throw EncodeMtg2Exception(oss.str(), Here());
                }
                appliedRule = rule.get();
            }
        }
        return (appliedRule != nullptr);
    }
};


template <typename Rule_, typename... Rules_>
ExclusiveRuleList<typename Rule_::MatchKeySet> exclusiveRuleList(Rule_&& rule, Rules_&&... rules) {
    using MatchKS = typename Rule_::MatchKeySet;
    ExclusiveRuleList<MatchKS> res;
    res.rules.emplace_back(std::make_unique<std::decay_t<Rule_>>(std::forward<Rule_>(rule)));
    (res.rules.emplace_back(std::make_unique<std::decay_t<Rules_>>(std::forward<Rules_>(rules))), ...);
    return res;
}


// Chains multiple rules on a struct matter.
// If the first rule applies, all others also have to apply - otherwise an exception is thrown to indicate that the key
// set is not definitely mapped.
// If the first rule does not apply, false is returned (and indicates that no modification happened)
// This is expected to be compbined
// with multiple ExclusiveRuleList to eventually form a combination of all partial rules.
template <typename MatchKeySet_>
struct ChainedRuleList : DerivedRule<MatchKeySet_, ChainedRuleList<MatchKeySet_>> {
    using MatchKeySet = MatchKeySet_;
    std::vector<std::unique_ptr<DynRule<MatchKeySet>>> rules;

    // Match and set
    bool operator()(const datamod::KeyValueSet<MatchKeySet>& kvs, EncoderSections& conf) const {
        bool first = true;
        for (const auto& rule : rules) {
            bool matched = rule->apply(kvs, conf);
            if (first) {
                // First failed - nothing has been applied yet
                if (!matched) {
                    return false;
                }
                first = false;
                continue;
            }

            if (!matched) {
                std::ostringstream oss;
                // TODO print rule
                oss << "ChainedRuleList: KeySet is not definitely matched. Some previous rules matched but an "
                       "intermediate rule failed.";
                oss << " Keys: " << kvs;
                throw EncodeMtg2Exception(oss.str(), Here());
            }
        }

        return true;
    }
};


template <typename Rule_, typename... Rules_>
ChainedRuleList<typename Rule_::MatchKeySet> chainedRuleList(Rule_&& rule, Rules_&&... rules) {
    using MatchKS = typename Rule_::MatchKeySet;
    ChainedRuleList<MatchKS> res;
    res.rules.emplace_back(std::make_unique<std::decay_t<Rule_>>(std::forward<Rule_>(rule)));
    (res.rules.emplace_back(std::make_unique<std::decay_t<Rules_>>(std::forward<Rules_>(rules))), ...);
    return res;
}

}  // namespace multio::action::rules

