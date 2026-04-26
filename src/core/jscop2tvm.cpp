#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <regex>
#include <stdexcept>
#include <sstream>
#include <algorithm>

// Requires nlohmann/json (e.g., install via package manager or download json.hpp)
#include "json.hpp"

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Helper functions for string manipulation
// ---------------------------------------------------------------------------

std::string trim(const std::string& s) {
    auto start = s.begin();
    while (start != s.end() && std::isspace(*start)) { start++; }
    auto end = s.end();
    do { end--; } while (std::distance(start, end) > 0 && std::isspace(*end));
    return std::string(start, end + 1);
}

std::vector<std::string> split_comma(const std::string& s) {
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        std::string t = trim(item);
        if (!t.empty()) result.push_back(t);
    }
    return result;
}

std::vector<std::string> split_and(const std::string& s) {
    std::regex and_re(R"(\band\b)");
    std::sregex_token_iterator iter(s.begin(), s.end(), and_re, -1);
    std::sregex_token_iterator end;
    std::vector<std::string> parts;
    for (; iter != end; ++iter) {
        std::string p = trim(*iter);
        if (!p.empty()) parts.push_back(p);
    }
    return parts;
}

std::string replace_identifiers(const std::string& input, const std::map<std::string, std::string>& var_map) {
    std::regex id_re(R"([A-Za-z_]\w*)");
    std::sregex_iterator begin(input.begin(), input.end(), id_re);
    std::sregex_iterator end;
    std::string result;
    size_t last_pos = 0;
    for (auto it = begin; it != end; ++it) {
        std::smatch match = *it;
        result += input.substr(last_pos, match.position() - last_pos);
        std::string tok = match.str();
        if (var_map.count(tok)) {
            result += var_map.at(tok);
        } else {
            result += tok;
        }
        last_pos = match.position() + match.length();
    }
    result += input.substr(last_pos);
    return result;
}

// ---------------------------------------------------------------------------
// Data Structures
// ---------------------------------------------------------------------------

struct Bound {
    std::string var;
    std::string lo;
    std::string hi;
};

struct Access {
    std::string kind;
    std::string array;
    std::vector<std::string> indices;
};

struct Statement {
    std::string name;
    std::vector<std::string> ivars;
    std::map<std::string, Bound> bounds;
    std::vector<std::string> schedule_order;
    std::vector<Access> accesses;
};

struct ArrayInfo {
    std::string name;
    std::string type;
    std::vector<std::string> sizes;
};

struct Scop {
    std::string name;
    std::vector<std::string> params;
    std::vector<ArrayInfo> arrays;
    std::vector<Statement> statements;
};

// ---------------------------------------------------------------------------
// Parsers
// ---------------------------------------------------------------------------

std::pair<std::vector<std::string>, std::string> split_params(const std::string& s) {
    // 1. Try matching the standard format with parameters: [params] -> { body }
    std::regex param_re(R"(^\s*\[([^\]]*)\]\s*->\s*\{([\s\S]*)\}\s*$)");
    std::smatch m;
    if (std::regex_match(s, m, param_re)) {
        return {split_comma(m[1].str()), trim(m[2].str())};
    }
    
    // 2. Try matching the parameterless format: { body }
    std::regex no_param_re(R"(^\s*\{([\s\S]*)\}\s*$)");
    if (std::regex_match(s, m, no_param_re)) {
        // Return an empty parameter list
        return {{}, trim(m[1].str())};
    }
    
    throw std::invalid_argument("Not an ISL set string: " + s);
}

std::pair<std::string, std::vector<std::string>> parse_tuple(const std::string& text) {
    std::regex tuple_re(R"(([A-Za-z_]\w*)?\s*\[([^\]]*)\])");
    std::smatch m;
    std::string t = trim(text);
    if (!std::regex_match(t, m, tuple_re)) {
        throw std::invalid_argument("Expected a tuple like Name[..]: " + text);
    }
    std::string name = m[1].str();
    std::vector<std::string> indices = split_comma(m[2].str());
    return {name, indices};
}

std::string to_exclusive_upper(const std::string& expr, const std::string& op) {
    std::string e = trim(expr);
    return (op == "<") ? e : "(" + e + ") + 1";
}

std::string to_inclusive_lower(const std::string& expr, const std::string& op) {
    std::string e = trim(expr);
    return (op == "<=") ? e : "(" + e + ") + 1";
}

std::map<std::string, Bound> parse_domain_constraints(const std::string& constraints, const std::vector<std::string>& ivars) {
    std::map<std::string, Bound> bounds;
    for (const auto& v : ivars) {
        bounds[v] = {v, "", ""};
    }

    std::vector<std::string> parts = split_and(constraints);
    std::regex chained_re(R"(^\s*(.+?)\s*(<=|<)\s*([A-Za-z_]\w*)\s*(<=|<)\s*(.+?)\s*$)");
    std::regex one_sided_re(R"(^\s*(.+?)\s*(<=|<|>=|>)\s*(.+?)\s*$)");

    for (const auto& p : parts) {
        std::smatch m;
        if (std::regex_match(p, m, chained_re)) {
            std::string v = m[3].str();
            if (bounds.count(v)) {
                bounds[v].lo = to_inclusive_lower(m[1].str(), m[2].str());
                bounds[v].hi = to_exclusive_upper(m[5].str(), m[4].str());
                continue;
            }
        }

        if (std::regex_match(p, m, one_sided_re)) {
            std::string lhs = trim(m[1].str());
            std::string op = m[2].str();
            std::string rhs = trim(m[3].str());

            if (bounds.count(rhs) && !bounds.count(lhs)) {
                std::swap(lhs, rhs);
                if (op == "<") op = ">";
                else if (op == "<=") op = ">=";
                else if (op == ">") op = "<";
                else if (op == ">=") op = "<=";
            }

            if (bounds.count(lhs)) {
                std::string v = lhs;
                if (op == "<" || op == "<=") {
                    bounds[v].hi = to_exclusive_upper(rhs, op);
                } else {
                    std::string inv_op = (op == ">=") ? "<=" : "<";
                    bounds[v].lo = to_inclusive_lower(rhs, inv_op);
                }
            }
        }
    }

    for (const auto& kv : bounds) {
        if (kv.second.lo.empty() || kv.second.hi.empty()) {
            throw std::invalid_argument("Could not derive both bounds for " + kv.first + " from: " + constraints);
        }
    }
    return bounds;
}

std::string strip_memref(const std::string& name) {
    if (name.find("MemRef_") == 0) return name.substr(7);
    return name;
}

// ---------------------------------------------------------------------------
// File Parsing Logic
// ---------------------------------------------------------------------------

Scop parse_jscop(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) throw std::runtime_error("Could not open file: " + path);
    json data;
    f >> data;

    Scop scop;
    scop.name = data.value("name", "scop");
    
    auto ctx_split = split_params(data["context"].get<std::string>());
    scop.params = ctx_split.first;

    if (data.contains("arrays")) {
        for (const auto& arr_json : data["arrays"]) {
            ArrayInfo arr;
            arr.name = arr_json["name"].get<std::string>();
            arr.type = arr_json["type"].get<std::string>();
            for (const auto& s : arr_json["sizes"]) {
                if (s.is_string()) arr.sizes.push_back(s.get<std::string>());
                else arr.sizes.push_back(std::to_string(s.get<int>()));
            }
            scop.arrays.push_back(arr);
        }
    }

    for (const auto& st_json : data["statements"]) {
        Statement stmt;
        
        // Parse Domain
        auto dom_split = split_params(st_json["domain"].get<std::string>());
        std::string body = dom_split.second;
        std::string tup, constraints;
        size_t colon_pos = body.find(':');
        if (colon_pos != std::string::npos) {
            tup = body.substr(0, colon_pos);
            constraints = body.substr(colon_pos + 1);
        } else {
            tup = body;
        }
        
        auto tup_parsed = parse_tuple(tup);
        stmt.name = tup_parsed.first;
        stmt.ivars = tup_parsed.second;
        
        if (trim(constraints).empty()) {
            throw std::invalid_argument("Domain has no constraints");
        }
        stmt.bounds = parse_domain_constraints(constraints, stmt.ivars);

        // Parse Schedule
        auto sched_split = split_params(st_json["schedule"].get<std::string>());
        std::string sched_body = sched_split.second;
        size_t arrow_pos = sched_body.find("->");
        if (arrow_pos == std::string::npos) throw std::invalid_argument("Schedule missing '->'");
        
        auto rhs_tup = parse_tuple(sched_body.substr(arrow_pos + 2));
        for (const std::string& d : rhs_tup.second) {
            std::string d_trim = trim(d);
            if (std::find(stmt.ivars.begin(), stmt.ivars.end(), d_trim) != stmt.ivars.end()) {
                stmt.schedule_order.push_back(d_trim);
            } else {
                std::regex const_re(R"(-?\d+)");
                if (!std::regex_match(d_trim, const_re)) {
                    throw std::invalid_argument("Unsupported schedule dim: " + d_trim);
                }
            }
        }
        for (const auto& v : stmt.ivars) {
            if (std::find(stmt.schedule_order.begin(), stmt.schedule_order.end(), v) == stmt.schedule_order.end()) {
                stmt.schedule_order.push_back(v);
            }
        }

        // Parse Accesses
        for (const auto& acc_json : st_json["accesses"]) {
            Access acc;
            acc.kind = acc_json["kind"].get<std::string>();
            
            auto rel_split = split_params(acc_json["relation"].get<std::string>());
            std::string rel_body = rel_split.second;
            size_t rel_arrow_pos = rel_body.find("->");
            if (rel_arrow_pos == std::string::npos) throw std::invalid_argument("Access missing '->'");
            
            auto acc_rhs_tup = parse_tuple(rel_body.substr(rel_arrow_pos + 2));
            if (acc_rhs_tup.first.empty()) throw std::invalid_argument("Access RHS has no array name");
            
            acc.array = strip_memref(acc_rhs_tup.first);
            acc.indices = acc_rhs_tup.second;
            stmt.accesses.push_back(acc);
        }
        scop.statements.push_back(stmt);
    }
    return scop;
}

// ---------------------------------------------------------------------------
// Codegen Utils
// ---------------------------------------------------------------------------

std::map<std::string, std::string> classify_axes(const Statement& stmt) {
    std::set<std::string> write_vars;
    for (const auto& acc : stmt.accesses) {
        if (acc.kind == "write") {
            for (const auto& idx : acc.indices) {
                std::regex word_re(R"([A-Za-z_]\w*)");
                std::sregex_iterator begin(idx.begin(), idx.end(), word_re), end;
                for (auto it = begin; it != end; ++it) {
                    std::string tok = it->str();
                    if (std::find(stmt.ivars.begin(), stmt.ivars.end(), tok) != stmt.ivars.end()) {
                        write_vars.insert(tok);
                    }
                }
            }
        }
    }
    std::map<std::string, std::string> kinds;
    for (const auto& v : stmt.ivars) {
        kinds[v] = write_vars.count(v) ? "S" : "R";
    }
    return kinds;
}

std::string get_dtype(const std::string& type) {
    if (type == "float") return "float32";
    if (type == "double") return "float64";
    if (type == "int") return "int32";
    if (type == "long") return "int64";
    return type;
}

std::string zero_for(const std::string& dtype) {
    if (dtype.find("float") == 0) return "T." + dtype + "(0.0)";
    return "T." + dtype + "(0)";
}

std::string index_str(const std::vector<std::string>& indices, const std::map<std::string, std::string>& var_map) {
    std::string result;
    for (size_t i = 0; i < indices.size(); ++i) {
        result += replace_identifiers(indices[i], var_map);
        if (i < indices.size() - 1) result += ", ";
    }
    return result;
}

std::string join(const std::vector<std::string>& vec, const std::string& delim) {
    std::string res;
    for (size_t i = 0; i < vec.size(); ++i) {
        res += vec[i];
        if (i < vec.size() - 1) res += delim;
    }
    return res;
}

// ---------------------------------------------------------------------------
// TVM TensorIR Emission
// ---------------------------------------------------------------------------

std::string emit_tvm(const Scop& scop) {
    if (scop.statements.size() != 1) {
        throw std::runtime_error("Only single-statement SCoPs are supported.");
    }
    const Statement& stmt = scop.statements[0];
    auto kinds = classify_axes(stmt);

    const auto& loop_vars = stmt.schedule_order;
    std::map<std::string, std::string> var_map;
    for (const auto& v : stmt.ivars) var_map[v] = "v_" + v;

    std::vector<std::string> extents;
    for (const auto& v : loop_vars) {
        const Bound& b = stmt.bounds.at(v);
        if (trim(b.lo) == "0") extents.push_back(b.hi);
        else extents.push_back("(" + b.hi + ") - (" + b.lo + ")");
    }

    std::vector<Access> reads, writes;
    std::set<std::string> seen_r, seen_w;
    for (const auto& acc : stmt.accesses) {
        std::string key = acc.array + "|" + join(acc.indices, ",");
        if (acc.kind == "read" && seen_r.insert(key).second) reads.push_back(acc);
        else if (acc.kind == "write" && seen_w.insert(key).second) writes.push_back(acc);
    }

    if (writes.size() != 1) throw std::runtime_error("Expected exactly one write access.");
    const Access& out = writes[0];

    std::string out_dtype = "float32";
    for (const auto& arr : scop.arrays) {
        if (arr.name == out.array) {
            out_dtype = get_dtype(arr.type);
            break;
        }
    }

    std::vector<ArrayInfo> sig_arrays;
    for (const auto& arr : scop.arrays) {
        bool is_read = false;
        for (const auto& r : reads) if (r.array == arr.name) is_read = true;
        if (is_read && arr.name != out.array) sig_arrays.push_back(arr);
    }
    for (const auto& arr : scop.arrays) {
        if (arr.name == out.array) { sig_arrays.push_back(arr); break; }
    }

    std::vector<std::string> params_sig_vec;
    for (const auto& arr : sig_arrays) {
        std::vector<std::string> dims;
        for (const auto& s : arr.sizes) dims.push_back((s == "*") ? "N" : s);
        std::string dtype = get_dtype(arr.type);
        params_sig_vec.push_back(arr.name + ": T.Buffer((" + join(dims, ", ") + ",), \"" + dtype + "\")");
    }

    const Access* self_load = nullptr;
    std::vector<Access> rhs_factors;
    for (const auto& r : reads) {
        if (r.array == out.array && r.indices == out.indices) self_load = &r;
        else rhs_factors.push_back(r);
    }

    bool has_reduction = false;
    for (const auto& kv : kinds) { if (kv.second == "R") has_reduction = true; }

    std::string out_idx = index_str(out.indices, var_map);
    std::vector<std::string> rhs_terms;
    for (const auto& r : rhs_factors) {
        rhs_terms.push_back(r.array + "[" + index_str(r.indices, var_map) + "]");
    }
    std::string rhs_expr = rhs_terms.empty() ? zero_for(out_dtype) : join(rhs_terms, " * ");

    std::string s_kinds;
    std::vector<std::string> bound_lhs_vec;
    for (const auto& v : loop_vars) {
        s_kinds += kinds.at(v);
        bound_lhs_vec.push_back(var_map.at(v));
    }

    std::vector<std::string> reads_list_vec;
    for (const auto& r : reads) {
        reads_list_vec.push_back(r.array + "[" + index_str(r.indices, var_map) + "]");
    }

    std::stringstream ss;
    ss << "import tvm\n";
    ss << "from tvm.script import tir as T\n\n";
    ss << "@T.prim_func\n";
    ss << "def " << scop.name << "(" << join(params_sig_vec, ", ") << ") -> None:\n";
    ss << "    for " << join(loop_vars, ", ") << " in T.grid(" << join(extents, ", ") << "):\n";
    ss << "        with T.block(\"" << out.array << "\"):\n";
    ss << "            " << join(bound_lhs_vec, ", ") << " = T.axis.remap(\"" << s_kinds << "\", [" << join(loop_vars, ", ") << "])\n";
    ss << "            T.reads(" << join(reads_list_vec, ", ") << ")\n";
    ss << "            T.writes(" << out.array << "[" << out_idx << "])\n";

    if (has_reduction && self_load) {
        ss << "            with T.init():\n";
        ss << "                " << out.array << "[" << out_idx << "] = " << zero_for(out_dtype) << "\n";
        ss << "            " << out.array << "[" << out_idx << "] = " << out.array << "[" << out_idx << "] + " << rhs_expr << "\n";
    } else {
        ss << "            " << out.array << "[" << out_idx << "] = " << rhs_expr << "\n";
    }

    return ss.str();
}

// ---------------------------------------------------------------------------
// CLI Entry Point
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <file.jscop>\n";
        return 1;
    }
    try {
        Scop scop = parse_jscop(argv[1]);
        std::cout << emit_tvm(scop);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}