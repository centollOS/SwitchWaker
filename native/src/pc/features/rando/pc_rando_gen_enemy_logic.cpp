// pc_rando_gen_enemy_logic.cpp - the enemy randomizer's logic filter (docs/RANDOMIZER_PLAN.md step
// R7.3b). See the header for what it ports and why the probe order matters.
#include "pc_rando_gen_enemy_logic.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>

#include "pc_rando_enemy_data.h"
#include "pc_rando_logic.h"

namespace pcr {

// --- Enemy lists (EnemyRandomizer.__init__) -------------------------------------------------------

std::vector<int> enemyTypesToRandomizeTo() {
    std::vector<int> out;
    Span<EnemyType> types = enemy_types();
    for (size_t i = 0; i < types.size; i++) {
        if (types[i].allow_to) {
            out.push_back((int)i);
        }
    }
    return out;
}

std::vector<int> enemyTypesToRandomizeToWhenAllMustBeKilled() {
    std::vector<int> out;
    Span<EnemyType> types = enemy_types();
    for (size_t i = 0; i < types.size; i++) {
        const EnemyType& t = types[i];
        // Morths do not count as living enemies; Dexivines cannot be killed (enemies.py).
        if (t.allow_to && std::strstr(t.pretty_name, "Morth") == nullptr &&
            std::strcmp(t.pretty_name, "Dexivine") != 0) {
            out.push_back((int)i);
        }
    }
    return out;
}

// --- repr(Logic.parse_logic_expression(s)) ---------------------------------------------------------

namespace {

struct PyNode {
    bool isList = false;
    std::string text;
    std::vector<PyNode> kids;
};

bool pyIsSpace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f' || (c >= 0x1c && c <= 0x1f);
}

std::string pyStrip(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && pyIsSpace((unsigned char)s[b])) {
        b++;
    }
    while (e > b && pyIsSpace((unsigned char)s[e - 1])) {
        e--;
    }
    return s.substr(b, e - b);
}

std::string pyReprStr(const std::string& s) {
    const bool hasSingle = s.find('\'') != std::string::npos;
    const bool hasDouble = s.find('"') != std::string::npos;
    const char q = (hasSingle && !hasDouble) ? '"' : '\'';
    std::string out(1, q);
    for (unsigned char c : s) {
        if (c == (unsigned char)q || c == '\\') {
            out.push_back('\\');
            out.push_back((char)c);
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else if (c == '\t') {
            out += "\\t";
        } else if (c < 0x20 || c == 0x7f) {
            char b[8];
            std::snprintf(b, sizeof b, "\\x%02x", c);
            out += b;
        } else {
            out.push_back((char)c);
        }
    }
    out.push_back(q);
    return out;
}

void pyReprNode(const PyNode& n, std::string& out) {
    if (!n.isList) {
        out += pyReprStr(n.text);
        return;
    }
    out.push_back('[');
    for (size_t i = 0; i < n.kids.size(); i++) {
        if (i) {
            out += ", ";
        }
        pyReprNode(n.kids[i], out);
    }
    out.push_back(']');
}

bool isStr(const PyNode& n, const char* s) { return !n.isList && n.text == s; }

} // namespace

std::string pyReprLogicExpression(const std::string& reqString) {
    // tokens = [substring.strip() for substring in re.split("([&|()])", string)], minus empty ones.
    std::vector<std::string> tokens;
    std::string cur;
    auto flush = [&]() {
        std::string t = pyStrip(cur);
        if (!t.empty()) {
            tokens.push_back(t);
        }
        cur.clear();
    };
    for (char c : reqString) {
        if (c == '&' || c == '|' || c == '(' || c == ')') {
            flush();
            tokens.emplace_back(1, c);
        } else {
            cur.push_back(c);
        }
    }
    flush();
    std::vector<PyNode> stack;
    for (const std::string& tok : tokens) {
        if (tok == ")") {
            PyNode nested;
            nested.isList = true;
            int level = 0;
            while (!stack.empty()) {
                PyNode exp = stack.back();
                stack.pop_back();
                if (isStr(exp, "(")) {
                    if (level == 0) {
                        break;
                    }
                    level--;
                }
                if (isStr(exp, ")")) {
                    level++;
                }
                nested.kids.push_back(exp);
            }
            std::reverse(nested.kids.begin(), nested.kids.end());
            PyNode open, close;
            open.text = "(";
            close.text = ")";
            stack.push_back(open);
            stack.push_back(nested);
            stack.push_back(close);
        } else {
            PyNode n;
            n.text = tok;
            stack.push_back(n);
        }
    }
    PyNode top;
    top.isList = true;
    top.kids = stack;
    std::string out;
    pyReprNode(top, out);
    return out;
}

// --- EnemyLogicFilter ------------------------------------------------------------------------------

EnemyLogicFilter::CacheEntry& EnemyLogicFilter::cacheEntry(const std::string& req, bool throwable,
                                                           bool bombFlowers, bool* existed) {
    for (CacheEntry& e : mCache) {
        if (e.throwable == throwable && e.bombFlowers == bombFlowers && e.req == req) {
            *existed = true;
            return e;
        }
    }
    *existed = false;
    CacheEntry e;
    e.req = req;
    e.throwable = throwable;
    e.bombFlowers = bombFlowers;
    e.result.assign(enemy_types().size, (int8_t)-1);
    mCache.push_back(std::move(e));
    return mCache.back();
}

int EnemyLogicFilter::typeExpr(int type) {
    if (mTypeExpr.empty()) {
        mTypeExpr.assign(enemy_types().size, -2);
    }
    if (mTypeExpr[type] == -2) {
        mTypeExpr[type] = mLogic.req().compile(enemy_types()[type].requirements);
    }
    return mTypeExpr[type];
}

namespace {

// get_all_item_combo_subsets_meeting_req_recursive.
struct ComboWalk {
    Logic& logic;
    int expr;
    const std::string& reqString;
    std::vector<std::vector<int>>& matched;
    std::set<std::vector<int>> checked;
    std::string error;

    bool walk(const std::vector<int>& combo) {
        if (checked.count(combo) != 0) {
            return true;
        }
        logic.addTemporaryItems(combo);
        const bool met = logic.req().checkExpr(expr);
        logic.removeTemporaryItems(combo);
        if (logic.failed()) {
            error = logic.error();
            return false;
        }
        checked.insert(combo);
        if (checked.size() >= 10000) {
            error = "Enemy randomizer got stuck in an exponential loop checking over " +
                    std::to_string(checked.size()) + " possibilities for requirement: " +
                    pyReprLogicExpression(reqString);
            return false;
        }
        if (!met) {
            return true;
        }
        matched.push_back(combo);
        for (size_t i = 0; i < combo.size(); i++) {
            // subset = list(item_combo); subset.remove(item): drops the first occurrence of the item.
            std::vector<int> subset = combo;
            subset.erase(std::find(subset.begin(), subset.end(), combo[i]));
            if (!walk(subset)) {
                return false;
            }
        }
        return true;
    }
};

} // namespace

bool EnemyLogicFilter::combosMeetingReq(const std::vector<int>& combo, int expr, const std::string& reqString,
                                        std::vector<std::vector<int>>& matched, size_t* checked,
                                        std::string* err) {
    matched.clear();
    ComboWalk w{mLogic, expr, reqString, matched, {}, {}};
    const bool ok = w.walk(combo);
    if (checked != nullptr) {
        *checked = w.checked.size();
    }
    if (!ok && err != nullptr) {
        *err = w.error;
    }
    return ok;
}

bool EnemyLogicFilter::filter(const std::string& originalReq, bool hasThrowableObjects, bool hasBombFlowers,
                              const std::vector<int>& possible, std::vector<int>& allowed, std::string* err) {
    mLastMatched = mLastChecked = -1;
    auto fail = [&](const std::string& msg) {
        if (err != nullptr) {
            *err = msg;
        }
        return false;
    };
    allowed = possible;
    std::vector<int> toCheck = possible;
    auto eraseValue = [](std::vector<int>& v, int x) {
        auto it = std::find(v.begin(), v.end(), x);
        if (it != v.end()) {
            v.erase(it);
        }
    };
    bool existed = false;
    const size_t entryIdx = &cacheEntry(originalReq, hasThrowableObjects, hasBombFlowers, &existed) - mCache.data();
    if (existed) {
        for (int p : possible) {
            const int8_t r = mCache[entryIdx].result[p];
            if (r != -1) {
                eraseValue(toCheck, p);
                if (r == 0) {
                    eraseValue(allowed, p);
                }
            }
        }
    }
    if (toCheck.empty()) {
        return true;
    }

    ReqLogic& req = mLogic.req();
    const int expr = req.compile(originalReq);
    if (expr < 0 || mLogic.failed()) {
        return fail(mLogic.error());
    }
    ItemsNeeded need;
    req.itemsNeededForExpr(expr, need);
    if (mLogic.failed()) {
        return fail(mLogic.error());
    }
    // Remove starting items from being checked.
    for (int owned : mLogic.currentlyOwnedItems) {
        for (size_t i = 0; i < need.size(); i++) {
            if (need[i].name == owned) {
                if (--need[i].count == 0) {
                    need.erase(need.begin() + (ptrdiff_t)i);
                }
                break;
            }
        }
    }
    if (mLogic.options().sword_mode == SwordMode::Swordless) {
        for (const char* n : {"Progressive Sword", "Hurricane Spin"}) {
            const int id = mLogic.item(n);
            need.erase(std::remove_if(need.begin(), need.end(), [&](const ItemCount& c) { return c.name == id; }),
                       need.end());
        }
    }
    std::vector<int> biggest;
    ReqLogic::flatten(need, biggest);

    std::vector<std::vector<int>> combos;
    size_t checked = 0;
    std::string walkErr;
    if (!combosMeetingReq(biggest, expr, originalReq, combos, &checked, &walkErr)) {
        return fail(walkErr);
    }
    mLastMatched = (int)combos.size();
    mLastChecked = (int)checked;
    if (combos.empty()) {
        return fail("No item combos satisfied the requirement!");
    }
    // sorted(item_combos_to_check): tuples of str, element by element, a prefix first.
    std::sort(combos.begin(), combos.end(), [&](const std::vector<int>& a, const std::vector<int>& b) {
        return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [&](int x, int y) {
            return mLogic.name(x) < mLogic.name(y);
        });
    });

    Span<EnemyType> types = enemy_types();
    for (const std::vector<int>& combo : combos) {
        mLogic.addTemporaryItems(combo);
        for (int p : toCheck) {
            if (std::find(allowed.begin(), allowed.end(), p) == allowed.end()) {
                continue; // already removed
            }
            const EnemyType& t = types[p];
            if (hasThrowableObjects && t.killable_thrown) {
                continue;
            }
            if (hasBombFlowers && t.killable_bomb_flowers) {
                continue;
            }
            const int e = typeExpr(p);
            if (e < 0) {
                break;
            }
            if (!req.checkExpr(e)) {
                eraseValue(allowed, p);
                mCache[entryIdx].result[p] = 0;
            }
        }
        mLogic.removeTemporaryItems(combo);
        if (mLogic.failed()) {
            return fail(mLogic.error());
        }
    }
    for (int p : allowed) {
        mCache[entryIdx].result[p] = 1;
    }
    return true;
}

} // namespace pcr
