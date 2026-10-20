// pc_rando_logic_expr.h - the randomizer's requirement compiler and evaluator (docs/RANDOMIZER_PLAN.md
// step R1.1): a port of wwrando 1.10.0_9775811's logic/logic.py
//   parse_logic_expression, check_requirement_met, check_logical_expression_req,
//   check_location_accessible, get_items_needed_by_req_name,
//   get_items_needed_from_logical_expression_req, flatten_items_needed_to_item_names,
//   check_progressive_item_req, check_small_key_req, check_item_location_requirement,
//   check_option_enabled_requirement, set_macro, clear_req_caches and the cache part of
//   save/load_simulated_playthrough_state.
//
// Pure C++ (no game headers): cos_rando_core and the unit tests link it. R1.3 (pc_rando_logic) owns the
// item lists and the inventory's list order; this class only holds what the requirement checks read:
// the owned count of every name, the macros, the locations' Need expressions and the options.
//
// Exactness (decision D1). Seed parity depends on these results, including their order and their
// caches, so the port keeps wwrando's behaviour, not an idealised one:
//   - Names are the exact Python strings. A requirement token is resolved in check_requirement_met's
//     order: "Progressive X xN", "<K> Small Key xN", Can Access Item Location "L", Option "..." ...,
//     a name of the logic's item list (Logic.all_cleaned_item_names, given to init), a macro,
//     Nothing, Impossible; anything else is an error.
//   - Every child of an & or | expression is evaluated (Python builds the whole result list before
//     any()/all()), so the caches fill exactly as wwrando's do.
//   - Results are cached per requirement name (requirement_met_cache / items_needed_cache) and every
//     cache is dropped on an inventory change, a set_macro or clearCaches(). The drop is a generation
//     bump, not a clear.
//   - Recursion guard: a requirement in the nested-entrance map whose outer zone macro is on the
//     current check stack returns true (met) or {} (items needed) without being cached; any other
//     recursion is an error. The items-needed walk checks "already satisfied" with a fresh met stack,
//     as Python passes reqs_being_checked=None there.
//   - items_needed is the union, keeping the larger count, over all children of AND and OR alike, in
//     Python dict insertion order, and {} for an expression that is already met.
//
// Errors. The build has no exceptions: where wwrando raises (unknown name, & and | at one level,
// non-whitelisted recursion, a malformed atom) the first error is recorded (failed()/error()) and the
// check returns false / {}. Bad tokens and mixed operators are reported when evaluated, as in Python;
// unbalanced parentheses are reported by compile (Python would build a malformed token list).
#pragma once

#include <stdint.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "pc_rando_options.h"

namespace pcr {

// One entry of an items-needed dict: a name id (ReqLogic::name) and the count required.
struct ItemCount {
    int name;
    int count;
    bool operator==(const ItemCount& o) const { return name == o.name && count == o.count; }
};
// A Python items_needed dict, in insertion order.
using ItemsNeeded = std::vector<ItemCount>;

class ReqLogic {
public:
    // The caches and inventory as save_simulated_playthrough_state copies them (plus the macros'
    // compiled bodies are not part of it: wwrando does not restore macros either).
    struct Snapshot {
        uint32_t generation = 0;
        std::vector<uint16_t> owned;
        std::vector<uint32_t> metGen;
        std::vector<uint8_t> metVal;
        std::vector<uint32_t> needGen;
        std::vector<ItemsNeeded> need;
    };

    ReqLogic();

    // Sets up a Logic: the options (Option tokens are folded against them once, at compile time),
    // the logic's item names (Logic.all_cleaned_item_names, in its order), then the 307 macros of
    // macros.txt and the Need of the 320 locations of item_locations.txt (pc_rando_data). The
    // inventory starts empty. False (and failed()) when the data does not compile.
    bool init(const Options& opts, const std::vector<std::string>& cleanedItemNames);

    // --- Names: every requirement name, item, macro and location is interned once. ---
    int intern(std::string_view s);
    int find(std::string_view s) const; // -1 when never interned
    const std::string& name(int id) const { return mNames[id].text; }
    int nameCount() const { return (int)mNames.size(); }
    bool isItem(int id) const { return id >= 0 && id < (int)mNames.size() && mNames[id].isItem; }
    // The item names given to init, in order.
    const std::vector<int>& itemNames() const { return mItemNames; }

    // --- Inventory (Logic.currently_owned_items as counts). Every change drops the caches. ---
    int owned(int id) const { return id >= 0 && id < (int)mOwned.size() ? mOwned[id] : 0; }
    // add_owned_item / remove_owned_item's effect on the counts. An unknown item (not in the item
    // names) is an error, as in Python; removing an item not owned is an error (list.remove).
    void addOwned(int id);
    void removeOwned(int id);
    void setOwned(int id, int count);
    void clearOwned();

    // --- Macros ---
    // Logic.set_macro: (re)compiles the body and drops the caches. A new name is appended to the
    // macro order (Python dict order). False on a compile error.
    bool setMacro(std::string_view macroName, std::string_view body);
    bool hasMacro(int id) const { return id >= 0 && id < (int)mNames.size() && mNames[id].macroExpr >= 0; }
    // The body string last given to setMacro, or nullptr.
    const std::string* macroBody(int id) const;
    // The compiled expression of a macro, or -1.
    int macroExpr(int id) const { return hasMacro(id) ? mNames[id].macroExpr : -1; }
    // Macro names in Python dict order.
    const std::vector<int>& macroOrder() const { return mMacroOrder; }

    // nested_entrance_macros: "Can Access <nested entrance>" -> "Can Access <outer zone exit>".
    void setNestedEntranceMacro(std::string_view entranceMacro, std::string_view zoneMacro);
    void clearNestedEntranceMacros();

    // --- Expressions ---
    // parse_logic_expression: compiles a requirement string and returns its expression id, or -1
    // (and failed()) when the parentheses do not balance. Identical strings share one expression.
    int compile(std::string_view text);
    // The location's Need expression (index into pcr::locations()).
    int locationExpr(int loc) const { return loc >= 0 && loc < (int)mLocExpr.size() ? mLocExpr[loc] : -1; }
    int locationCount() const { return (int)mLocExpr.size(); }
    int locationName(int loc) const { return mLocName[loc]; }
    // The name ids of the expression's top-level tokens that are not groups, in order (what
    // chart_name_for_location scans, and reqs[0] of a macro).
    void topLevelTokens(int expr, std::vector<int>& out) const;

    // --- Checks ---
    // check_requirement_met(req) with a fresh stack (the name is interned and parsed if new).
    bool checkRequirementMet(std::string_view req);
    bool checkRequirementMet(int id);
    // check_logical_expression_req with a fresh stack.
    bool checkExpr(int expr);
    // check_location_accessible.
    bool checkLocationAccessible(int loc);

    // get_items_needed_by_req_name / get_items_needed_from_logical_expression_req (fresh stacks).
    void itemsNeededByReq(std::string_view req, ItemsNeeded& out);
    void itemsNeededByReq(int id, ItemsNeeded& out);
    void itemsNeededForExpr(int expr, ItemsNeeded& out);
    void itemsNeededForLocation(int loc, ItemsNeeded& out) { itemsNeededForExpr(locationExpr(loc), out); }
    // flatten_items_needed_to_item_names: each name repeated count times, in order.
    static void flatten(const ItemsNeeded& needed, std::vector<int>& names);

    // clear_req_caches.
    void clearCaches() { mGeneration++; }

    Snapshot snapshot() const;
    void restore(const Snapshot& s);

    // --- Errors (where wwrando raises) ---
    bool failed() const { return !mError.empty(); }
    const std::string& error() const { return mError; }
    void clearError() { mError.clear(); }

private:
    enum class Kind : uint8_t {
        Unparsed, // never used as a requirement token
        Count,    // "Progressive X xN" / "K Small Key xN": owned(target) >= n
        Loc,      // Can Access Item Location "L": target = location index
        Const,    // an Option token, folded
        Name,     // an item, a macro, Nothing or Impossible (resolved when checked, as in Python)
        Bad,      // a token Python raises on when it is checked; badText holds the message
    };
    struct Name {
        std::string text;
        Kind kind = Kind::Unparsed;
        bool isItem = false;
        bool constVal = false;
        bool onMetStack = false;
        bool onNeedStack = false;
        int target = -1;    // Count: counted name; Loc: location index
        int count = 0;      // Count: n
        int macroExpr = -1; // compiled macro body
        int macroBody = -1; // index into mMacroBodies
        int nestedOuter = -1; // nested_entrance_macros[this], or -1
        int badText = -1;     // Bad: index into mBadTexts
    };
    // An expression: its operator and its tokens. A token is a name id (>= 0) or a nested group,
    // encoded as ~exprId (< 0).
    enum class Op : uint8_t { And, Or, Mixed, Adjacent };
    struct Expr {
        Op op;
        uint32_t first; // into mTokens
        uint32_t count;
    };

    void fail(const std::string& msg);
    void parseToken(int id);
    int compileTokens(const std::vector<std::string_view>& toks, size_t& pos, int depth, bool& ok);
    bool metExpr(int expr);
    bool metReq(int id);
    void needExpr(int expr, ItemsNeeded& out);
    const ItemsNeeded* needReq(int id);
    static void merge(ItemsNeeded& into, const ItemsNeeded& from);
    static void mergeOne(ItemsNeeded& into, int name, int count);
    bool optionToken(const std::string& tok, bool& value, std::string& err) const;
    void growCaches();

    Options mOpts;
    std::vector<Name> mNames;
    std::unordered_map<std::string, int> mIndex;
    std::vector<int> mItemNames;
    std::vector<uint16_t> mOwned;
    std::vector<int> mMacroOrder;
    std::vector<std::string> mMacroBodies;
    std::vector<int> mNestedKeys;
    std::vector<int> mLocExpr;
    std::vector<int> mLocName;
    std::unordered_map<std::string, int> mLocIndex;
    std::vector<Expr> mExprs;
    std::vector<int> mTokens;
    std::unordered_map<std::string, int> mCompiled;
    std::vector<std::string> mBadTexts;
    // Caches, by name id.
    uint32_t mGeneration = 1;
    std::vector<uint32_t> mMetGen;
    std::vector<uint8_t> mMetVal;
    std::vector<uint32_t> mNeedGen;
    std::vector<ItemsNeeded> mNeed;
    int mMetDepth = 0;
    std::string mError;
};

} // namespace pcr
