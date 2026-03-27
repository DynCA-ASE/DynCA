#include "io/ctw.hpp"
#include "util/logger.hpp"
using namespace std;

void print_param(const Param *p) {
    cout << "param name: " << p->name << endl;
    switch (p->pt) {
    case PT_Bool: {
        cout << "param type: bool" << endl;
    } break;

    case PT_Enum: {
        cout << "param type: enum (values: ";
        const EnumParam *ep = dynamic_cast<const EnumParam *>(p);
        for (const string &v : ep->values) {
            cout << v << ";";
        }
        cout << ")" << endl;
    } break;

    case PT_Range: {
        const RangeParam *rp = dynamic_cast<const RangeParam *>(p);
        cout << "param type: range (from " << rp->l << " to " << rp->r << " with step=" << rp->step
             << ")" << endl;
    } break;
    }
}

const vector<pair<string, ImplOperator>> dict_implopr{
    {"<=>", IO_Iff},
    {"<->", IO_Iff},
    {"=>", IO_Impl},
    {"->", IO_Impl},
};
const vector<pair<string, EqOperator>> dict_eqopr{
    {"==", EO_Eq},
    {"=", EO_Eq},
    {"!=", EO_Ne},
};
const vector<pair<string, RelOperator>> dict_relopr{
    {"<=", RO_Le},
    {">=", RO_Ge},
    {"<", RO_Lt},
    {">", RO_Gt},
};
const vector<pair<string, PMOperator>> dict_pmopr{
    {"+", PMO_Plus},
    {"-", PMO_Minus},
};
const vector<pair<string, MMDOperator>> dict_mmdopr{
    {"%", MMDO_Mod},
    {"/", MMDO_Div},
    {"*", MMDO_Mult},
};
const vector<pair<string, AndOperator>> dict_andopr{
    {"&&", AO_Unit},
    {"&", AO_Unit},
    {"and", AO_Unit},
    {"AND", AO_Unit},
};
const vector<pair<string, OrOperator>> dict_oropr{
    {"||", OO_Unit},
    {"|", OO_Unit},
    {"or", OO_Unit},
    {"OR", OO_Unit},
};

const vector<string> all_opr_string{
    "<=>", "<->", "=>", "->", "==", "=", "!=", "<=", ">=",  "<",   ">",  "+",
    "-",   "%",   "/",  "*",  "&&", "&", "||", "|",  "and", "AND", "or", "OR"};

void print_expr(const AtomExpr *p, int sw, bool print_space) {
    if (print_space) {
        for (int j = 0; j < sw; ++j) cout << ' ';
    }

    switch (p->at) {
    case AT_Bool: {
        cout << "Boolean: ";
        const BoolAtom *ba = dynamic_cast<const BoolAtom *>(p);
        if (ba->val)
            cout << "TRUE";
        else
            cout << "FALSE";
    } break;

    case AT_Value: {
        cout << "Value: ";
        const ValueAtom *va = dynamic_cast<const ValueAtom *>(p);
        cout << va->val;
    } break;

    case AT_Int: {
        cout << "Int: ";
        const IntAtom *ia = dynamic_cast<const IntAtom *>(p);
        cout << ia->val;
    } break;
    }
    cout << endl;
}

void print_expr(const Primary *p, int sw) {
    switch (p->pt) {
    case ST_Not: {
        for (int j = 0; j < sw; ++j) cout << ' ';
        cout << "NotExpression:" << endl;
        const NotExpr *ne = dynamic_cast<const NotExpr *>(p);
        print_expr(ne->inner, sw + 2);
    } break;

    case ST_Impl: {
        const PrimaryImplExpr *pie = dynamic_cast<const PrimaryImplExpr *>(p);
        print_expr(pie->inner, sw);
    } break;

    case ST_Atom: {
        const AtomExpr *ae = dynamic_cast<const AtomExpr *>(p);
        print_expr(ae, sw, true);
    } break;
    }
}

void CTWModel::read_first(istream &is) {
    std::string line;
    while (std::getline(is, line)) {
        char *tmp = line.data();
        size_t st = 0;
        size_t len = line.size();
        while (st < len && isspace(tmp[st])) ++st;
        if (tmp[st] == '/' && tmp[st + 1] == '/') {
            continue;
        }
        while (st < len) {
            while (st < len && isspace(tmp[st])) ++st;
            size_t st2 = st;
            while (st2 < len && !isspace(tmp[st2])) ++st2;
            if (st2 > st) {
                if (!text.empty()) text.push_back(' ');
                while (st < st2) {
                    text.push_back(tmp[st]);
                    ++st;
                }
            }
        }
    }
}

void CTWModel::read_spaces(int &st) {
    while (st < textlen && isspace(text[st])) ++st;
}

void CTWModel::skip_one_empty(int &st) {
    if (st < textlen) {
        if (text[st] != ' ') {
            ok = false;
            error_msg = "no good empty separator";
            fail_position = st;
            return;
        }
        ++st;
    }
}

bool CTWModel::allowed_id_first_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '$' || c == '_';
}

bool CTWModel::allowed_id_char(char c) {
    return (c >= '0' && c <= '9') || allowed_id_first_char(c);
}

string CTWModel::read_id(int &st) {
    string tmps;
    read_spaces(st);
    if (st >= textlen) {
        ok = false;
        error_msg = "empty id";
        fail_position = st;
        return "";
    } else if (!allowed_id_first_char(text[st])) {
        ok = false;
        error_msg = "not allowed first char";
        fail_position = st;
        return "";
    }

    do {
        tmps.push_back(text[st]);
        ++st;
    } while (st < textlen && allowed_id_char(text[st]));
    return tmps;
}

void CTWModel::find_next_char_must(char c, int &st) {
    read_spaces(st);
    if (st >= textlen || text[st] != c) {
        ok = false;
        error_msg = "failed to get char ";
        error_msg.push_back(c);
        fail_position = st;
        return;
    }
    ++st;
}

bool CTWModel::find_next_char(char c, int &st) {
    read_spaces(st);
    if (st < textlen && text[st] == c) {
        ++st;
        return true;
    }
    return false;
}

char CTWModel::read_next_char(int &st) {
    read_spaces(st);
    if (st >= textlen) {
        ok = false;
        error_msg = "no more char";
        fail_position = st;
        return '!';
    }
    ++st;
    return text[st - 1];
}

int CTWModel::read_int(int &st) {
    int oldst = st;
    int x = 0;
    while (st < textlen && text[st] >= '0' && text[st] <= '9') {
        x = x * 10 + text[st] - '0';
        ++st;
    }
    if (st == oldst) {
        ok = false;
        error_msg = "failed to read int";
        fail_position = st;
        return -1;
    }
    return x;
}

bool CTWModel::check_startwith(const string &s, int &st) {
    int l = s.length();
    if (st + l > textlen) return false;
    for (int i = 0; i < l; ++i)
        if (s[i] != text[st + i]) return false;
    st += l;
    return true;
}

int CTWModel::read_possibly_signed_int(int &st) {
    int f = 1;
    if (find_next_char('-', st)) {
        f = -1;
    }
    f *= read_int(st);
    if (!ok) return -1;
    return f;
}

void CTWModel::find_separator(char c, int &st) {
    int oldst = st;
    find_next_char_must(c, st);
    if (ok) return;

    ok = true;
    error_msg.clear();
    st = oldst;
    skip_one_empty(st);
}

void CTWModel::read_range_param(int &st) {
    int l = read_possibly_signed_int(st);
    if (!ok) return;

    char c = read_next_char(st);
    if (!ok) return;
    if (c != '.') {
        ok = false;
        error_msg = "failed to read the first dot";
        fail_position = st;
        return;
    }

    if (st >= textlen || text[st] != '.') {
        ok = false;
        error_msg = "failed to read the second dot";
        fail_position = st;
        return;
    }
    ++st;

    int r = read_possibly_signed_int(st);
    if (!ok) return;

    find_next_char_must(']', st);
    if (!ok) return;

    int oldst = st;
    int step = 1;
    string tmps = read_id(st);
    if (!ok) {
        st = oldst;
        ok = true;
        error_msg.clear();
    } else {
        if (tmps == "step") {
            step = read_possibly_signed_int(st);
            if (!ok) {
                error_msg = "failed to read step int";
                fail_position = st;
                return;
            }
            if (step < 0) {
                ok = false;
                error_msg = "step int < 0";
                fail_position = st;
                return;
            }
        } else {
            st = oldst;
        }
    }

    Param *rp = new RangeParam(l, r, step);
    params.push_back(rp);
}

void CTWModel::read_boolean_param(char fc, int &st) {
    find_separator(',', st);
    if (!ok) return;

    string next_elem = read_id(st);
    if (!ok) return;
    if ((fc == 'T' && next_elem != "FALSE") || (fc == 'F' && next_elem != "TRUE") ||
        (fc == 't' && next_elem != "false") || (fc == 'f' && next_elem != "true")) {
        ok = false;
        error_msg = "Boolean const not complementary";
        fail_position = st;
        return;
    }

    find_next_char_must('}', st);
    if (!ok) return;

    Param *bp = new BoolParam();
    params.push_back(bp);
}

void CTWModel::read_enum_param(int &st) {
    vector<string> values;
    for (;;) {
        int oldst = st;
        string next_elem = read_id(st);
        if (!ok) {
            ok = true;
            error_msg.clear();
            st = oldst;
            find_next_char_must('}', st);
            if (!ok) {
                error_msg = "enum values ending incorrectly";
                return;
            }
            break;
        }

        values.emplace_back(next_elem);

        oldst = st;
        find_separator(',', st);
        if (!ok) {
            ok = true;
            error_msg.clear();
            st = oldst;
            find_next_char_must('}', st);
            if (!ok) {
                error_msg = "enum values ending incorrectly";
                return;
            }
            break;
        }
    }

    Param *ep = new EnumParam(values);
    params.push_back(ep);
}

void CTWModel::read_model_name(int &st) {
    string model_verbatim = read_id(st);
    if (!ok) return;
    if (model_verbatim != "Model") {
        ok = false;
        error_msg = "Model not found";
        fail_position = st;
        return;
    }

    skip_one_empty(st);
    if (!ok) return;
    model_name = read_id(st);
}

void CTWModel::read_parameters(int &st) {
    string parameters_verbatim = read_id(st);
    if (!ok) return;
    if (parameters_verbatim != "Parameters") {
        ok = false;
        error_msg = "Parameters not found";
        fail_position = st;
        return;
    }

    find_next_char_must(':', st);
    if (!ok) return;

    for (;;) {
        int oldst = st;
        string next_id = read_id(st);
        if (!ok) {
            if (error_msg[0] == 'e') {
                ok = true;
                error_msg.clear();
                st = oldst;
                break;
            } else
                return;
        }
        if (next_id == "Constraints") {
            st = oldst;
            break;
        }

        find_next_char_must(':', st);
        if (!ok) return;

        oldst = st;
        char c = read_next_char(st);
        if (!ok) return;
        if (c == '{') {
            oldst = st;
            string next_elem = read_id(st);
            if (!ok) return;
            if (next_elem == "TRUE" || next_elem == "FALSE" || next_elem == "true" ||
                next_elem == "false") {
                read_boolean_param(next_elem[0], st);
            } else {
                st = oldst;
                read_enum_param(st);
            }
            if (!ok) return;
        } else if (c == '[') {
            read_range_param(st);
            if (!ok) return;
        } else if (c == 'B') {
            st = oldst;
            string boolean_verbatim = read_id(st);
            if (!ok) return;
            if (boolean_verbatim != "Boolean") {
                ok = false;
                error_msg = "read Boolean failed";
                fail_position = st;
                return;
            }

            Param *bp = new BoolParam();
            params.push_back(bp);
        } else {
            ok = false;
            error_msg = "wrong type of param";
            fail_position = st;
            return;
        }

        params.back()->name = next_id;

        find_separator(';', st);
        if (!ok) return;
    }

    if (params.empty()) {
        ok = false;
        error_msg = "no param found";
        fail_position = st;
        return;
    }
}

template <class Operand, class Operator>
auto CTWModel::read_binop(const vector<pair<string, Operator>> &dict,
                          Operand *(CTWModel::*frec)(int &), int &st)
    -> BinOp<Operand, Operator> * {
    BinOp<Operand, Operator> *res = new BinOp<Operand, Operator>();

    for (;;) {
        Operand *o1 = (this->*frec)(st);
        if (!ok) {
            if (o1 != nullptr) delete o1;
            delete res;
            return nullptr;
        }

        res->opr.push_back(o1);

        bool found = false;
        read_spaces(st);
        for (const pair<string, Operator> &pp : dict) {
            int oldst = st;
            if (check_startwith(pp.first, st)) {
                int newst = st;
                bool noother = true;
                for (const string &s : all_opr_string) {
                    st = oldst;
                    if (s.length() > pp.first.length() && check_startwith(s, st)) {
                        noother = false;
                        break;
                    }
                }
                if (!noother) {
                    st = oldst;
                    continue;
                }

                st = newst;
                res->opt.push_back(pp.second);
                found = true;
                break;
            }
        }

        if (!found) break;
    }

    return res;
}

Primary *CTWModel::read_primary(int &st) {
    int oldst = st;
    char c = read_next_char(st);
    if (!ok) return nullptr;

    if (c == '(') {
        ImplExpr *ie = read_implexpr(st);
        if (!ok) return nullptr;

        find_next_char_must(')', st);
        if (!ok) {
            delete ie;
            return nullptr;
        }

        Primary *res = new PrimaryImplExpr(ie);
        return res;
    } else if (c == '!') {
        Primary *p = read_primary(st);
        if (!ok) return nullptr;

        Primary *res = new NotExpr(p);
        return res;
    } else {
        st = oldst;
        string next_elem = read_id(st);
        if (!ok) {
            if (error_msg[0] == 'e') return nullptr;

            ok = true;
            error_msg.clear();
            st = oldst;
            int x = read_possibly_signed_int(st);
            if (!ok) {
                error_msg = "cannot find integer atom";
                return nullptr;
            }

            Primary *res = new IntAtom(x);
            return res;
        }
        if (next_elem == "not") {
            skip_one_empty(st);
            if (!ok) return nullptr;

            Primary *p = read_primary(st);
            if (!ok) return nullptr;

            Primary *res = new NotExpr(p);
            return res;
        }
        if (next_elem == "true" || next_elem == "TRUE") {
            Primary *res = new BoolAtom(true);
            return res;
        }
        if (next_elem == "false" || next_elem == "FALSE") {
            Primary *res = new BoolAtom(false);
            return res;
        }
        Primary *res = new ValueAtom(next_elem);
        return res;
    }

    return nullptr;
}

ModMultDiv *CTWModel::read_modmultdiv(int &st) {
    return read_binop(dict_mmdopr, &CTWModel::read_primary, st);
}

PlusMinus *CTWModel::read_plusminus(int &st) {
    return read_binop(dict_pmopr, &CTWModel::read_modmultdiv, st);
}

RelExpr *CTWModel::read_relexpr(int &st) {
    return read_binop(dict_relopr, &CTWModel::read_plusminus, st);
}

EqExpr *CTWModel::read_eqexpr(int &st) {
    return read_binop(dict_eqopr, &CTWModel::read_relexpr, st);
}

AndExpr *CTWModel::read_andexpr(int &st) {
    return read_binop(dict_andopr, &CTWModel::read_eqexpr, st);
}

OrExpr *CTWModel::read_orexpr(int &st) {
    return read_binop(dict_oropr, &CTWModel::read_andexpr, st);
}

ImplExpr *CTWModel::read_implexpr(int &st) {
    return read_binop(dict_implopr, &CTWModel::read_orexpr, st);
}

void CTWModel::read_constraints(int &st) {
    string constraints_verbatim = read_id(st);
    if (!ok) {
        if (error_msg[0] == 'e') {
            ok = true;
            error_msg.clear();
        }
        return;
    }
    if (constraints_verbatim != "Constraints") {
        ok = false;
        error_msg = "Constraints not found";
        fail_position = st;
        return;
    }

    find_next_char_must(':', st);
    if (!ok) return;

    for (;;) {
        find_next_char_must('#', st);
        if (!ok) {
            if (st >= textlen) {
                ok = true;
                error_msg.clear();
                break;
            }
            return;
        }

        ImplExpr *res = read_implexpr(st);
        if (!ok) return;

        constraints.push_back(res);

        find_next_char_must('#', st);
        if (!ok) return;
    }
}

unique_ptr<CTWModel> CTWModel::parse(istream &is) {
    unique_ptr<CTWModel> ctw{new CTWModel()};
    ctw->read_first(is);
    ctw->textlen = ctw->text.length();

    int st = 0;
    ctw->ok = true;
    ctw->error_msg = "";

    ctw->read_model_name(st);
    if (!ctw->ok) goto error;

    ctw->skip_one_empty(st);
    if (!ctw->ok) goto error;
    ctw->read_parameters(st);
    if (!ctw->ok) goto error;

    ctw->read_constraints(st);
    if (!ctw->ok) goto error;
    return ctw;

error:
    logger::infof(logger::Color::RED, "Error while parsing ctw file: {}", ctw->error_msg);
    for (char &c : ctw->text)
        if (c == ' ') c = '~';
    logger::infof(logger::Color::RED, "Error occur here: {}",
                  string_view(ctw->text).substr(0, ctw->fail_position));
    throw std::runtime_error("Error while parsing ctw file");
}

SimpleNode *compile_expr(const AtomExpr *p) {
    SimpleLeafNode *res = new SimpleLeafNode();

    switch (p->at) {
    case AT_Bool: {
        const BoolAtom *ba = dynamic_cast<const BoolAtom *>(p);
        BoolAtom *ba2 = new BoolAtom(ba->val);
        res->ae = ba2;
    } break;

    case AT_Value: {
        const ValueAtom *va = dynamic_cast<const ValueAtom *>(p);
        ValueAtom *va2 = new ValueAtom(va->val);
        res->ae = va2;
    } break;

    case AT_Int: {
        const IntAtom *ia = dynamic_cast<const IntAtom *>(p);
        IntAtom *ia2 = new IntAtom(ia->val);
        res->ae = ia2;
    } break;
    }

    return res;
}

SimpleNode *compile_expr(const Primary *p) {
    SimpleNode *res = nullptr;

    switch (p->pt) {
    case ST_Not: {
        const NotExpr *ne = dynamic_cast<const NotExpr *>(p);
        res = compile_expr(ne->inner);
        res->notmark = !res->notmark;
    } break;

    case ST_Impl: {
        const PrimaryImplExpr *pie = dynamic_cast<const PrimaryImplExpr *>(p);
        res = compile_expr(pie->inner);
    } break;

    case ST_Atom: {
        const AtomExpr *ae = dynamic_cast<const AtomExpr *>(p);
        res = compile_expr(ae);
    } break;
    }

    return res;
}

void print_simple_node(const SimpleNode *p, int sw) {
    for (int i = 0; i < sw; ++i) cout << ' ';
    if (p->notmark) cout << "not ";

    if (p->isleaf) {
        const SimpleLeafNode *sln = dynamic_cast<const SimpleLeafNode *>(p);
        print_expr(sln->ae, sw, false);
        return;
    }

    const SimpleBranchNode *sbn = dynamic_cast<const SimpleBranchNode *>(p);
    int sz = (int)sbn->opt.size();
    cout << "(" << endl;

    if (sz == 0) {
        cout << "FATAL ERROR! arity wrong!" << endl;
    }

    for (int i = 0; i < sz; ++i) {
        print_simple_node(sbn->opr[i], sw + 2);
        for (int j = 0; j < sw + 2; ++j) cout << ' ';
        cout << sbn->opt[i] << endl;
    }

    print_simple_node(sbn->opr[sz], sw + 2);
    for (int i = 0; i < sw; ++i) cout << ' ';
    cout << ")" << endl;
}

void print_simple_node_simple(const SimpleNode *p, int sw) {
    for (int i = 0; i < sw; ++i) cout << ' ';
    if (p->notmark) cout << "not ";

    if (p->isleaf) {
        const SimpleLeafNode *sln = dynamic_cast<const SimpleLeafNode *>(p);
        print_expr(sln->ae, sw, false);
        return;
    }

    const SimpleBranchNode *sbn = dynamic_cast<const SimpleBranchNode *>(p);
    int sz = (int)sbn->opr.size();
    cout << sbn->opt[0] << " (" << endl;

    if (sz == 1) {
        cout << "FATAL ERROR! arity wrong!" << endl;
    }

    for (int i = 0; i < sz; ++i) {
        print_simple_node_simple(sbn->opr[i], sw + 2);
    }

    for (int i = 0; i < sw; ++i) cout << ' ';
    cout << ")" << endl;
}

SimpleNode *normalize(SimpleNode *p, bool relop_passed,
                      const unordered_map<string, const Param *> &pmp) {
    if (p->isleaf) {
        SimpleLeafNode *sln = dynamic_cast<SimpleLeafNode *>(p);
        if (!relop_passed) {
            if (sln->ae->at != AT_Value) {
                cout << "FATAL ERROR! single atom is not param type!" << endl;
                return p;
            }

            const ValueAtom *va = dynamic_cast<const ValueAtom *>(sln->ae);
            if (!pmp.count(va->val)) {
                cout << "FATAL ERROR! single atom is not a param!" << endl;
                return p;
            }

            const Param *param = pmp.find(va->val)->second;
            if (param->pt != PT_Bool) {
                cout << "FATAL ERROR! single atom is not a bool param!" << endl;
                return p;
            }

            SimpleLeafNode *slnb = new SimpleLeafNode();
            slnb->ae = new BoolAtom(!sln->notmark);
            sln->notmark = false;

            SimpleBranchNode *sbn = new SimpleBranchNode();
            sbn->opt.push_back("==");
            sbn->opr.push_back(sln);
            sbn->opr.push_back(slnb);

            p = sbn;
        }

        return p;
    }

    SimpleBranchNode *sbn = dynamic_cast<SimpleBranchNode *>(p);
    int sz = (int)sbn->opt.size();

    bool thisisrel =
        (sz == 1 && (sbn->opt[0] == "==" || sbn->opt[0] == "!=" || sbn->opt[0] == "<=" ||
                     sbn->opt[0] == ">=" || sbn->opt[0] == "<" || sbn->opt[0] == ">"));

    for (int i = 0; i <= sz; ++i) {
        sbn->opr[i] = normalize(sbn->opr[i], thisisrel, pmp);
    }

    if (thisisrel) {
        if (!sbn->opr[0]->isleaf || !sbn->opr[1]->isleaf) {
            cout << "FATAL ERROR! one side of rel is not leaf!" << endl;
            return sbn;
        }

        SimpleLeafNode *slnl = dynamic_cast<SimpleLeafNode *>(sbn->opr[0]);
        if (slnl->ae->at != AT_Value) {
            cout << "FATAL ERROR! left side is not param type!" << endl;
            return sbn;
        }

        const ValueAtom *va = dynamic_cast<const ValueAtom *>(slnl->ae);
        if (!pmp.count(va->val)) {
            cout << "FATAL ERROR! left side is not a param!" << endl;
            return sbn;
        }

        const Param *param = pmp.find(va->val)->second;
        SimpleLeafNode *slnr = dynamic_cast<SimpleLeafNode *>(sbn->opr[1]);

        if (sbn->notmark) {
            sbn->opt[0] = opt_negate(sbn->opt[0]);
            sbn->notmark = false;
        }

        switch (param->pt) {
        case PT_Bool: {
            if (slnr->ae->at != AT_Bool) {
                cout << "FATAL ERROR! left side is bool, right side is not" << endl;
                return sbn;
            }

            if (sbn->opt[0] != "==" && sbn->opt[0] != "!=") {
                cout << "FATAL ERROR! operator does not fit bool" << endl;
                return sbn;
            }

            bool change = !((slnl->notmark && slnr->notmark) || (!slnl->notmark && !slnr->notmark));
            if (change) {
                sbn->opt[0][0] = '!' + '=' - sbn->opt[0][0];
            }
            slnl->notmark = slnr->notmark = false;
        } break;

        case PT_Enum: {
            if (slnr->ae->at != AT_Value) {
                cout << "FATAL ERROR! left side is enum, right side is not value" << endl;
                return sbn;
            }
            const EnumParam *ep = dynamic_cast<const EnumParam *>(param);
            const ValueAtom *var = dynamic_cast<const ValueAtom *>(slnr->ae);
            bool ok = false;
            for (const string &s : ep->values) {
                if (s == var->val) {
                    ok = true;
                    break;
                }
            }

            if (!ok) {
                cout << "FATAL ERROR! left side is enum, cannot take right "
                        "side value"
                     << endl;
                return sbn;
            }
        } break;

        case PT_Range: {
            if (slnr->ae->at != AT_Int) {
                cout << "FATAL ERROR! left side is range, right side is not int" << endl;
                return sbn;
            }
        } break;
        }
    }

    return sbn;
}

SimpleNode *compile_iff_impl(SimpleNode *p) {
    if (p->isleaf) return p;

    SimpleBranchNode *sbn = dynamic_cast<SimpleBranchNode *>(p);
    int sz = (int)sbn->opt.size();

    bool thisisrel =
        (sz == 1 && (sbn->opt[0] == "==" || sbn->opt[0] == "!=" || sbn->opt[0] == "<=" ||
                     sbn->opt[0] == ">=" || sbn->opt[0] == "<" || sbn->opt[0] == ">"));

    if (thisisrel) {
        if (sbn->notmark) {
            sbn->opt[0] = opt_negate(sbn->opt[0]);
            sbn->notmark = false;
        }

        return sbn;
    }

    if (sbn->opt[0] == "->") {
        sbn->opt[0] = "or";
        sbn->opr[0]->notmark = !sbn->opr[0]->notmark;
    } else if (sbn->opt[0] == "<->") {
        SimpleNode *slnl2 = sbn->opr[0]->clone();
        SimpleNode *slnr2 = sbn->opr[1]->clone();
        sbn->opr[0]->notmark = !sbn->opr[0]->notmark;
        sbn->opr[1]->notmark = !sbn->opr[1]->notmark;
        SimpleBranchNode *sbnnewl = new SimpleBranchNode();
        SimpleBranchNode *sbnnewr = new SimpleBranchNode();
        sbnnewl->opt.push_back("or");
        sbnnewl->opr.push_back(sbn->opr[0]);
        sbnnewl->opr.push_back(slnr2);
        sbnnewr->opt.push_back("or");
        sbnnewr->opr.push_back(sbn->opr[1]);
        sbnnewr->opr.push_back(slnl2);
        sbn->opt[0] = "and";
        sbn->opr[0] = sbnnewl;
        sbn->opr[1] = sbnnewr;
    }

    bool exists_not_or = false, exists_not_and = false;
    for (const string &s : sbn->opt) {
        if (s != "or") exists_not_or = true;
        if (s != "and") exists_not_and = true;
    }
    if (exists_not_or && exists_not_and) {
        cout << "FATAL ERROR! unexpected connective" << endl;
        return sbn;
    }

    if (sbn->notmark) {
        for (int i = 0; i <= sz; ++i) {
            sbn->opr[i]->notmark = !sbn->opr[i]->notmark;
        }
        if (!exists_not_or) {
            for (int i = 0; i < sz; ++i) {
                sbn->opt[i] = "and";
            }
        } else {
            for (int i = 0; i < sz; ++i) {
                sbn->opt[i] = "or";
            }
        }
        sbn->notmark = false;
    }

    for (int i = 0; i <= sz; ++i) {
        sbn->opr[i] = compile_iff_impl(sbn->opr[i]);
    }

    return sbn;
}

SimpleNode *atomicize(SimpleNode *p, const unordered_map<string, const Param *> &pmp) {
    if (p->isleaf) return p;

    SimpleBranchNode *sbn = dynamic_cast<SimpleBranchNode *>(p);
    int sz = (int)sbn->opt.size();

    bool thisisrel =
        (sz == 1 && (sbn->opt[0] == "==" || sbn->opt[0] == "!=" || sbn->opt[0] == "<=" ||
                     sbn->opt[0] == ">=" || sbn->opt[0] == "<" || sbn->opt[0] == ">"));

    if (thisisrel) {
        if (sbn->notmark) {
            sbn->opt[0] = opt_negate(sbn->opt[0]);
            sbn->notmark = false;
        }

        SimpleLeafNode *slnl = dynamic_cast<SimpleLeafNode *>(sbn->opr[0]);
        SimpleLeafNode *slnr = dynamic_cast<SimpleLeafNode *>(sbn->opr[1]);
        const ValueAtom *va = dynamic_cast<const ValueAtom *>(slnl->ae);
        const Param *param = pmp.find(va->val)->second;

        if (slnl->notmark || slnr->notmark) {
            cout << "FATAL ERROR! should have no not on opr" << endl;
            return sbn;
        }

        SimpleNode *res;

        switch (param->pt) {
        case PT_Bool: {
            const BoolAtom *ba = dynamic_cast<const BoolAtom *>(slnr->ae);
            bool posi = true;
            if (!ba->val) posi = !posi;
            if (sbn->opt[0][0] == '!') posi = !posi;

            SimpleLeafNode *slnnew = new SimpleLeafNode();
            slnnew->ae = new ValueAtom(va->val);
            slnnew->notmark = !posi;
            res = slnnew;
        } break;

        case PT_Enum: {
            const ValueAtom *var = dynamic_cast<const ValueAtom *>(slnr->ae);
            SimpleLeafNode *slnnew = new SimpleLeafNode();
            slnnew->ae = new ValueAtom(va->val + "@" + var->val);
            slnnew->notmark = (sbn->opt[0][0] == '!');
            res = slnnew;
        } break;

        case PT_Range: {
            const IntAtom *ia = dynamic_cast<const IntAtom *>(slnr->ae);
            const RangeParam *rp = dynamic_cast<const RangeParam *>(param);
            int rl = 1, rr = 0;
            bool posi = true;
            if (sbn->opt[0] == "!=") {
                posi = false;
                if (ia->val >= rp->l && ia->val <= rp->r) {
                    rl = rr = ia->val;
                }
            } else if (sbn->opt[0] == "==") {
                if (ia->val >= rp->l && ia->val <= rp->r) {
                    rl = rr = ia->val;
                }
            } else if (sbn->opt[0][0] == '<') {
                rl = rp->l, rr = ia->val - 1;
                if (sbn->opt[0].length() > 1) {
                    ++rr;
                }
                rr = min(rr, rp->r);
            } else if (sbn->opt[0][0] == '>') {
                rl = ia->val + 1, rr = rp->r;
                if (sbn->opt[0].length() > 1) {
                    --rl;
                }
                rl = max(rl, rp->l);
            }

            if (rr < rl) {
                SimpleBranchNode *sbnnew = new SimpleBranchNode();
                if (posi) {
                    sbnnew->opt.push_back("and");
                } else {
                    sbnnew->opt.push_back("or");
                }
                SimpleLeafNode *slnnew = new SimpleLeafNode();
                slnnew->ae = new ValueAtom("@top");
                SimpleLeafNode *slnnew2 = new SimpleLeafNode();
                slnnew2->ae = new ValueAtom("@top");
                slnnew2->notmark = true;

                sbnnew->opr.push_back(slnnew);
                sbnnew->opr.push_back(slnnew2);
                res = sbnnew;
            } else if (rl == rr) {
                SimpleLeafNode *slnnew = new SimpleLeafNode();
                slnnew->ae = new ValueAtom(va->val + "@" + to_string(rl));
                slnnew->notmark = !posi;
                res = slnnew;
            } else {
                SimpleBranchNode *sbnnew = new SimpleBranchNode();
                if (posi) {
                    for (int i = rl; i < rr; ++i) {
                        sbnnew->opt.push_back("or");
                    }
                } else {
                    for (int i = rl; i < rr; ++i) {
                        sbnnew->opt.push_back("and");
                    }
                }
                for (int i = rl; i <= rr; ++i) {
                    SimpleLeafNode *slnnew = new SimpleLeafNode();
                    slnnew->ae = new ValueAtom(va->val + "@" + to_string(i));
                    slnnew->notmark = !posi;
                    sbnnew->opr.push_back(slnnew);
                }
                res = sbnnew;
            }
        } break;

        default: __builtin_unreachable();
        }

        delete p;
        return res;
    }

    for (int i = 0; i <= sz; ++i) {
        sbn->opr[i] = atomicize(sbn->opr[i], pmp);
    }

    return sbn;
}

SimpleNode *flatten(SimpleNode *p) {
    if (p->isleaf) return p;

    SimpleBranchNode *sbn = dynamic_cast<SimpleBranchNode *>(p);
    int sz = (int)sbn->opt.size();
    string thisopt = sbn->opt[0];

    vector<SimpleNode *> newvec;
    for (int i = 0; i <= sz; ++i) {
        SimpleNode *tmp = sbn->opr[i];
        tmp = flatten(tmp);
        if (!tmp->isleaf) {
            SimpleBranchNode *childsbn = dynamic_cast<SimpleBranchNode *>(tmp);
            int childsz = (int)childsbn->opt.size();
            if (childsz == 0) {
                cout << "FATAL ERROR! 0 arity branch node" << endl;
                newvec.push_back(tmp);
                continue;
            }

            if (childsbn->opt[0] != thisopt) {
                newvec.push_back(tmp);
                continue;
            }

            for (int j = 0; j <= childsz; ++j) {
                newvec.push_back(childsbn->opr[j]);
            }
            childsbn->opr.clear();
            delete childsbn;
        } else {
            newvec.push_back(tmp);
        }
    }

    sbn->opr.clear();
    sbn->opr = newvec;
    sz = (int)newvec.size();
    sbn->opt.clear();
    for (int i = 0; i < sz - 1; ++i) {
        sbn->opt.push_back(thisopt);
    }

    return sbn;
}

SimpleLeafNode *tseitin_encode(vector<SimpleNode *> &encoded, SimpleNode *p, int &varcount) {
    if (p->isleaf) {
        SimpleLeafNode *sln = dynamic_cast<SimpleLeafNode *>(p);
        return sln;
    }

    SimpleBranchNode *sbn = dynamic_cast<SimpleBranchNode *>(p);
    int sz = (int)sbn->opr.size();
    string thisopt = sbn->opt[0];

    vector<SimpleLeafNode *> newvec;
    for (int i = 0; i < sz; ++i) {
        newvec.push_back(tseitin_encode(encoded, sbn->opr[i], varcount));
    }

    sbn->opt.clear();
    sbn->opr.clear();
    delete sbn;

    ++varcount;
    int thisvar = varcount;
    SimpleLeafNode *newsln = new SimpleLeafNode();
    newsln->ae = new ValueAtom("@tmp" + to_string(thisvar));

    SimpleLeafNode *newsln_copy = new SimpleLeafNode();
    newsln_copy->ae = new ValueAtom("@tmp" + to_string(thisvar));

    if (thisopt == "and") {
        for (SimpleLeafNode *subsln : newvec) {
            SimpleBranchNode *newsbn = new SimpleBranchNode();
            newsbn->opt.push_back("or");
            SimpleNode *tmpsln1 = newsln->clone();
            SimpleNode *tmpsln2 = subsln->clone();
            tmpsln1->notmark = !tmpsln1->notmark;
            newsbn->opr.push_back(tmpsln1);
            newsbn->opr.push_back(tmpsln2);
            encoded.push_back(newsbn);
        }

        SimpleBranchNode *newsbn = new SimpleBranchNode();
        newsbn->opt.push_back("or");
        for (SimpleLeafNode *subsln : newvec) {
            subsln->notmark = !subsln->notmark;
            newsbn->opr.push_back(subsln);
        }
        newsbn->opr.push_back(newsln);
        encoded.push_back(newsbn);
    } else {
        for (SimpleLeafNode *subsln : newvec) {
            SimpleBranchNode *newsbn = new SimpleBranchNode();
            newsbn->opt.push_back("or");
            SimpleNode *tmpsln1 = newsln->clone();
            SimpleNode *tmpsln2 = subsln->clone();
            tmpsln2->notmark = !tmpsln2->notmark;
            newsbn->opr.push_back(tmpsln1);
            newsbn->opr.push_back(tmpsln2);
            encoded.push_back(newsbn);
        }

        SimpleBranchNode *newsbn = new SimpleBranchNode();
        newsbn->opt.push_back("or");
        for (SimpleLeafNode *subsln : newvec) {
            newsbn->opr.push_back(subsln);
        }
        newsln->notmark = !newsln->notmark;
        newsbn->opr.push_back(newsln);
        encoded.push_back(newsbn);
    }

    return newsln_copy;
}

void process_or_subtree(vector<SimpleNode *> &encoded, SimpleNode *p, int &varcount) {
    SimpleBranchNode *sbn = dynamic_cast<SimpleBranchNode *>(p);
    int sz = (int)sbn->opr.size();

    for (int i = 0; i < sz; ++i) {
        sbn->opr[i] = tseitin_encode(encoded, sbn->opr[i], varcount);
    }

    sbn->opt.clear();
    sbn->opt.push_back("or");
    encoded.push_back(sbn);
}
