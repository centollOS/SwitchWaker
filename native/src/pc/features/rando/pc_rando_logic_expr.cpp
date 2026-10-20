// pc_rando_logic_expr.cpp - requirement compiler and evaluator (docs/RANDOMIZER_PLAN.md step R1.1).
// A port of wwrando 1.10.0_9775811 logic/logic.py; see pc_rando_logic_expr.h for what is kept exact.
#include "pc_rando_logic_expr.h"

#include <string.h>

#include "pc_rando_data.h"

namespace pcr {

namespace {

std::string_view strip(std::string_view s) {
    // Python str.strip(): ASCII whitespace is all the data holds.
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\n' || s[b] == '\r' || s[b] == '\f' || s[b] == '\v')) {
        b++;
    }
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\n' || s[e - 1] == '\r' ||
                     s[e - 1] == '\f' || s[e - 1] == '\v')) {
        e--;
    }
    return s.substr(b, e - b);
}

bool startsWith(std::string_view s, std::string_view p) { return s.size() >= p.size() && s.substr(0, p.size()) == p; }
bool endsWith(std::string_view s, std::string_view p) {
    return s.size() >= p.size() && s.substr(s.size() - p.size()) == p;
}

// re.search(r"^(<prefix>.+<suffix>) x(\d+)$", s): the greedy group ends at the last " x<digits>" that
// reaches the end of the string. Returns false when it does not match.
bool matchCount(std::string_view s, std::string_view prefix, std::string_view suffix, std::string_view& group,
                int& n) {
    size_t e = s.size();
    size_t d = e;
    while (d > 0 && s[d - 1] >= '0' && s[d - 1] <= '9') {
        d--;
    }
    if (d == e || d < 2 || s[d - 1] != 'x' || s[d - 2] != ' ') {
        return false;
    }
    group = s.substr(0, d - 2);
    // ".+" between the prefix and the suffix needs at least one character.
    if (group.size() < prefix.size() + suffix.size() + 1 || !startsWith(group, prefix) || !endsWith(group, suffix)) {
        return false;
    }
    long long v = 0;
    for (size_t i = d; i < e; i++) {
        v = v * 10 + (s[i] - '0');
        if (v > 1000000) {
            v = 1000000; // more than any count can reach
        }
    }
    n = (int)v;
    return true;
}

// re.search(r'^<head>"([^"]+)"<tail>$'): the quoted part of s after `head`, which must be followed by
// exactly `tail`.
bool matchQuoted(std::string_view s, std::string_view head, std::string_view& q, std::string_view& rest) {
    if (!startsWith(s, head) || s.size() <= head.size() || s[head.size()] != '"') {
        return false;
    }
    size_t b = head.size() + 1;
    size_t e = s.find('"', b);
    if (e == std::string_view::npos || e == b) {
        return false;
    }
    q = s.substr(b, e - b);
    rest = s.substr(e + 1);
    return true;
}

} // namespace

ReqLogic::ReqLogic() {}

void ReqLogic::fail(const std::string& msg) {
    if (mError.empty()) {
        mError = msg;
    }
}

void ReqLogic::growCaches() {
    size_t n = mNames.size();
    if (mOwned.size() < n) {
        mOwned.resize(n, 0);
        mMetGen.resize(n, 0);
        mMetVal.resize(n, 0);
        mNeedGen.resize(n, 0);
        mNeed.resize(n);
    }
}

int ReqLogic::intern(std::string_view s) {
    auto it = mIndex.find(std::string(s));
    if (it != mIndex.end()) {
        return it->second;
    }
    int id = (int)mNames.size();
    Name n;
    n.text.assign(s.data(), s.size());
    mNames.push_back(n);
    mIndex.emplace(mNames.back().text, id);
    growCaches();
    return id;
}

int ReqLogic::find(std::string_view s) const {
    auto it = mIndex.find(std::string(s));
    return it == mIndex.end() ? -1 : it->second;
}

bool ReqLogic::init(const Options& opts, const std::vector<std::string>& cleanedItemNames) {
    mOpts = opts;
    mNames.clear();
    mIndex.clear();
    mItemNames.clear();
    mOwned.clear();
    mMacroOrder.clear();
    mMacroBodies.clear();
    mNestedKeys.clear();
    mLocExpr.clear();
    mLocName.clear();
    mLocIndex.clear();
    mExprs.clear();
    mTokens.clear();
    mCompiled.clear();
    mBadTexts.clear();
    mMetGen.clear();
    mMetVal.clear();
    mNeedGen.clear();
    mNeed.clear();
    mGeneration = 1;
    mMetDepth = 0;
    mError.clear();

    for (const std::string& item : cleanedItemNames) {
        int id = intern(item);
        if (!mNames[id].isItem) {
            mNames[id].isItem = true;
            mItemNames.push_back(id);
        }
    }
    // The location table first, so Can Access Item Location tokens resolve while macros compile.
    Span<Location> locs = locations();
    for (size_t i = 0; i < locs.size; i++) {
        mLocIndex.emplace(locs[i].name, (int)i);
        mLocName.push_back(intern(locs[i].name));
    }
    for (const Macro& m : macros()) {
        if (!setMacro(m.name, m.body)) {
            return false;
        }
    }
    mLocExpr.resize(locs.size, -1);
    for (size_t i = 0; i < locs.size; i++) {
        int e = compile(locs[i].need);
        if (e < 0) {
            return false;
        }
        mLocExpr[i] = e;
    }
    mGeneration++;
    return !failed();
}

// --- Inventory -----------------------------------------------------------------------------------

void ReqLogic::addOwned(int id) {
    if (!isItem(id)) {
        fail("Unknown item name: " + (id >= 0 && id < (int)mNames.size() ? mNames[id].text : std::string("?")));
        return;
    }
    if (mOwned[id] < 0xFFFF) {
        mOwned[id]++;
    }
    mGeneration++;
}

void ReqLogic::removeOwned(int id) {
    if (!isItem(id)) {
        fail("Unknown item name: " + (id >= 0 && id < (int)mNames.size() ? mNames[id].text : std::string("?")));
        return;
    }
    if (mOwned[id] == 0) {
        fail("list.remove(x): x not in list: " + mNames[id].text);
        return;
    }
    mOwned[id]--;
    mGeneration++;
}

void ReqLogic::setOwned(int id, int count) {
    if (id < 0 || id >= (int)mNames.size()) {
        return;
    }
    mOwned[id] = (uint16_t)(count < 0 ? 0 : count > 0xFFFF ? 0xFFFF : count);
    mGeneration++;
}

void ReqLogic::clearOwned() {
    for (uint16_t& c : mOwned) {
        c = 0;
    }
    mGeneration++;
}

// --- Macros --------------------------------------------------------------------------------------

bool ReqLogic::setMacro(std::string_view macroName, std::string_view body) {
    int e = compile(body);
    if (e < 0) {
        return false;
    }
    int id = intern(macroName);
    Name& n = mNames[id];
    if (n.macroExpr < 0) {
        mMacroOrder.push_back(id);
        n.macroBody = (int)mMacroBodies.size();
        mMacroBodies.emplace_back(body);
    } else {
        mMacroBodies[n.macroBody].assign(body.data(), body.size());
    }
    n.macroExpr = e;
    mGeneration++;
    return true;
}

const std::string* ReqLogic::macroBody(int id) const {
    return hasMacro(id) ? &mMacroBodies[mNames[id].macroBody] : nullptr;
}

void ReqLogic::setNestedEntranceMacro(std::string_view entranceMacro, std::string_view zoneMacro) {
    int k = intern(entranceMacro);
    int v = intern(zoneMacro);
    if (mNames[k].nestedOuter < 0) {
        mNestedKeys.push_back(k);
    }
    mNames[k].nestedOuter = v;
}

void ReqLogic::clearNestedEntranceMacros() {
    for (int k : mNestedKeys) {
        mNames[k].nestedOuter = -1;
    }
    mNestedKeys.clear();
}

// --- Compiler ------------------------------------------------------------------------------------

// check_option_enabled_requirement, folded once: the options cannot change within a Logic.
bool ReqLogic::optionToken(const std::string& tok, bool& value, std::string& err) const {
    std::string_view s(tok), opt, rest;
    if (!matchQuoted(s, "Option ", opt, rest)) {
        err = "Invalid option check requirement: " + tok;
        return false;
    }
    enum { Enabled, Disabled, Is, IsNot, Contains, NotContains } mode;
    std::string_view v;
    std::string_view r2;
    if (rest == " Enabled") {
        mode = Enabled;
    } else if (rest == " Disabled") {
        mode = Disabled;
    } else if (matchQuoted(rest, " Is ", v, r2) && r2.empty()) {
        mode = Is;
    } else if (matchQuoted(rest, " Is Not ", v, r2) && r2.empty()) {
        mode = IsNot;
    } else if (matchQuoted(rest, " Contains ", v, r2) && r2.empty()) {
        mode = Contains;
    } else if (matchQuoted(rest, " Does Not Contain ", v, r2) && r2.empty()) {
        mode = NotContains;
    } else {
        err = "Invalid option check requirement: " + tok;
        return false;
    }
    std::string optName(opt);
    int id = optionFind(optName.c_str());
    if (id < 0) {
        err = "'" + optName + "' is not a valid option name"; // Options.__getitem__ raises KeyError
        return false;
    }
    const OptionInfo& info = kOptionInfo[id];
    std::string text = optionToString(mOpts, id);
    std::string val(v);
    switch (mode) {
    case Enabled:
    case Disabled: {
        bool truthy;
        switch (info.kind) {
        case OptKind::Bool:
        case OptKind::Int:
            truthy = optionGetInt(mOpts, id) != 0;
            break;
        case OptKind::Enum: // a StrEnum value is a non-empty str
        case OptKind::Str:
            truthy = !text.empty();
            break;
        case OptKind::List:
            truthy = !(id == OPT_randomized_gear ? mOpts.randomized_gear : mOpts.starting_gear).empty();
            break;
        case OptKind::Dict:
        default: {
            std::string_view t = strip(text);
            truthy = !(t.empty() || t == "{}");
            break;
        }
        }
        value = mode == Enabled ? truthy : !truthy;
        return true;
    }
    case Is:
    case IsNot: {
        // A bool, int, list or dict never equals a str; a StrEnum or str compares its text.
        bool eq = (info.kind == OptKind::Enum || info.kind == OptKind::Str) && text == val;
        value = mode == Is ? eq : !eq;
        return true;
    }
    case Contains:
    case NotContains: {
        bool in;
        if (info.kind == OptKind::List) {
            const std::vector<std::string>& l =
                id == OPT_randomized_gear ? mOpts.randomized_gear : mOpts.starting_gear;
            in = false;
            for (const std::string& x : l) {
                in = in || x == val;
            }
        } else if (info.kind == OptKind::Enum || info.kind == OptKind::Str) {
            in = text.find(val) != std::string::npos; // str.__contains__: a substring
        } else if (info.kind == OptKind::Dict) {
            in = text.find("\"" + val + "\"") != std::string::npos; // a key of the dict
        } else {
            err = "argument of type '" + std::string(info.kind == OptKind::Bool ? "bool" : "int") +
                  "' is not iterable";
            return false;
        }
        value = mode == Contains ? in : !in;
        return true;
    }
    }
    return false;
}

// Decides what a requirement token is, in check_requirement_met's order. Counted names and locations
// are interned here, never while checking.
void ReqLogic::parseToken(int id) {
    if (mNames[id].kind != Kind::Unparsed) {
        return;
    }
    std::string text = mNames[id].text;
    std::string_view s(text);
    Kind kind = Kind::Name;
    int target = -1, count = 0;
    bool cval = false;
    std::string bad;
    std::string_view group;
    if (startsWith(s, "Progressive ")) {
        if (matchCount(s, "Progressive ", "", group, count)) {
            kind = Kind::Count;
            target = intern(group);
        } else {
            kind = Kind::Bad;
            bad = "Malformed progressive item requirement: " + text;
        }
    } else if (s.find(" Small Key x") != std::string_view::npos) {
        if (matchCount(s, "", " Small Key", group, count)) {
            kind = Kind::Count;
            target = intern(group);
        } else {
            kind = Kind::Bad;
            bad = "Malformed small key requirement: " + text;
        }
    } else if (startsWith(s, "Can Access Item Location \"")) {
        std::string_view loc, rest;
        if (matchQuoted(s, "Can Access Item Location ", loc, rest) && rest.empty()) {
            auto it = mLocIndex.find(std::string(loc));
            if (it != mLocIndex.end()) {
                kind = Kind::Loc;
                target = it->second;
            } else {
                kind = Kind::Bad;
                bad = "KeyError: '" + std::string(loc) + "'";
            }
        } else {
            kind = Kind::Bad;
            bad = "Malformed item location requirement: " + text;
        }
    } else if (startsWith(s, "Option \"")) {
        if (optionToken(text, cval, bad)) {
            kind = Kind::Const;
        } else {
            kind = Kind::Bad;
        }
    }
    Name& n = mNames[id]; // intern above may have moved the table
    n.kind = kind;
    n.target = target;
    n.count = count;
    n.constVal = cval;
    if (kind == Kind::Bad) {
        n.badText = (int)mBadTexts.size();
        mBadTexts.push_back(bad);
    }
}

// parse_logic_expression on tokens from re.split("([&|()])") with each token stripped and empty ones
// dropped. One level: the operators seen decide the operator (both kinds: Mixed, an error when
// checked); two operands without an operator between them (Adjacent) fail Python's assert when
// checked. Leading, trailing and repeated operators are accepted, as wwrando's loop does.
int ReqLogic::compileTokens(const std::vector<std::string_view>& toks, size_t& pos, int depth, bool& ok) {
    std::vector<int> items;
    bool sawAnd = false, sawOr = false, adjacent = false;
    bool prevOperand = false;
    bool closed = false;
    while (pos < toks.size()) {
        std::string_view t = toks[pos++];
        if (t == ")") {
            if (depth == 0) {
                ok = false; // a ")" without its "("
                return -1;
            }
            closed = true;
            break;
        }
        if (t == "&" || t == "|") {
            (t == "&" ? sawAnd : sawOr) = true;
            prevOperand = false;
            continue;
        }
        if (prevOperand) {
            adjacent = true;
        }
        prevOperand = true;
        if (t == "(") {
            int sub = compileTokens(toks, pos, depth + 1, ok);
            if (!ok) {
                return -1;
            }
            items.push_back(~sub);
        } else {
            int id = intern(t);
            parseToken(id);
            items.push_back(id);
        }
    }
    if (depth > 0 && !closed) {
        ok = false; // a "(" without its ")"
        return -1;
    }
    Expr e;
    e.op = adjacent ? Op::Adjacent : (sawAnd && sawOr) ? Op::Mixed : sawOr ? Op::Or : Op::And;
    e.first = (uint32_t)mTokens.size();
    e.count = (uint32_t)items.size();
    for (int x : items) {
        mTokens.push_back(x);
    }
    mExprs.push_back(e);
    return (int)mExprs.size() - 1;
}

int ReqLogic::compile(std::string_view text) {
    std::string key(text);
    auto it = mCompiled.find(key);
    if (it != mCompiled.end()) {
        return it->second;
    }
    std::vector<std::string_view> toks;
    size_t b = 0;
    for (size_t i = 0; i <= text.size(); i++) {
        if (i == text.size() || text[i] == '&' || text[i] == '|' || text[i] == '(' || text[i] == ')') {
            std::string_view piece = strip(text.substr(b, i - b));
            if (!piece.empty()) {
                toks.push_back(piece);
            }
            if (i < text.size()) {
                toks.push_back(text.substr(i, 1));
            }
            b = i + 1;
        }
    }
    size_t pos = 0;
    bool ok = true;
    int e = compileTokens(toks, pos, 0, ok);
    if (!ok || pos != toks.size()) {
        fail("Unbalanced parentheses in requirement: " + key);
        return -1;
    }
    mCompiled.emplace(key, e);
    return e;
}

void ReqLogic::topLevelTokens(int expr, std::vector<int>& out) const {
    out.clear();
    if (expr < 0 || expr >= (int)mExprs.size()) {
        return;
    }
    const Expr& e = mExprs[expr];
    for (uint32_t i = 0; i < e.count; i++) {
        int t = mTokens[e.first + i];
        if (t >= 0) {
            out.push_back(t);
        }
    }
}

// --- Met checks ----------------------------------------------------------------------------------

// check_logical_expression_req: every child is checked, then any()/all().
bool ReqLogic::metExpr(int expr) {
    const Expr e = mExprs[expr];
    if (e.op == Op::Mixed) {
        fail("Error parsing progression requirements: & and | must not be within the same nesting level.");
        return false;
    }
    if (e.op == Op::Adjacent) {
        fail("Invalid expression (two operands without an operator)");
        return false;
    }
    bool anyTrue = false, allTrue = true;
    for (uint32_t i = 0; i < e.count; i++) {
        int t = mTokens[e.first + i];
        bool r = t < 0 ? metExpr(~t) : metReq(t);
        anyTrue = anyTrue || r;
        allTrue = allTrue && r;
    }
    return e.op == Op::Or ? anyTrue : allTrue;
}

// check_requirement_met with the current stack.
bool ReqLogic::metReq(int id) {
    if (mMetGen[id] == mGeneration) {
        return mMetVal[id] != 0;
    }
    Name& n0 = mNames[id];
    if (n0.nestedOuter >= 0 && mNames[n0.nestedOuter].onMetStack) {
        return true; // not cached, as in Python
    }
    if (n0.onMetStack) {
        fail("Recursive requirement check on non-whitelisted macro: " + n0.text);
        return false;
    }
    n0.onMetStack = true;
    mMetDepth++;
    bool result = false;
    // mNames does not grow while checking, so the reference stays valid.
    Name& n = mNames[id];
    switch (n.kind) {
    case Kind::Count:
        result = owned(n.target) >= n.count;
        break;
    case Kind::Loc:
        result = metExpr(mLocExpr[n.target]);
        break;
    case Kind::Const:
        result = n.constVal;
        break;
    case Kind::Name:
        if (n.isItem) {
            result = mOwned[id] > 0;
        } else if (n.macroExpr >= 0) {
            result = metExpr(n.macroExpr);
        } else if (n.text == "Nothing") {
            result = true;
        } else if (n.text == "Impossible") {
            result = false;
        } else {
            fail("Unknown requirement name: " + n.text);
        }
        break;
    case Kind::Bad:
        fail(mBadTexts[n.badText]);
        break;
    case Kind::Unparsed:
        fail("Unparsed requirement: " + n.text);
        break;
    }
    n.onMetStack = false;
    mMetDepth--;
    mMetGen[id] = mGeneration;
    mMetVal[id] = result ? 1 : 0;
    return result;
}

bool ReqLogic::checkRequirementMet(std::string_view req) {
    int id = intern(strip(req));
    parseToken(id);
    return checkRequirementMet(id);
}

bool ReqLogic::checkRequirementMet(int id) {
    if (id < 0 || id >= (int)mNames.size()) {
        return false;
    }
    parseToken(id);
    return metReq(id);
}

bool ReqLogic::checkExpr(int expr) {
    if (expr < 0 || expr >= (int)mExprs.size()) {
        return false;
    }
    return metExpr(expr);
}

bool ReqLogic::checkLocationAccessible(int loc) { return checkExpr(locationExpr(loc)); }

// --- Items needed --------------------------------------------------------------------------------

void ReqLogic::mergeOne(ItemsNeeded& into, int name, int count) {
    for (ItemCount& c : into) {
        if (c.name == name) {
            if (count > c.count) {
                c.count = count;
            }
            return;
        }
    }
    into.push_back(ItemCount{name, count});
}

void ReqLogic::merge(ItemsNeeded& into, const ItemsNeeded& from) {
    for (const ItemCount& c : from) {
        mergeOne(into, c.name, c.count);
    }
}

// get_items_needed_from_logical_expression_req: {} when the expression is met (checked with a fresh
// stack: the met stack is always empty here), else the union over every child.
void ReqLogic::needExpr(int expr, ItemsNeeded& out) {
    out.clear();
    if (metExpr(expr)) {
        return;
    }
    const Expr e = mExprs[expr];
    ItemsNeeded sub;
    for (uint32_t i = 0; i < e.count; i++) {
        int t = mTokens[e.first + i];
        if (t < 0) {
            needExpr(~t, sub);
            merge(out, sub);
        } else {
            const ItemsNeeded* r = needReq(t);
            if (r != nullptr) {
                merge(out, *r);
            }
        }
    }
}

// get_items_needed_by_req_name with the current stack. nullptr stands for a {} that is not cached.
const ItemsNeeded* ReqLogic::needReq(int id) {
    if (mNeedGen[id] == mGeneration) {
        return &mNeed[id];
    }
    Name& n0 = mNames[id];
    if (n0.nestedOuter >= 0 && mNames[n0.nestedOuter].onNeedStack) {
        return nullptr;
    }
    if (n0.onNeedStack) {
        fail("Recursive requirement check on non-whitelisted macro: " + n0.text);
        return nullptr;
    }
    n0.onNeedStack = true;
    ItemsNeeded result;
    Name& n = mNames[id];
    switch (n.kind) {
    case Kind::Count:
        result.push_back(ItemCount{n.target, n.count});
        break;
    case Kind::Loc:
        needExpr(mLocExpr[n.target], result);
        break;
    case Kind::Const:
        break;
    case Kind::Name:
        if (n.isItem) {
            result.push_back(ItemCount{id, 1});
        } else if (n.macroExpr >= 0) {
            needExpr(n.macroExpr, result);
        } else if (n.text == "Nothing" || n.text == "Impossible") {
        } else {
            fail("Unknown requirement name: " + n.text);
        }
        break;
    case Kind::Bad:
        fail(mBadTexts[n.badText]);
        break;
    case Kind::Unparsed:
        fail("Unparsed requirement: " + n.text);
        break;
    }
    n.onNeedStack = false;
    mNeedGen[id] = mGeneration;
    mNeed[id] = std::move(result);
    return &mNeed[id];
}

void ReqLogic::itemsNeededByReq(std::string_view req, ItemsNeeded& out) {
    int id = intern(strip(req));
    parseToken(id);
    itemsNeededByReq(id, out);
}

void ReqLogic::itemsNeededByReq(int id, ItemsNeeded& out) {
    out.clear();
    if (id < 0 || id >= (int)mNames.size()) {
        return;
    }
    parseToken(id);
    const ItemsNeeded* r = needReq(id);
    if (r != nullptr) {
        out = *r;
    }
}

void ReqLogic::itemsNeededForExpr(int expr, ItemsNeeded& out) {
    out.clear();
    if (expr < 0 || expr >= (int)mExprs.size()) {
        return;
    }
    needExpr(expr, out);
}

void ReqLogic::flatten(const ItemsNeeded& needed, std::vector<int>& names) {
    names.clear();
    for (const ItemCount& c : needed) {
        for (int i = 0; i < c.count; i++) {
            names.push_back(c.name);
        }
    }
}

// --- Snapshots -----------------------------------------------------------------------------------

ReqLogic::Snapshot ReqLogic::snapshot() const {
    Snapshot s;
    s.generation = mGeneration;
    s.owned = mOwned;
    s.metGen = mMetGen;
    s.metVal = mMetVal;
    s.needGen = mNeedGen;
    s.need = mNeed;
    return s;
}

// load_simulated_playthrough_state: the inventory and both caches as they were. Names interned since
// the snapshot start uncached. The generation continues from the larger one so that no stale entry can
// become valid again.
void ReqLogic::restore(const Snapshot& s) {
    size_t n = mNames.size();
    uint32_t next = (mGeneration > s.generation ? mGeneration : s.generation) + 1;
    for (size_t i = 0; i < n; i++) {
        bool have = i < s.owned.size();
        mOwned[i] = have ? s.owned[i] : 0;
        mMetGen[i] = have && s.metGen[i] == s.generation ? next : 0;
        mMetVal[i] = have ? s.metVal[i] : 0;
        mNeedGen[i] = have && s.needGen[i] == s.generation ? next : 0;
        if (have) {
            mNeed[i] = s.need[i];
        } else {
            mNeed[i].clear();
        }
    }
    mGeneration = next;
}

} // namespace pcr
