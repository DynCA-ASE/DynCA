#pragma once

#include "io/base.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

enum ParamType { PT_Bool, PT_Enum, PT_Range };

class Param {
 public:
    ParamType pt;
    std::string name;
    virtual ~Param() = default;
    virtual size_t size() const = 0;
};

class BoolParam : public Param {
 public:
    BoolParam() { pt = PT_Bool; }
    ~BoolParam() = default;
    size_t size() const override { return 2; }
};

class EnumParam : public Param {
 public:
    std::vector<std::string> values;
    explicit EnumParam(const std::vector<std::string> &_v) : values(_v) { pt = PT_Enum; }
    ~EnumParam() = default;
    size_t size() const override { return values.size(); }
};

class RangeParam : public Param {
 public:
    int l, r;
    int step;
    RangeParam(int _l, int _r, int _step) : l(_l), r(_r), step(_step) { pt = PT_Range; }
    ~RangeParam() = default;
    size_t size() const override { return (r - l + 1) / step; }
};

void print_param(const Param *p);

enum ImplOperator { IO_Impl, IO_Iff };
enum EqOperator { EO_Eq, EO_Ne };
enum RelOperator { RO_Ge, RO_Gt, RO_Le, RO_Lt };
enum PMOperator { PMO_Plus, PMO_Minus };
enum MMDOperator { MMDO_Mod, MMDO_Mult, MMDO_Div };
enum OrOperator { OO_Unit };
enum AndOperator { AO_Unit };

inline std::string to_string(ImplOperator o) {
    switch (o) {
    case IO_Impl: return "->";
    case IO_Iff: return "<->";
    }
    return "";
}

inline std::string to_string(EqOperator o) {
    switch (o) {
    case EO_Eq: return "==";
    case EO_Ne: return "!=";
    }
    return "";
}

inline std::string to_string(RelOperator o) {
    switch (o) {
    case RO_Ge: return ">=";
    case RO_Le: return "<=";
    case RO_Gt: return ">";
    case RO_Lt: return "<";
    }
    return "";
}

inline std::string to_string(PMOperator o) {
    switch (o) {
    case PMO_Plus: return "+";
    case PMO_Minus: return "-";
    }
    return "";
}

inline std::string to_string(MMDOperator o) {
    switch (o) {
    case MMDO_Div: return "/";
    case MMDO_Mod: return "mod";
    case MMDO_Mult: return "*";
    }
    return "";
}

inline std::string to_string(AndOperator o) { return "and"; }

inline std::string to_string(OrOperator o) { return "or"; }

template <class Operand, class Operator>
struct BinOp {
    std::vector<Operand *> opr;
    std::vector<Operator> opt;

    ~BinOp() {
        for (Operand *p : opr) {
            delete p;
        }
    }
};

inline bool strict_printing = false;

template <class Operand, class Operator>
void print_binop(const BinOp<Operand, Operator> *p, int spacewidth, std::string binop_name,
                 bool goodcheck) {
    using std::cout, std::endl;
    int sz = p->opt.size();

    if (goodcheck) {
        if (sz > 1) {
            cout << "FATAL ERROR! shape check failing at " << binop_name << endl;
        }
    }

    if (sz > 0 || strict_printing) {
        for (int i = 0; i < spacewidth; ++i) cout << ' ';
        cout << binop_name << ":" << endl;
    }

    for (int i = 0; i < sz; ++i) {
        print_expr(p->opr[i], spacewidth + 2);
        for (int j = 0; j < spacewidth + 2; ++j) cout << ' ';
        cout << to_string(p->opt[i]) << endl;
    }

    print_expr(p->opr[sz], spacewidth + ((sz == 0 && !strict_printing) ? 0 : 2));
}

enum SingleType { ST_Not, ST_Impl, ST_Atom };

class Primary {
 public:
    SingleType pt;
    virtual ~Primary() = default;
};

typedef BinOp<Primary, MMDOperator> ModMultDiv;
typedef BinOp<ModMultDiv, PMOperator> PlusMinus;
typedef BinOp<PlusMinus, RelOperator> RelExpr;
typedef BinOp<RelExpr, EqOperator> EqExpr;
typedef BinOp<EqExpr, AndOperator> AndExpr;
typedef BinOp<AndExpr, OrOperator> OrExpr;
typedef BinOp<OrExpr, ImplOperator> ImplExpr;

class NotExpr : public Primary {
 public:
    Primary *inner;
    explicit NotExpr(Primary *_i) : inner(_i) { pt = ST_Not; }
    ~NotExpr() { delete inner; }
};

class PrimaryImplExpr : public Primary {
 public:
    ImplExpr *inner;
    explicit PrimaryImplExpr(ImplExpr *_i) : inner(_i) { pt = ST_Impl; }
    ~PrimaryImplExpr() { delete inner; }
};

enum AtomType { AT_Bool, AT_Value, AT_Int };

class AtomExpr : public Primary {
 public:
    AtomType at;
    virtual ~AtomExpr() = default;
    virtual AtomExpr *clone() const = 0;
};

class BoolAtom : public AtomExpr {
 public:
    bool val;
    explicit BoolAtom(bool _v) : val(_v) {
        pt = ST_Atom;
        at = AT_Bool;
    }
    ~BoolAtom() = default;
    AtomExpr *clone() const override { return new BoolAtom(val); }
};

class ValueAtom : public AtomExpr {
 public:
    std::string val;
    explicit ValueAtom(std::string _v) : val(std::move(_v)) {
        pt = ST_Atom;
        at = AT_Value;
    }
    ~ValueAtom() = default;
    AtomExpr *clone() const override { return new ValueAtom(val); }
};

class IntAtom : public AtomExpr {
 public:
    int val;
    explicit IntAtom(int _v) : val(_v) {
        pt = ST_Atom;
        at = AT_Int;
    }
    ~IntAtom() = default;
    AtomExpr *clone() const override { return new IntAtom(val); }
};

void print_expr(const AtomExpr *p, int sw, bool print_space);
void print_expr(const Primary *p, int sw);

// clang-format off
inline void print_expr(const ModMultDiv *p, int sw) { print_binop(p, sw, "ModMultDiv", false); }
inline void print_expr(const PlusMinus *p, int sw) { print_binop(p, sw, "PlusMinus", false); }
inline void print_expr(const RelExpr *p, int sw) { print_binop(p, sw, "RelationalExpression", true); }
inline void print_expr(const EqExpr *p, int sw) { print_binop(p, sw, "EquationalExpression", true); }
inline void print_expr(const AndExpr *p, int sw) { print_binop(p, sw, "AndExpression", false); }
inline void print_expr(const OrExpr *p, int sw) { print_binop(p, sw, "OrExpression", false); }
inline void print_expr(const ImplExpr *p, int sw) { print_binop(p, sw, "ImplicationExpression", true); }
// clang-format on

class CTWModel : InputBase {
    std::string model_name;
    std::vector<Param *> params;
    std::vector<ImplExpr *> constraints;

    std::string text;
    int textlen;

    bool ok;
    std::string error_msg;
    int fail_position;

 private:
    CTWModel() = default;
    void read_first(std::istream &is);
    void read_spaces(int &st);
    void skip_one_empty(int &st);
    bool allowed_id_first_char(char c);
    bool allowed_id_char(char c);
    std::string read_id(int &st);
    void find_next_char_must(char c, int &st);
    bool find_next_char(char c, int &st);
    char read_next_char(int &st);
    int read_int(int &st);
    bool check_startwith(const std::string &s, int &st);
    int read_possibly_signed_int(int &st);
    void find_separator(char c, int &st);
    void read_range_param(int &st);
    void read_boolean_param(char fc, int &st);
    void read_enum_param(int &st);
    void read_model_name(int &st);
    void read_parameters(int &st);
    template <class Operand, class Operator>
    auto read_binop(const std::vector<std::pair<std::string, Operator>> &dict,
                    Operand *(CTWModel::*frec)(int &), int &st) -> BinOp<Operand, Operator> *;
    Primary *read_primary(int &st);
    ModMultDiv *read_modmultdiv(int &st);
    PlusMinus *read_plusminus(int &st);
    RelExpr *read_relexpr(int &st);
    EqExpr *read_eqexpr(int &st);
    AndExpr *read_andexpr(int &st);
    OrExpr *read_orexpr(int &st);
    ImplExpr *read_implexpr(int &st);
    void read_constraints(int &st);

 public:
    static auto parse(std::istream &ctw) -> std::unique_ptr<CTWModel>;
    static auto parse(const std::string &ctw_file) -> std::unique_ptr<CTWModel> {
        std::ifstream ctw(ctw_file);
        if (!ctw.is_open()) {
            throw std::runtime_error("Failed to open CTW file.");
        }
        return parse(ctw);
    }

    void print_model() const {
        using std::cout, std::endl;
        cout << "model name: " << model_name << endl;
        cout << endl;

        for (const Param *p : params) {
            print_param(p);
            cout << endl;
        }

        for (const ImplExpr *ie : constraints) {
            print_expr(ie, 0);
            cout << endl;
        }
    }

    std::unordered_map<std::string, const Param *> dump_param_info() const {
        std::unordered_map<std::string, const Param *> res;
        for (const Param *p : params) {
            res[p->name] = p;
        }
        return res;
    }

    std::vector<std::string> dump_param_names() const {
        std::vector<std::string> res;
        for (const Param *p : params) {
            res.push_back(p->name);
        }
        return res;
    }

    /// Convert
    template <std::derived_from<InputBase> T>
    std::unique_ptr<T> convert_to(std::vector<std::string> &var_mapping) const;

    ~CTWModel() {
        for (Param *p : params) delete p;
        for (ImplExpr *p : constraints) delete p;
    }
};

class SimpleNode {
 public:
    bool notmark;
    bool isleaf;
    virtual ~SimpleNode() = default;
    virtual SimpleNode *clone() const = 0;
};

class SimpleBranchNode : public SimpleNode {
 public:
    std::vector<std::string> opt;
    std::vector<SimpleNode *> opr;
    SimpleBranchNode() {
        notmark = false;
        isleaf = false;
    }
    ~SimpleBranchNode() {
        for (SimpleNode *p : opr) {
            delete p;
        }
    }
    SimpleNode *clone() const override {
        SimpleBranchNode *res = new SimpleBranchNode();
        res->notmark = this->notmark;
        res->opt = this->opt;
        for (const SimpleNode *sn : this->opr) {
            res->opr.push_back(sn->clone());
        }
        return res;
    }
};

class SimpleLeafNode : public SimpleNode {
 public:
    AtomExpr *ae;
    SimpleLeafNode() {
        notmark = false;
        isleaf = true;
        ae = nullptr;
    }
    ~SimpleLeafNode() { delete ae; }
    SimpleNode *clone() const override {
        SimpleLeafNode *res = new SimpleLeafNode();
        res->notmark = this->notmark;
        res->ae = this->ae->clone();
        return res;
    }
};

template <class Operand, class Operator>
SimpleNode *compile_expr(const BinOp<Operand, Operator> *p) {
    int sz = p->opt.size();
    if (sz == 0) return compile_expr(p->opr[0]);

    SimpleBranchNode *sbn = new SimpleBranchNode();

    for (int i = 0; i < sz; ++i) {
        sbn->opt.push_back(to_string(p->opt[i]));
        sbn->opr.push_back(compile_expr(p->opr[i]));
    }

    sbn->opr.push_back(compile_expr(p->opr[sz]));
    return sbn;
}

SimpleNode *compile_expr(const AtomExpr *p);
SimpleNode *compile_expr(const Primary *p);

void print_simple_node(const SimpleNode *p, int sw);
void print_simple_node_simple(const SimpleNode *p, int sw);

inline std::string opt_negate(const std::string &s) {
    if (s == ">=") {
        return "<";
    } else if (s == "<=") {
        return ">";
    } else if (s == ">") {
        return "<=";
    } else if (s == "<") {
        return ">=";
    }
    std::string res = s;
    res[0] = '!' + '=' - s[0];
    return res;
}

SimpleNode *normalize(SimpleNode *p, bool relop_passed,
                      const std::unordered_map<std::string, const Param *> &pmp);

SimpleNode *compile_iff_impl(SimpleNode *p);

SimpleNode *atomicize(SimpleNode *p, const std::unordered_map<std::string, const Param *> &pmp);

SimpleNode *flatten(SimpleNode *p);

inline std::string get_opt(const SimpleNode *p) {
    if (p->isleaf) return "";
    const SimpleBranchNode *sbn = dynamic_cast<const SimpleBranchNode *>(p);
    return sbn->opt[0];
}

SimpleLeafNode *tseitin_encode(std::vector<SimpleNode *> &encoded, SimpleNode *p, int &varcount);

void process_or_subtree(std::vector<SimpleNode *> &encoded, SimpleNode *p, int &varcount);
