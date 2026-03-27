#include "io/casa.hpp"
#include "io/cnf.hpp"
#include "io/ctw.hpp"
using namespace std;

template <>
unique_ptr<CNF> CTWModel::convert_to<CNF>(vector<string> &var_mapping) const {
    unique_ptr<CNF> cnf{new CNF()};

    const unordered_map<string, const Param *> pmp = dump_param_info();

    unordered_map<string, int> var2int;
    int varcount = 0;
    for (const Param *p : params) {
        switch (p->pt) {
        case PT_Bool: {
            var2int[p->name] = ++varcount;
            var_mapping.push_back("true");
        } break;

        case PT_Enum: {
            const EnumParam *ep = dynamic_cast<const EnumParam *>(p);
            int oldvarcnt = varcount;
            auto &group = cnf->group_info_.emplace_back();
            for (const string &v : ep->values) {
                var2int[p->name + "@" + v] = ++varcount;
                group.push_back(varcount);
                var_mapping.push_back(v);
            }

            vector<int> clause;
            for (int i = oldvarcnt + 1; i <= varcount; ++i) {
                clause.push_back(i);
            }
            cnf->clauses_.push_back(std::move(clause));
            vector<int>().swap(clause);

            for (int i = oldvarcnt + 1; i <= varcount; ++i) {
                for (int j = i + 1; j <= varcount; ++j) {
                    clause.push_back(-i);
                    clause.push_back(-j);
                    cnf->clauses_.push_back(std::move(clause));
                    vector<int>().swap(clause);
                }
            }
        } break;

        case PT_Range: {
            const RangeParam *rp = dynamic_cast<const RangeParam *>(p);
            int oldvarcnt = varcount;
            auto &group = cnf->group_info_.emplace_back();
            for (int i = rp->l; i <= rp->r; ++i) {
                var2int[p->name + "@" + to_string(i)] = ++varcount;
                group.push_back(varcount);
                var_mapping.push_back(to_string(i));
            }

            vector<int> clause;
            for (int i = oldvarcnt + 1; i <= varcount; ++i) {
                clause.push_back(i);
            }
            cnf->clauses_.push_back(std::move(clause));
            vector<int>().swap(clause);

            for (int i = oldvarcnt + 1; i <= varcount; ++i) {
                for (int j = i + 1; j <= varcount; ++j) {
                    clause.push_back(-i);
                    clause.push_back(-j);
                    cnf->clauses_.push_back(std::move(clause));
                    vector<int>().swap(clause);
                }
            }
        } break;
        }
    }
    var2int["@top"] = 1;

    int old_varcount = varcount;
    cnf->num_cared_ = varcount;

    vector<SimpleNode *> encoded;
    for (const ImplExpr *ie : constraints) {
        SimpleNode *sn = compile_expr(ie);

        sn = normalize(sn, false, pmp);
        sn = compile_iff_impl(sn);
        sn = atomicize(sn, pmp);
        sn = flatten(sn);

        string o = get_opt(sn);

        if (o == "or") {
            process_or_subtree(encoded, sn, varcount);
        } else if (o == "and") {
            SimpleBranchNode *sbn = dynamic_cast<SimpleBranchNode *>(sn);
            int sz = (int)sbn->opr.size();
            for (int i = 0; i < sz; ++i) {
                SimpleNode *tmp = sbn->opr[i];
                if (tmp->isleaf) {
                    encoded.push_back(tmp);
                } else {
                    process_or_subtree(encoded, tmp, varcount);
                }
            }

            sbn->opr.clear();
            sbn->opt.clear();
            delete sbn;
        } else {
            encoded.push_back(sn);
        }
    }

    for (int vv = old_varcount + 1; vv <= varcount; ++vv) {
        string name = "@tmp" + to_string(vv);
        var2int[name] = vv;
        var_mapping.push_back("true");
    }

    cnf->clauses_.push_back({varcount, -varcount});
    cnf->num_variables_ = varcount;

    int numcl_original = (int)encoded.size();
    for (int cid = 0; cid < numcl_original; ++cid) {
        SimpleNode *sn = encoded[cid];
        auto &cl = cnf->clauses_.emplace_back();

        if (sn->isleaf) {
            SimpleLeafNode *sln = dynamic_cast<SimpleLeafNode *>(sn);
            const ValueAtom *va = dynamic_cast<const ValueAtom *>(sln->ae);
            int l = (sln->notmark ? (-1) : 1) * var2int[va->val];
            cl.push_back(l);
        } else {
            SimpleBranchNode *sbn = dynamic_cast<SimpleBranchNode *>(sn);
            for (SimpleNode *sn2 : sbn->opr) {
                SimpleLeafNode *sln = dynamic_cast<SimpleLeafNode *>(sn2);
                const ValueAtom *va = dynamic_cast<const ValueAtom *>(sln->ae);
                int l = (sln->notmark ? (-1) : 1) * var2int[va->val];
                cl.push_back(l);
            }
        }

        delete sn;
    }

    cnf->calc_cnf_info();
    return cnf;
}

template <>
unique_ptr<CASA> CTWModel::convert_to<CASA>(vector<string> &var_mapping) const {
    unique_ptr<CASA> casa{new CASA()};

    const unordered_map<string, const Param *> pmp = dump_param_info();

    unordered_map<string, int> var2int;
    int varcount = 0;
    for (auto p : params) {
        switch (p->pt) {
        case PT_Bool: {
            var2int[p->name] = ++varcount;
            var2int[p->name + "@false"] = ++varcount;
            var_mapping.push_back("true");
            var_mapping.push_back("false");
            casa->options_.push_back(2);
        } break;

        case PT_Enum: {
            const EnumParam *ep = dynamic_cast<const EnumParam *>(p);
            casa->options_.push_back(ep->size());
            for (const string &v : ep->values) {
                var2int[p->name + "@" + v] = ++varcount;
                var_mapping.push_back(v);
            }
        } break;

        case PT_Range: {
            const RangeParam *rp = dynamic_cast<const RangeParam *>(p);
            casa->options_.push_back(rp->size());
            for (int i = rp->l; i <= rp->r; ++i) {
                var2int[p->name + "@" + to_string(i)] = ++varcount;
                var_mapping.push_back(to_string(i));
            }
        } break;
        }
    }

    var2int["@top"] = 1;
    int old_varcount = varcount;
    casa->num_cared_ = params.size();

    vector<SimpleNode *> encoded;
    for (const ImplExpr *ie : constraints) {
        SimpleNode *sn = compile_expr(ie);

        sn = normalize(sn, false, pmp);
        sn = compile_iff_impl(sn);
        sn = atomicize(sn, pmp);
        sn = flatten(sn);

        string o = get_opt(sn);

        if (o == "or") {
            process_or_subtree(encoded, sn, varcount);
        } else if (o == "and") {
            SimpleBranchNode *sbn = dynamic_cast<SimpleBranchNode *>(sn);
            int sz = (int)sbn->opr.size();
            for (int i = 0; i < sz; ++i) {
                SimpleNode *tmp = sbn->opr[i];
                if (tmp->isleaf) {
                    encoded.push_back(tmp);
                } else {
                    process_or_subtree(encoded, tmp, varcount);
                }
            }

            sbn->opr.clear();
            sbn->opt.clear();
            delete sbn;
        } else {
            encoded.push_back(sn);
        }
    }

    for (int vv = old_varcount + 1, ex = 0; vv <= varcount; ++vv, ++ex) {
        var2int["@tmp" + to_string(vv)] = vv + ex;
        var_mapping.push_back("true");
        var_mapping.push_back("false");
    }

    int numcl_original = (int)encoded.size();
    for (int cid = 0; cid < numcl_original; ++cid) {
        SimpleNode *sn = encoded[cid];
        auto &cl = casa->clauses_.emplace_back();

        if (sn->isleaf) {
            SimpleLeafNode *sln = dynamic_cast<SimpleLeafNode *>(sn);
            const ValueAtom *va = dynamic_cast<const ValueAtom *>(sln->ae);
            cl.emplace_back(var2int[va->val] - 1, sln->notmark);
        } else {
            SimpleBranchNode *sbn = dynamic_cast<SimpleBranchNode *>(sn);
            for (SimpleNode *sn2 : sbn->opr) {
                SimpleLeafNode *sln = dynamic_cast<SimpleLeafNode *>(sn2);
                const ValueAtom *va = dynamic_cast<const ValueAtom *>(sln->ae);
                cl.emplace_back(var2int[va->val] - 1, sln->notmark);
            }
        }

        delete sn;
    }

    for (int i = 0; i < varcount - old_varcount; i++) {
        casa->options_.push_back(2);
    }

    casa->calc_opt_infos();
    return casa;
}
