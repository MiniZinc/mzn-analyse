#include "pass_get_ast.hh"
#include "location_utils.hh"
#include "string_utils.hh"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include <minizinc/astiterator.hh>
#include <minizinc/file_utils.hh>
#include <minizinc/model.hh>
#include <minizinc/prettyprinter.hh>
#include <minizinc/solver.hh>

using namespace MiniZinc;
using std::ostream;
using std::string;
using std::vector;

std::string json_escape(const std::string &orig) {
  std::string repchars = "\\&\"\'<>\n";
  vector<std::string> repstrs = {"\\\\", "&", "\\\"", "'", "<", ">", "\\n"};

  std::stringstream out;
  size_t last = 0;
  size_t found = orig.find_first_of(repchars);
  while (found != std::string::npos) {
    out << orig.substr(last, found - last);
    for (size_t i = 0; i < repchars.size(); i++) {
      if (orig[found] == repchars[i]) {
        out << repstrs[i];
        break;
      }
    }
    last = found + 1;
    found = orig.find_first_of(repchars, found + 1);
  }
  out << orig.substr(last);
  return out.str();
}

void json_floatval(std::ostream& os, const FloatVal& fv) {
  std::ostringstream oss;
  if (fv.isFinite()) {
    oss << std::setprecision(std::numeric_limits<double>::digits10 + 1);
    oss << fv;
    if (oss.str().find('e') == std::string::npos && oss.str().find('.') == std::string::npos) {
      oss << ".0";
    }
    os << oss.str();

  } else {
    if (fv.isPlusInfinity()) {
      os << "\"infinity\"";
    } else {
      os << "\"-infinity\"";
    }
  }
}

void json_intval(std::ostream& os, const IntVal& iv) {
  std::ostringstream oss;
  if (iv.isFinite()) {
    os << iv;
  } else {
    if (iv.isPlusInfinity()) {
      os << "\"infinity\"";
    } else {
      os << "\"-infinity\"";
    }
  }
}

std::string bt_to_name(Type::BaseType bt) {
  switch (bt) {
    case Type::BT_BOOL:
      return "BT_BOOL";
    case Type::BT_INT:
      return "BT_INT";
    case Type::BT_FLOAT:
      return "BT_FLOAT";
    case Type::BT_STRING:
      return "BT_STRING";
    case Type::BT_ANN:
      return "BT_ANN";
    case Type::BT_TOP:
      return "BT_TOP";
    case Type::BT_BOT:
      return "BT_BOT";
    case Type::BT_UNKNOWN:
      return "BT_UNKNOWN";
    default:
      assert(false);
      return "null";
  };
}

std::string uot_to_name(UnOpType bot) {
  switch (bot) {
    case UOT_NOT:
      return "UOT_NOT";
    case UOT_PLUS:
      return "UOT_PLUS";
    case UOT_MINUS:
      return "UOT_MINUS";
    default:
      assert(false);
      return "null";
  }
}

std::string bot_to_name(BinOpType bot) {
  switch (bot) {
    case BOT_PLUS:
      return "BOT_PLUS";
    case BOT_MINUS:
      return "BOT_MINUS";
    case BOT_MULT:
      return "BOT_MULT";
    case BOT_DIV:
      return "BOT_DIV";
    case BOT_IDIV:
      return "BOT_IDIV";
    case BOT_MOD:
      return "BOT_MOD";
    case BOT_LE:
      return "BOT_LE";
    case BOT_LQ:
      return "BOT_LQ";
    case BOT_GR:
      return "BOT_GR";
    case BOT_GQ:
      return "BOT_GQ";
    case BOT_EQ:
      return "BOT_EQ";
    case BOT_NQ:
      return "BOT_NQ";
    case BOT_IN:
      return "BOT_IN";
    case BOT_SUBSET:
      return "BOT_SUBSET";
    case BOT_SUPERSET:
      return "BOT_SUPERSET";
    case BOT_UNION:
      return "BOT_UNION";
    case BOT_DIFF:
      return "BOT_DIFF";
    case BOT_SYMDIFF:
      return "BOT_SYMDIFF";
    case BOT_INTERSECT:
      return "BOT_INTERSECT";
    case BOT_PLUSPLUS:
      return "BOT_PLUSPLUS";
    case BOT_DOTDOT:
      return "BOT_DOTDOT";
    case BOT_EQUIV:
      return "BOT_EQUIV";
    case BOT_IMPL:
      return "BOT_IMPL";
    case BOT_RIMPL:
      return "BOT_RIMPL";
    case BOT_OR:
      return "BOT_OR";
    case BOT_AND:
      return "BOT_AND";
    case BOT_XOR:
      return "BOT_XOR";
    default:
      assert(false);
      return "null";
  }
}

class ExprPrinter {

private:
  bool hide_locations;
  bool hide_annotations;

  public:
  ExprPrinter(bool hide_locs = false, bool hide_anns = false) 
    : hide_locations{hide_locs},
      hide_annotations{hide_anns} {}

  std::string to_string(const Location &loc) {
    std::stringstream ss;

    ss << "{ \"filename\": \"" << to_string(loc.filename()) << "\", "
       << "\"firstLine\": " << loc.firstLine() << ", "
       << "\"lastLine\": " << loc.lastLine() << ", "
       << "\"firstColumn\": " << loc.firstColumn() << ", "
       << "\"lastColumn\": " << loc.lastColumn() << "}";

    return ss.str();
  }

  std::string to_string(const ASTString &as) {
    if (as.empty()) return "";
    return json_escape(std::string(as.c_str()));
  }

  std::string to_string(const Annotation &anns) {
    std::vector<std::string> ann_strs;
    for(auto it = anns.begin(); it != anns.end(); ++it) {
      ann_strs.push_back(to_string(*it));
    }
    std::stringstream ss;
    ss << "[" << utils::join(ann_strs, ", ") << "]";
    return ss.str();
  }

  std::string to_string(const Type &type) {

    std::string ti = type.ti() == Type::TI_VAR ? "TI_VAR" : "TI_PAR";
    std::string st = type.st() == Type::ST_PLAIN ? "ST_PLAIN" : "ST_SET";
    std::string ot = type.ot() == Type::OT_PRESENT ? "OT_PRESENT" : "OT_OPTIONAL";
    std::string cv = type.cv() == Type::CV_NO ? "CV_NO" : "CV_YES";
    std::string bt = bt_to_name(type.bt());

    std::stringstream ss_type;

    ss_type << "{"
            << "\"ti\": \"" << ti << "\", "
            << "\"bt\": \"" << bt << "\", "
            << "\"st\": \"" << st << "\", "
            << "\"ot\": \"" << ot << "\", "
            << "\"cv\": \"" << cv << "\", "
            << "\"enumId\": " << type.enumId() << ", "
            << "\"dim\": " << type.dim()
            << "}";

    return ss_type.str();
  }

  std::string to_string(const Expression *e) {
    if (!e) return "null";
    std::vector<std::string> records;

    std::stringstream r_type;
    r_type << "\"ExprType\": ";

    switch (e->eid()) {
      case Expression::E_INTLIT:
        r_type << "\"IntLit\"";
        records.push_back(r_type.str());
        vIntLit(e->template cast<IntLit>(), records);
        break;
      case Expression::E_FLOATLIT:
        r_type << "\"FloatLit\"";
        records.push_back(r_type.str());
        vFloatLit(e->template cast<FloatLit>(), records);
        break;
      case Expression::E_SETLIT:
        r_type << "\"SetLit\"";
        records.push_back(r_type.str());
        vSetLit(e->template cast<SetLit>(), records);
        break;
      case Expression::E_BOOLLIT:
        r_type << "\"BoolLit\"";
        records.push_back(r_type.str());
        vBoolLit(e->template cast<BoolLit>(), records);
        break;
      case Expression::E_STRINGLIT:
        r_type << "\"StringLit\"";
        records.push_back(r_type.str());
        vStringLit(e->template cast<StringLit>(), records);
        break;
      case Expression::E_ID:
        r_type << "\"Id\"";
        records.push_back(r_type.str());
        vId(e->template cast<Id>(), records);
        break;
      case Expression::E_ANON:
        r_type << "\"AnonVar\"";
        records.push_back(r_type.str());
        vAnonVar(e->template cast<AnonVar>(), records);
        break;
      case Expression::E_ARRAYLIT:
        r_type << "\"ArrayLit\"";
        records.push_back(r_type.str());
        vArrayLit(e->template cast<ArrayLit>(), records);
        break;
      case Expression::E_ARRAYACCESS:
        r_type << "\"ArrayAccess\"";
        records.push_back(r_type.str());
        vArrayAccess(e->template cast<ArrayAccess>(), records);
        break;
      case Expression::E_COMP:
        r_type << "\"Comprehension\"";
        records.push_back(r_type.str());
        vComprehension(e->template cast<Comprehension>(), records);
        break;
      case Expression::E_ITE:
        r_type << "\"ITE\"";
        records.push_back(r_type.str());
        vITE(e->template cast<ITE>(), records);
        break;
      case Expression::E_BINOP:
        r_type << "\"BinOp\"";
        records.push_back(r_type.str());
        vBinOp(e->template cast<BinOp>(), records);
        break;
      case Expression::E_UNOP:
        r_type << "\"UnOp\"";
        records.push_back(r_type.str());
        vUnOp(e->template cast<UnOp>(), records);
        break;
      case Expression::E_CALL:
        r_type << "\"Call\"";
        records.push_back(r_type.str());
        vCall(e->template cast<Call>(), records);
        break;
      case Expression::E_VARDECL:
        r_type << "\"VarDecl\"";
        records.push_back(r_type.str());
        vVarDecl(e->template cast<VarDecl>(), records);
        break;
      case Expression::E_LET:
        r_type << "\"Let\"";
        records.push_back(r_type.str());
        vLet(e->template cast<Let>(), records);
        break;
      case Expression::E_TI:
        r_type << "\"TypeInst\"";
        records.push_back(r_type.str());
        vTypeInst(e->template cast<TypeInst>(), records);
        break;
      case Expression::E_TIID:
        r_type << "\"TypeInstId\"";
        records.push_back(r_type.str());
        vTIId(e->template cast<TIId>(), records);
        break;
    }

    if (!hide_locations) {
      std::stringstream ss_loc;
      ss_loc << "\"location\": " << to_string(e->loc());
      records.push_back(ss_loc.str());
    }

    std::stringstream ss_type;
    ss_type << "\"type\": " << to_string(e->type());
    records.push_back(ss_type.str());

    if (!hide_annotations) {
      std::stringstream ss_anns;
      ss_anns << "\"annotations\": " << to_string(e->ann());
      records.push_back(ss_anns.str());
    }

    std::stringstream ss;
    ss << "{" << utils::join(records, ", ") << "}";
    return ss.str();
  }

  /// Visit integer literal
  void vIntLit(const IntLit* il, std::vector<std::string> &records) {
    std::stringstream ss;

    ss << "\"val\": ";
    json_intval(ss, il->v());

    records.push_back(ss.str());
  }

  /// Visit floating point literal
  void vFloatLit(const FloatLit* fl, std::vector<std::string> &records) {
    std::stringstream ss;

    ss << "\"val\": ";
    json_floatval(ss, fl->v());

    records.push_back(ss.str());
  }

  /// Visit Boolean literal
  void vBoolLit(const BoolLit* bl, std::vector<std::string> &records) {
    std::stringstream ss;
    Printer pp(ss, 0, true);

    ss << "\"val\": ";
    pp.print(bl);

    records.push_back(ss.str());
  }

  /// Visit set literal
  void vSetLit(const SetLit* sl, std::vector<std::string> &records) {

    std::vector<std::string> val_strs;
    auto vals = sl->v();
    for(size_t i=0; i<vals.size(); i++) {
      val_strs.push_back(to_string(vals[i]));
    }

    std::stringstream ss_val;
    ss_val << "\"values\": [" << utils::join(val_strs, ", ") << "]";
    records.push_back(ss_val.str());

    std::vector<std::string> range_strs;
    if (sl->isv() != nullptr) {
      if (sl->type().bt() == Type::BT_BOOL) {
        if (!sl->isv()->empty()) {
          if (sl->isv()->min() == 0) {
            if (sl->isv()->max() == 0) {
              range_strs.push_back("[false, false]");
            } else {
              range_strs.push_back("[false, true]");
            }
          } else {
            range_strs.push_back("[true, true]");
          }
        }
      } else {
        for (IntSetRanges isr(sl->isv()); isr(); ++isr) {
          std::stringstream ss_inner;
          ss_inner << "[";
          json_intval(ss_inner, isr.min());
          ss_inner << ", ";
          json_intval(ss_inner, isr.max());
          ss_inner << "]";
          range_strs.push_back(ss_inner.str());
        }
      }
    } else if (sl->fsv() != nullptr) {
      for (FloatSetRanges isr(sl->fsv()); isr(); ++isr) {
        std::stringstream ss_inner;
        ss_inner << "[";
        json_floatval(ss_inner, isr.min());
        ss_inner << ", ";
        json_floatval(ss_inner, isr.max());
        ss_inner << "]";
        range_strs.push_back(ss_inner.str());
      }
    }

    std::stringstream ss_ranges;
    ss_ranges << "\"ranges\": [" << utils::join(range_strs, ", ") << "]";
    records.push_back(ss_ranges.str());
  }

  /// Visit string literal
  void vStringLit(const StringLit* sl, std::vector<std::string> &records) {
    std::stringstream ss;
    ss << "\"val\": \"" << to_string(sl->v()) << "\"";

    records.push_back(ss.str());
  }

  /// Visit identifier
  void vId(const Id* ident, std::vector<std::string> &records) {
    std::stringstream ss;
    ss << "\"v\": \"" << *ident << "\"";
    records.push_back(ss.str());
  }

  /// Visit anonymous variable
  void vAnonVar(const AnonVar* /*x*/, std::vector<std::string> &records) { }

  /// Visit array literal
  void vArrayLit(const ArrayLit* al, std::vector<std::string> &records) {
    std::vector<std::string> dim_strs;
    for(size_t i=0; i<al->dims(); i++) {
      std::stringstream ss_inner;
      ss_inner << "[" << al->min(i) << ", " << al->max(i) << "]";
      dim_strs.push_back(ss_inner.str());
    }

    std::stringstream ss_dims;
    ss_dims << "\"dims\": [" << utils::join(dim_strs, ", ") << "]";
    records.push_back(ss_dims.str());

    std::vector<std::string> val_strs;
    for(size_t i=0; i<al->size(); i++) {
      std::stringstream ss_inner;
      ss_inner << to_string(al->operator[](i));
      val_strs.push_back(ss_inner.str());
    }

    std::stringstream ss_vals;
    ss_vals << "\"values\": [" << utils::join(val_strs, ", ") << "]";
    records.push_back(ss_vals.str());
  }

  /// Visit array access
  void vArrayAccess(const ArrayAccess* aa, std::vector<std::string> &records) {
    std::stringstream ss_v;
    ss_v << "\"v\": " << to_string(aa->v());
    records.push_back(ss_v.str());

    std::vector<std::string> idx_strs;
    auto idx_vec = aa->idx();
    for (size_t i=0; i<idx_vec.size(); i++) {
      std::stringstream ss_index;
      ss_index << to_string(idx_vec[i]);
      idx_strs.push_back(ss_index.str());
    }

    std::stringstream ss_idx;
    ss_idx << "\"idx\": [" << utils::join(idx_strs, ", ") << "]";
    records.push_back(ss_idx.str());
  }

  /// Visit array comprehension
  void vComprehension(const Comprehension* comp, std::vector<std::string> &records) {

    std::vector<std::string> gens;
    for (size_t i = 0; i<comp->numberOfGenerators(); i++) {
      std::vector<std::string> decls;
      for(size_t j = 0; j<comp->numberOfDecls(i); j++) {
        decls.push_back(to_string(comp->decl(i,j)->id()));
      }

      std::stringstream ss_gen;
      ss_gen << "{"
             << "\"ExprType\": \"Generator\", "
             << "\"decls\": [" << utils::join(decls, ", ") << "], "
             << "\"in\": " << to_string(comp->in(i)) << ", "
             << "\"where\": " << to_string(comp->where(i))
             << "}";

      gens.push_back(ss_gen.str());
    }

    std::stringstream ss_gens;
    ss_gens << "\"generators\": [" << utils::join(gens, ", ") << "]";
    records.push_back(ss_gens.str());

    std::stringstream ss_exp;
    ss_exp << "\"e\": " << to_string(comp->e());
    records.push_back(ss_exp.str());
  }

  /// Visit array comprehension (only generator \a gen_i)
  void vComprehensionGenerator(const Comprehension* /*c*/, int /*gen_i*/, std::vector<std::string> &records) {
    records.push_back("\"ERROR\": \"Not implemented\"");
  }

  /// Visit if-then-else
  void vITE(const ITE* ite, std::vector<std::string> &records) {
    std::vector<std::string> branches_strs;
    for(size_t i=0; i<ite->size(); i++) {
      std::stringstream ss_branch;
      ss_branch << "["
                << to_string(ite->ifExpr(i)) << ", "
                << to_string(ite->thenExpr(i))
                << "]";
      branches_strs.push_back(ss_branch.str());
    }

    std::stringstream ss_branches;
    ss_branches << "\"branches\": [" << utils::join(branches_strs, ", ") << "]";
    records.push_back(ss_branches.str());

    std::stringstream ss_else;
    ss_else << "\"else\": " << to_string(ite->elseExpr());
    records.push_back(ss_else.str());
  }

  /// Visit binary operator
  void vBinOp(const BinOp* bo, std::vector<std::string> &records) {
    std::stringstream ss_op;
    ss_op << "\"op\": \"" << bot_to_name(bo->op()) << "\"";
    records.push_back(ss_op.str());

    std::stringstream ss_lhs;
    ss_lhs << "\"lhs\": " << to_string(bo->lhs());
    records.push_back(ss_lhs.str());

    std::stringstream ss_rhs;
    ss_rhs << "\"rhs\": " << to_string(bo->rhs());
    records.push_back(ss_rhs.str());
  }

  /// Visit unary operator
  void vUnOp(const UnOp* uo, std::vector<std::string> &records) {
    std::stringstream ss_op;
    ss_op << "\"op\": \"" << uot_to_name(uo->op()) << "\"";
    records.push_back(ss_op.str());

    std::stringstream ss_e;
    ss_e << "\"e\": " << to_string(uo->e());
    records.push_back(ss_e.str());
  }

  /// Visit call
  void vCall(const Call* ca, std::vector<std::string> &records) {
    std::stringstream ss_id;
    ss_id << "\"id\": \"" << ca->id() << "\"";
    records.push_back(ss_id.str());

    std::stringstream ss_args;

    vector<std::string> args;
    for (size_t i = 0; i < ca->argCount(); i++) {
      args.push_back(to_string(ca->arg(i)));
    }
    ss_args << "\"args\": [" << utils::join(args, ",") << "]";

    records.push_back(ss_args.str());

  }

  /// Visit let
  void vLet(const Let* let, std::vector<std::string> &records) {
    std::vector<std::string> decls_strs;
    auto decl_vec = let->let();
    for(size_t i=0; i<decl_vec.size(); i++) {
      std::stringstream ss;
      ss << to_string(decl_vec[i]);
      decls_strs.push_back(ss.str());
    }

    std::stringstream ss_decls;
    ss_decls << "\"declarations\": [" << utils::join(decls_strs, ", ") << "]";
    records.push_back(ss_decls.str());

    std::stringstream ss_e;
    ss_e << "\"in\": " << to_string(let->in());
    records.push_back(ss_e.str());
  }

  /// Visit variable declaration
  void vVarDecl(const VarDecl* vd, std::vector<std::string> &records) {
    std::stringstream ss_id;
    ss_id << "\"id\": " << to_string(vd->id());
    records.push_back(ss_id.str());

    std::stringstream ss_ti;
    ss_ti << "\"ti\": " << to_string(vd->ti());
    records.push_back(ss_ti.str());

    std::stringstream ss_expr;
    ss_expr << "\"expression\": " << to_string(vd->e());
    records.push_back(ss_expr.str());
  }

  /// Visit type inst
  void vTypeInst(const TypeInst* ti, std::vector<std::string> &records) {
    auto ti_ranges = ti->ranges();
    std::vector<std::string> ranges_strs;
    for(size_t i=0; i<ti_ranges.size(); i++) {
      ranges_strs.push_back(to_string(ti_ranges[i]));
    }

    std::stringstream ss_ranges;
    ss_ranges << "\"ranges\": [" << utils::join(ranges_strs, ", ") << "]";
    records.push_back(ss_ranges.str());

    std::stringstream ss_domain;
    ss_domain << "\"domain\": " << to_string(ti->domain());
    records.push_back(ss_domain.str());
  }

  /// Visit TIId
  void vTIId(const TIId* /*tiid*/, std::vector<std::string> &records) {
    records.push_back("\"ERROR\": \"Not implemented\"");
  }

};

/// Visitor for model items
ItemCollector::ItemCollector() {}

/// Enter model
bool ItemCollector::enterModel(Model* /*m*/) { return true; }

/// Enter item
bool ItemCollector::enter(Item* /*m*/) { return true; }

/// Visit include item
void ItemCollector::vIncludeI(IncludeI* ii) {
  std::stringstream ss;
  ExprPrinter ep;

  ss << "{"
     << "\"ItemType\": \"IncludeI\", "
     << "\"location\": " << ep.to_string(ii->loc()) << ", "
     << "\"filename\": \"" << ep.to_string(ii->f()) << "\""
     << "}";

  items.push_back(ss.str());
}

/// Visit variable declaration
void ItemCollector::vVarDeclI(VarDeclI* vdi) {
  std::stringstream ss;
  ExprPrinter ep;

  ss << "{"
     << "\"ItemType\": \"VarDeclI\","
     << "\"location\": " << ep.to_string(vdi->loc()) << ", "
     << "\"expression\": " << ep.to_string(vdi->e())
     << "}";

  items.push_back(ss.str());
}

/// Visit assign item
void ItemCollector::vAssignI(AssignI* ai) {
  std::stringstream ss;
  ExprPrinter ep;

  ss << "{"
     << "\"ItemType\": \"AssignI\","
     << "\"location\": " << ep.to_string(ai->loc()) << ", "
     << "\"id\": " << ep.to_string(ai->id()) << ", "
     << "\"expression\": " << ep.to_string(ai->e()) << ", "
     << "}";

  items.push_back(ss.str());
}

/// Visit constraint item
void ItemCollector::vConstraintI(ConstraintI* ci) {
  std::stringstream ss;
  ExprPrinter ep;

  ss << "{"
     << "\"ItemType\": \"ConstraintI\", "
     << "\"location\": " << ep.to_string(ci->loc()) << ", "
     << "\"expression\": " << ep.to_string(ci->e())
     << "}";

  items.push_back(ss.str());
}

/// Visit solve item
void ItemCollector::vSolveI(SolveI* si) {
  std::stringstream ss;
  ExprPrinter ep;

  ss << "{"
     << "\"ItemType\": \"SolveI\","
     << "\"location\": " << ep.to_string(si->loc()) << ", "
     << "\"sense\": ";

  if(si->st() == SolveI::ST_MAX) {
    ss << "\"maximize\"";
  } else if(si->st() == SolveI::ST_MIN) {
    ss << "\"minimize\"";
  } else {
    ss << "\"satisfy\"";
  }

  ss << ", "
     << "\"expression\": " << ep.to_string(si->e()) << ", "
     << "\"annotations\": " << ep.to_string(si->ann())
     << "}";

  items.push_back(ss.str());
}

/// Visit output item
void ItemCollector::vOutputI(OutputI* oi) {
  std::stringstream ss;
  ExprPrinter ep;

  ss << "{"
     << "\"ItemType\": \"OutputI\","
     << "\"location\": " << ep.to_string(oi->loc()) << ", "
     << "\"annotations\": " << ep.to_string(oi->ann()) << ", "
     << "\"expression\": " << ep.to_string(oi->e())
     << "}";

  items.push_back(ss.str());
}

/// Visit function item
void ItemCollector::vFunctionI(FunctionI* fi) {
  std::stringstream ss;
  ExprPrinter ep;

  std::vector<std::string> param_strs;
  for(size_t i =0; i<fi->paramCount(); i++) {
    std::stringstream ss;
    ss << ep.to_string(fi->param(i));
    param_strs.push_back(ss.str());
  }

  ss << "{"
     << "\"ItemType\": \"FunctionI\","
     << "\"location\": " << ep.to_string(fi->loc()) << ", "
     << "\"id\": \"" << ep.to_string(fi->id()) << "\", "
     << "\"ti\": " << ep.to_string(fi->ti()) << ", "
     << "\"fromStdlib\": " << (fi->fromStdLib() ? "true" : "false") << ", "
     << "\"params\": [" << utils::join(param_strs, ", ") << "],"
     << "\"expression\": " << ep.to_string(fi->e()) << ", "
     << "\"annotations\": " << ep.to_string(fi->ann())
     << "}";

  items.push_back(ss.str());
}

void ItemCollector::write_json(ostream &os) {
  os << "{ \"items\": [";
  os << utils::join(items, ",\n", false);
  os << "]}";
}

// GetAST
GetAST::GetAST() {}

std::string GetAST::get_name() { return "get-ast"; }

MiniZinc::Env *GetAST::run(MiniZinc::Env *e, std::ostream &log) {
  Model *m = e->model();
  iter_items(ic, m);

  return e;
}

void GetAST::write_json(ostream &os) { ic.write_json(os); }
