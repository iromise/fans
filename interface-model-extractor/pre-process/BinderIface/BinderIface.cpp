#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Sema/Sema.h"
#include "clang/Tooling/Core/QualTypeNames.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdlib>
#include <iostream>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <sstream>
#include <fstream>
using namespace clang;
using namespace std;
using namespace llvm;
using namespace clang::TypeName;

#define DEBUG 1
// <functionname, function signature>
vector<string> misc_parcel_related_function;
vector<string> special_parcelable_function;
map<string, int32_t> funcNameCount;

typedef enum
{
  STRUCTURE_FUNCTION,
  DEAL_PARCEL_FUNCTION,
  COMMON_FUNCTION,
} BinderFunctionType;
namespace BinderIface
{

  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr root;
  map<string, uint64_t> enumCounter;
  vector<pair<string, string>> typeMap;
  bool inStructure = false;

  void DebugInfo(string s)
  {
    if (DEBUG)
      cout << s << endl;
  }
  string start = "Start Visiting ";
  string end = "End Visiting ";
  bool interfacecast = false;

  template <typename T>
  bool isDataReplyExists(T target)
  {
    string test;
    raw_string_ostream out(test);
    target->dump(out);
    out.str();
    if (
        (test.find("data") != string::npos ||
         test.find("reply") != string::npos || inStructure) &&
            test.find("android::Parcel") != string::npos ||
        // test.find("request") != string::npos ||
        test.find("readFromParcel") != string::npos ||
        test.find("writeToParcel") != string::npos ||
        test.find("interface_cast") != string::npos ||
        test.find("android::IBinder") != string::npos ||
        test.find("ReturnStmt") != string::npos)
    {
      return true;
    }
    else
    {
      return false;
    }
  }
  vector<string> split(const string &str, const string &pattern)
  {
    vector<string> res;
    if (str == "")
      return res;
    string strs = str + pattern;
    size_t pos = strs.find(pattern);

    while (pos != strs.npos)
    {
      string temp = strs.substr(0, pos);
      res.push_back(temp);
      strs = strs.substr(pos + pattern.size(), strs.size());
      pos = strs.find(pattern);
    }
    return res;
  }

  vector<string> split_datareply(const string &str, const string &pattern)
  {
    vector<string> ans;
    ans = split(str, pattern);
    if (ans.size() == 0)
    {
      ans.push_back("");
      ans.push_back("");
    }
    else if (ans.size() == 1)
    {
      if (str.find("data") != string::npos)
      {
        ans.push_back(str);
        ans.push_back("");
      }
      else if (str.find("reply") != string::npos)
      {
        ans.push_back("");
        ans.push_back(str);
      }
      else
      {
        DebugInfo("data reply format is not ok!");
        DebugInfo(str);
        exit(0);
      }
    }
    return ans;
  }
  void FindAndReplaceAll(string &data, string &toSerach, string &replaceStr)
  {
    size_t pos = data.find(toSerach);
    while (pos != string::npos)
    {
      data.replace(pos, toSerach.size(), replaceStr);
      pos = data.find(toSerach, pos + replaceStr.size());
    }
  }
  bool EndsWith(const string &a, const string &b)
  {
    if (b.size() > a.size())
      return false;
    return std::equal(a.begin() + a.size() - b.size(), a.end(), b.begin());
  }
  // string GetChildContent(xmlNodePtr cur, char *name) {
  //   xmlChar *uri;
  //   cur = cur->xmlChildrenNode;
  //   while (cur != NULL) {
  //     if ((!xmlStrcmp(cur->name, (const xmlChar *)name))) {
  //       uri = xmlNodeGetContent(cur, (const xmlChar *)"info");
  //       // printf("Info: %s\n", uri);
  //     }
  //     cur = cur->next;
  //   }
  //   return;
  // }

  class BinderIfaceASTVisitor
      : public RecursiveASTVisitor<BinderIfaceASTVisitor>
  {
  private:
    ASTContext *context;

  public:
    void setContext(ASTContext &context) { this->context = &context; }
    // bool VisitDecl(Decl *decl) {
    //   if (const NamedDecl *ND = dyn_cast<NamedDecl>(decl)) {
    //     cout << this->context->getSourceManager()
    //                 .getFilename((decl->getLocation()))
    //                 .str()
    //          << endl;
    //     llvm::errs() << "top-level-decl: \"" << ND->getNameAsString() <<
    //     "\"\n"; ND->dump();
    //   }
    //   return true;
    // }

    string getType(const QualType &qualType)
    {
      // QualType tmp = clang::TypeName::getFullyQualifiedType(qualType, this->context, true)
      string canonicalType = qualType.getCanonicalType().getAsString();
      string originalType = qualType.getAsString();
      string qualifiedType = clang::TypeName::getFullyQualifiedName(qualType, *this->context);

      cout << "originalType: " << originalType << endl;
      cout << "canonicalType: " << canonicalType << endl;
      cout << "qualifiedType: " << qualifiedType << endl;
      if (originalType.find("(") != string::npos && originalType.find(")") != string::npos && originalType.find("(anonymous") == string::npos)
      {
        // for function signature? maybe this check is not right.
        return originalType;
      }
      else if (canonicalType.find("enum ", 0) != string::npos || canonicalType.rfind("struct ", 0) != string::npos)
      {
        // originalType: const enum android::Interpolator::InterpolatorType
        // canonicalType: const enum android::Interpolator::InterpolatorType
        // qualifiedType: const android::Interpolator<float, float>::InterpolatorType
        string toSearch = "/", toReplace = ".";
        FindAndReplaceAll(canonicalType, toSearch, toReplace);
        return canonicalType;
      }
      //TODO: might affect the typedefine structure type.
      else if (canonicalType.find("struct ") != string::npos)
      {
        // originalType: sp<struct android::AMessage>
        // canonicalType: class android::sp<struct android::AMessage>
        // qualifiedType: android::sp<android::AMessage>
        return canonicalType;
      }
      //TODO: might affect the typedefine class type.
      else if (canonicalType.find("class ") != string::npos)
      {
        // for sp<class android::IBinder>
        return canonicalType;
      }
      else if (canonicalType.find("vector<") != string::npos || canonicalType.find("Vector<") != string::npos)
      {
        return canonicalType;
      }
      else if (qualifiedType.find("enum ") != string::npos)
      {
        return qualifiedType;
      }
      else if (originalType == "int32_t" || originalType == "uint32_t" ||
               originalType == "int64_t" || originalType == "uint64_t" ||
               originalType == "int8_t" || originalType == "uint8_t" ||
               originalType == "int16_t" || originalType == "uint16_t" ||
               originalType.find("int8_t") != string::npos || originalType.find("uint8_t") != string::npos)
      {
        return canonicalType;
      }
      else if (canonicalType == "int" || canonicalType == "unsigned int" ||
               canonicalType == "long long " || canonicalType == "unsigned long long" ||
               canonicalType == "float" || canonicalType == "double" ||
               canonicalType == "_Bool" || canonicalType == "unsigned short" ||
               canonicalType == "short")
      {
        // originalType: class IOMX::buffer_id
        // canonicalType: unsigned int
        // qualifiedType: android::IOMX::buffer_id
        // android::omx_message::(anonymous union)::(anonymous)
        // originalType: class IOMX::buffer_id
        // canonicalType: unsigned int
        // qualifiedType: android::IOMX::buffer_id
        if (originalType.find("class ") != string::npos)
        {
          return qualifiedType;
        }
        else if (qualifiedType.find(originalType) != string::npos)
        {
          // originalType: internal::AtomicWord
          // canonicalType: int
          // qualifiedType: google::protobuf::internal::AtomicWord
          return qualifiedType;
        }
        else
        {
          return originalType;
        }
      }
      else if (canonicalType.find(qualifiedType) != string::npos)
      {
        return canonicalType;
      }
      else if (canonicalType.find("anonymous at") != string::npos)
      {
        string toSearch = "/", toReplace = ".";
        FindAndReplaceAll(canonicalType, toSearch, toReplace);
        return canonicalType;
      }
      else
      {
        return qualifiedType;
      }
    }

    xmlNodePtr WalkIntegerLiteral(IntegerLiteral *integerliteral)
    {
      xmlNodePtr currnode;
      currnode = xmlNewNode(NULL, BAD_CAST "IntegerLiteral");
      uint64_t value = integerliteral->getValue().getLimitedValue();
      xmlNewTextChild(currnode, NULL, BAD_CAST "value",
                      BAD_CAST to_string(value).c_str());
      return currnode;
    }
    xmlNodePtr WalkStringLiteral(clang::StringLiteral *stringliteral)
    {
      xmlNodePtr currnode, strnode;

      currnode = xmlNewNode(NULL, BAD_CAST "String");
      strnode = xmlNewText(BAD_CAST stringliteral->getString().str().c_str());
      xmlAddChild(currnode, strnode);
      return currnode;
    }
    xmlNodePtr getFuncArgv(FunctionDecl *funDecl)
    {
      xmlNodePtr argv;
      argv = xmlNewNode(NULL, BAD_CAST "argv");
      for (int i = 0; i < funDecl->getNumParams(); i++)
      {
        string tmp = "argv" + to_string(i);
        xmlNodePtr argvi = xmlNewNode(NULL, BAD_CAST tmp.c_str());
        string argvtype = getType(funDecl->parameters()[i]->getType());
        string argvname = funDecl->parameters()[i]->getQualifiedNameAsString();
        xmlNewTextChild(argvi, NULL, BAD_CAST "name", BAD_CAST argvname.c_str());
        xmlNewTextChild(argvi, NULL, BAD_CAST "type", BAD_CAST argvtype.c_str());
        xmlAddChild(argv, argvi);
      }
      return argv;
    }
    xmlNodePtr WalkDeclRefExpr(DeclRefExpr *declrefexpr)
    {
      string name = declrefexpr->getDecl()->getQualifiedNameAsString();
      string type = getType(declrefexpr->getType());
      xmlNodePtr declrefnode;
      declrefnode = xmlNewNode(NULL, BAD_CAST "DeclRef");
      xmlNewTextChild(declrefnode, NULL, BAD_CAST "name", BAD_CAST name.c_str());
      xmlNewTextChild(declrefnode, NULL, BAD_CAST "type", BAD_CAST type.c_str());
      if (FunctionDecl *fundecl = dyn_cast<FunctionDecl>(declrefexpr->getDecl()))
      {
        // string signature = clang::TypeName::getFullyQualifiedName(fundecl->getType(), *this->context).c_str();
        string signature = fundecl->getType().getAsString();
        xmlNewTextChild(declrefnode, NULL, BAD_CAST "signature", BAD_CAST signature.c_str());
        xmlNodePtr argv;
        argv = getFuncArgv(fundecl);
        xmlAddChild(declrefnode, argv);
      }

      return declrefnode;
    }
    xmlNodePtr getDeclNode(string decl)
    {
      xmlNodePtr declnode;
      string type, name;
      vector<string> vdecl;
      vdecl = split(decl, "@");
      type = vdecl[0];
      name = vdecl[1];

      declnode = xmlNewNode(NULL, BAD_CAST "DeclRef");
      xmlNewTextChild(declnode, NULL, BAD_CAST "name", BAD_CAST name.c_str());
      xmlNewTextChild(declnode, NULL, BAD_CAST "type", BAD_CAST type.c_str());
      return declnode;
    }

    xmlNodePtr WalkUnaryOperator(UnaryOperator *unaryoperator)
    {
      // DebugInfo(unaryoperator->getOpcodeStr(unaryoperator->getOpcode()).str());
      string decl;
      Expr *expr = unaryoperator->getSubExpr();
      xmlNodePtr node = NULL, tmpnode = NULL;
      node = xmlNewNode(NULL, BAD_CAST "UnaryOperator");
      xmlNewTextChild(
          node, NULL, BAD_CAST "opcode",
          BAD_CAST unaryoperator->getOpcodeStr(unaryoperator->getOpcode())
              .str()
              .c_str());
      tmpnode = WalkCommonExpr(expr);
      if (tmpnode)
        xmlAddChild(node, tmpnode);
      return node;
    }
    xmlNodePtr WalkFloatingLiteral(FloatingLiteral *floatLiteral)
    {
      xmlNodePtr constant;
      constant = xmlNewNode(NULL, BAD_CAST "FloatingLiteral");
      cout << "FloatingLiteral: " << to_string(floatLiteral->getValueAsApproximateDouble()) << endl;
      xmlNewTextChild(constant, NULL, BAD_CAST "value",
                      BAD_CAST to_string(floatLiteral->getValueAsApproximateDouble()).c_str());
      return constant;
    }

    xmlNodePtr WalkCXXThisExpr(CXXThisExpr *cxxthisexpr)
    {
      string type;
      xmlNodePtr currnode;
      type = getType(cxxthisexpr->getType());
      currnode = xmlNewNode(NULL, BAD_CAST "CXXThisExpr");
      xmlNewTextChild(currnode, NULL, BAD_CAST "Type", BAD_CAST type.c_str());
      // cout << "Expr Type: " << type << endl;
      return currnode;
    }
    xmlNodePtr WalkCommonExpr(Expr *expr)
    {
      xmlNodePtr currnode = NULL;

      if (BinaryOperator *bop = dyn_cast<BinaryOperator>(expr))
      {
        currnode = WalkBinaryOperator(bop);
      }
      else if (ImplicitCastExpr *implicitcastexpr =
                   dyn_cast<ImplicitCastExpr>(expr))
      {
        xmlNodePtr implicit;
        implicit = WalkImplicitCastExpr(implicitcastexpr);
        currnode = xmlNewNode(NULL, BAD_CAST "ImplicitCastExpr");
        xmlNewTextChild(currnode, NULL, BAD_CAST "type", BAD_CAST getType(implicitcastexpr->getType()).c_str());
        xmlAddChild(currnode, implicit);
      }
      else if (CXXOperatorCallExpr *cxxocexpr =
                   dyn_cast<CXXOperatorCallExpr>(expr))
      {
        currnode = WalkCXXOperatorCallExpr(cxxocexpr);
      }
      else if (CXXMemberCallExpr *cxxcall = dyn_cast<CXXMemberCallExpr>(expr))
      {
        currnode = WalkCXXMemberCallExpr(cxxcall);
      }
      else if (CallExpr *callexpr = dyn_cast<CallExpr>(expr))
      {
        currnode = WalkCallExpr(callexpr);
      }
      else if (UnaryOperator *uop = dyn_cast<UnaryOperator>(expr))
      {
        currnode = WalkUnaryOperator(uop);
      }
      else if (ExprWithCleanups *cleanups = dyn_cast<ExprWithCleanups>(expr))
      {
        currnode = WalkExprWithCleanups(cleanups);
      }
      else if (OpaqueValueExpr *opaquevalueexpr =
                   dyn_cast<OpaqueValueExpr>(expr))
      {
        currnode = WalkOpaqueValueExpr(opaquevalueexpr);
      }

      else if (CXXBoolLiteralExpr *cxxbool = dyn_cast<CXXBoolLiteralExpr>(expr))
      {
        currnode = WalkCXXBoolLiteralExpr(cxxbool);
      }
      else if (IntegerLiteral *integerliteral =
                   dyn_cast<IntegerLiteral>(expr))
      {
        currnode = WalkIntegerLiteral(integerliteral);
      }
      else if (ParenExpr *parenexpr = dyn_cast<ParenExpr>(expr))
      {
        currnode = WalkParenExpr(parenexpr);
      }
      else if (CXXBindTemporaryExpr *cbtexpr =
                   dyn_cast<CXXBindTemporaryExpr>(expr))
      {
        currnode = WalkCXXBindTemporaryExpr(cbtexpr);
      }

      else if (CXXStaticCastExpr *cxxstaticcastexpr =
                   dyn_cast<CXXStaticCastExpr>(expr))
      {
        xmlNodePtr cxxstaticcastnode;
        cxxstaticcastnode = WalkCXXStaticCastExpr(cxxstaticcastexpr);
        currnode = xmlNewNode(NULL, BAD_CAST "CXXStaticCastExpr");
        xmlNewTextChild(currnode, NULL, BAD_CAST "type", BAD_CAST getType(cxxstaticcastexpr->getType()).c_str());
        xmlAddChild(currnode, cxxstaticcastnode);
      }
      else if (CXXDependentScopeMemberExpr *cxxdepend = dyn_cast<CXXDependentScopeMemberExpr>(expr))
      {
        currnode = WalkCXXDependentScopeMemberExpr(cxxdepend);
      }

      else if (CStyleCastExpr *cstylecastexpr =
                   dyn_cast<CStyleCastExpr>(expr))
      {
        xmlNodePtr cstyle;
        cstyle = WalkCStyleCastExpr(cstylecastexpr);
        currnode = xmlNewNode(NULL, BAD_CAST "CStyleCastExpr");
        xmlNewTextChild(currnode, NULL, BAD_CAST "type", BAD_CAST getType(cstylecastexpr->getType()).c_str());
        xmlAddChild(currnode, cstyle);
      }
      else if (CXXDefaultArgExpr *cxxdefaultargexpr =
                   dyn_cast<CXXDefaultArgExpr>(expr))
      {
        currnode = WalkCXXDefaultArgExpr(cxxdefaultargexpr);
      }

      else if (UnaryExprOrTypeTraitExpr *uexpr =
                   dyn_cast<UnaryExprOrTypeTraitExpr>(expr))
      {
        currnode = WalkUnaryExprOrTypeTraitExpr(uexpr);
      }

      else if (GNUNullExpr *gnunullexpr = dyn_cast<GNUNullExpr>(expr))
      {
        currnode = xmlNewNode(NULL, BAD_CAST "GNUNullExpr");
        xmlNewTextChild(currnode, NULL, BAD_CAST "value", BAD_CAST "NULL");
      }

      else if (CXXNewExpr *cxxnewexpr = dyn_cast<CXXNewExpr>(expr))
      {
        currnode = WalkCXXNewExpr(cxxnewexpr);
      }

      else if (BinaryConditionalOperator *binaryconditionaloperator =
                   dyn_cast<BinaryConditionalOperator>(expr))
      {
        currnode = WalkBinaryConditionalOperator(binaryconditionaloperator);
      }

      else if (CXXReinterpretCastExpr *cxxreinterpret = dyn_cast<CXXReinterpretCastExpr>(expr))
      {
        currnode = WalkCXXReinterpretCastExpr(cxxreinterpret);
      }
      else if (CXXThisExpr *cxxthisexpr = dyn_cast<CXXThisExpr>(expr))
      {
        currnode = WalkCXXThisExpr(cxxthisexpr);
      }

      else if (DeclRefExpr *declref = dyn_cast<DeclRefExpr>(expr))
      {
        currnode = WalkDeclRefExpr(declref);
      }

      else if (MemberExpr *memberexpr = dyn_cast<MemberExpr>(expr))
      {
        currnode = WalkMemberExpr(memberexpr);
      }
      else if (clang::StringLiteral *stringliteral =
                   dyn_cast<clang::StringLiteral>(expr))
      {
        currnode = WalkStringLiteral(stringliteral);
      }

      else if (CXXConstCastExpr *cxxconstexpr =
                   dyn_cast<CXXConstCastExpr>(expr))
      {
        currnode = WalkCXXConstCastExpr(cxxconstexpr);
      }
      else if (CXXNullPtrLiteralExpr *cxxnullptrliteralexpr =
                   dyn_cast<CXXNullPtrLiteralExpr>(expr))
      {
        currnode = WalkCXXNullPtrLiteralExpr(cxxnullptrliteralexpr);
      }
      else if (PredefinedExpr *predefine = dyn_cast<PredefinedExpr>(expr))
      {
        // TODO:
        ;
      }
      else if (MaterializeTemporaryExpr *mater = dyn_cast<MaterializeTemporaryExpr>(expr))
      {
        currnode = WalkMaterializeTemporaryExpr(mater);
      }
      else if (ArraySubscriptExpr *arraySub = dyn_cast<ArraySubscriptExpr>(expr))
      {
        currnode = WalkArraySubscriptExpr(arraySub);
      }
      else if (ConditionalOperator *conditionaloperator =
                   dyn_cast<ConditionalOperator>(expr))
      {
        currnode = WalkConditionalOperator(conditionaloperator);
      }
      else if (CXXConstructExpr *cxxconstruct = dyn_cast<CXXConstructExpr>(expr))
      {
        currnode = WalkCXXConstructExpr(cxxconstruct);
      }
      else if (CXXFunctionalCastExpr *function = dyn_cast<CXXFunctionalCastExpr>(expr))
      {
        currnode = WalkCXXFunctionalCastExpr(function);
      }
      else if (LambdaExpr *lamda = dyn_cast<LambdaExpr>(expr))
      {
        // for now, we do nothing.
        ;
      }
      else if (FloatingLiteral *floatLiteral = dyn_cast<FloatingLiteral>(expr))
      {
        currnode = WalkFloatingLiteral(floatLiteral);
      }
      else
      {
        DebugInfo("WalkCommonExpr is not completed!");
        expr->dump();
        exit(0);
      }
      return currnode;
    }
    xmlNodePtr WalkParenExpr(ParenExpr *parenexpr)
    {
      xmlNodePtr currnode, tmpnode;
      vector<xmlNodePtr> nodelist;
      string decl;
      for (ParenExpr::child_iterator iter = parenexpr->child_begin();
           iter != parenexpr->child_end(); ++iter)
      {
        tmpnode = NULL;
        if (Expr *expr = dyn_cast<Expr>(*iter))
        {
          tmpnode = WalkCommonExpr(expr);
        }
        else
        {
          DebugInfo("WalkParenExpr is not completed!");
          (*iter)->dump();
          exit(0);
        }
        if (tmpnode)
        {
          nodelist.push_back(tmpnode);
        }
      }
      if (nodelist.size() == 0)
      {
        return NULL;
      }
      else if (nodelist.size() == 1)
      {
        return nodelist[0];
      }
      else
      {
        currnode = xmlNewNode(NULL, BAD_CAST "ParenExpr");
        for (int i = 0; i < nodelist.size(); ++i)
        {
          xmlAddChild(currnode, nodelist[i]);
        }
      }
      return currnode;
    }
    xmlNodePtr WalkCXXOperatorCallExpr(CXXOperatorCallExpr *cxxoperatorcallexpr)
    {
      xmlNodePtr currnode, tmpnode;
      string decl;
      currnode = xmlNewNode(NULL, BAD_CAST "CXXOperatorCallExpr");
      for (CXXOperatorCallExpr::child_iterator iter =
               cxxoperatorcallexpr->child_begin();
           iter != cxxoperatorcallexpr->child_end(); ++iter)
      {
        tmpnode = NULL;
        if (Expr *expr = dyn_cast<Expr>(*iter))
        {
          tmpnode = WalkCommonExpr(expr);
        }
        else
        {
          DebugInfo("WalkCXXOperatorCallExpr is not completed!");
          (*iter)->dump();
          exit(0);
        }
        if (tmpnode)
        {
          xmlAddChild(currnode, tmpnode);
        }
      }
      return currnode;
    }
    xmlNodePtr
    WalkCXXFunctionalCastExpr(CXXFunctionalCastExpr *cxxfunctionalcastexpr)
    {
      xmlNodePtr currnode = NULL;
      Expr *expr = cxxfunctionalcastexpr->getSubExpr();
      currnode = WalkCommonExpr(expr);
      return currnode;
    }
    xmlNodePtr WalkCXXConstructExpr(CXXConstructExpr *cxxconstructexpr)
    {
      xmlNodePtr currnode, tmpnode;
      vector<xmlNodePtr> nodelist;
      string decl;
      currnode = xmlNewNode(NULL, BAD_CAST "CXXConstructExpr");
      string signature = getType(cxxconstructexpr->getConstructor()->getType());
      xmlNewTextChild(currnode, NULL, BAD_CAST "signature",
                      BAD_CAST signature.c_str());
      string name = cxxconstructexpr->getConstructor()->getQualifiedNameAsString();
      xmlNewTextChild(currnode, NULL, BAD_CAST "name",
                      BAD_CAST name.c_str());
      xmlNodePtr member;
      member = xmlNewNode(NULL, BAD_CAST "member");
      for (CXXConstructExpr::child_iterator iter =
               cxxconstructexpr->child_begin();
           iter != cxxconstructexpr->child_end(); iter++)
      {
        tmpnode = NULL;
        if (Expr *expr = dyn_cast<Expr>(*iter))
        {
          tmpnode = WalkCommonExpr(expr);
        }
        else
        {
          DebugInfo("WalkCXXConstructExpr is not completed!");
          (*iter)->dump();
          exit(0);
        }
        if (tmpnode)
          nodelist.push_back(tmpnode);
      }
      if (nodelist.size() == 0)
      {
        ;
      }
      else
      {
        for (int i = 0; i < nodelist.size(); ++i)
        {
          xmlAddChild(member, nodelist[i]);
        }
      }
      xmlAddChild(currnode, member);
      return currnode;
    }
    xmlNodePtr WalkCXXNewExpr(CXXNewExpr *cxxnewexpr)
    {
      xmlNodePtr currnode, tmpnode;
      // currnode = xmlNewNode(NULL, BAD_CAST "CXXNewExpr");
      Expr *expr = cxxnewexpr->getInitializer();
      currnode = WalkCommonExpr(expr);
      return currnode;
    }
    xmlNodePtr WalkCXXConstCastExpr(CXXConstCastExpr *cxxconstcastexpr)
    {
      xmlNodePtr currnode, tmpnode;
      vector<xmlNodePtr> nodelist;
      string decl, type, name;
      vector<string> vdecl;
      for (CXXConstCastExpr::child_iterator iter =
               cxxconstcastexpr->child_begin();
           iter != cxxconstcastexpr->child_end(); iter++)
      {
        tmpnode = NULL;
        if (Expr *expr = dyn_cast<Expr>(*iter))
        {
          tmpnode = WalkCommonExpr(expr);
        }
        else
        {
          DebugInfo("CXXConstCastExpr is not completed!");
          (*iter)->dump();
          exit(0);
        }
        if (tmpnode)
          nodelist.push_back(tmpnode);
      }
      if (nodelist.size() == 0)
      {
        return NULL;
      }
      else if (nodelist.size() == 1)
      {
        return nodelist[0];
      }
      else
      {
        currnode = xmlNewNode(NULL, BAD_CAST "CXXConstCastExpr");
        for (int i = 0; i < nodelist.size(); ++i)
        {
          xmlAddChild(currnode, nodelist[i]);
        }
      }
      return currnode;
    }
    xmlNodePtr
    WalkCXXNullPtrLiteralExpr(CXXNullPtrLiteralExpr *cxxnullptrliteralexpr)
    {
      xmlNodePtr currnode;
      currnode = xmlNewNode(NULL, BAD_CAST "NULL");
      return currnode;
    }

    xmlNodePtr WalkCXXBoolLiteralExpr(CXXBoolLiteralExpr *cxxboolliteralexpr)
    {
      xmlNodePtr currnode;
      bool value = cxxboolliteralexpr->getValue();
      if (value)
      {
        currnode = xmlNewNode(NULL, BAD_CAST "True");
      }
      else
      {
        currnode = xmlNewNode(NULL, BAD_CAST "False");
      }
      return currnode;
    }

    xmlNodePtr WalkOpaqueValueExpr(OpaqueValueExpr *opaquevalueexpr)
    {
      xmlNodePtr currnode = NULL;
      Expr *expr = opaquevalueexpr->getSourceExpr();
      currnode = WalkCommonExpr(expr);
      return currnode;
    }
    xmlNodePtr WalkArraySubscriptExpr(ArraySubscriptExpr *arraysub)
    {
      xmlNodePtr currnode, lhs, rhs, tmpnode;
      int flag = 0;
      string decl;
      currnode = xmlNewNode(NULL, BAD_CAST "ArraySubscriptExpr");

      lhs = xmlNewNode(NULL, BAD_CAST "LHS");
      rhs = xmlNewNode(NULL, BAD_CAST "RHS");
      for (ArraySubscriptExpr::child_iterator iter = arraysub->child_begin();
           iter != arraysub->child_end(); iter++)
      {
        tmpnode = NULL;
        if (Expr *expr = dyn_cast<Expr>(*iter))
        {
          tmpnode = WalkCommonExpr(expr);
        }
        else
        {
          DebugInfo("WalkArraySubscriptExpr is not completed!");
          (*iter)->dump();
          exit(0);
        }
        if (tmpnode)
        {
          if (flag == 0)
          {
            xmlAddChild(lhs, tmpnode);
          }
          else
          {
            xmlAddChild(rhs, tmpnode);
          }
        }
        flag += 1;
        if (flag == 2)
          break;
      }
      xmlAddChild(currnode, lhs);
      xmlAddChild(currnode, rhs);
      return currnode;
    }
    xmlNodePtr WalkImplicitCastExpr(ImplicitCastExpr *implicitcastexpr)
    {
      xmlNodePtr currnode, tmpnode;
      vector<xmlNodePtr> nodelist;
      string decl, type, name;
      vector<string> vdecl;
      bool callExists = false;
      llvm::APSInt apsInt;
      if (implicitcastexpr->EvaluateAsInt(apsInt, *(this->context)))
      {
        // means this is a Int
        // cout << "value: " << apsInt.getExtValue() << endl;
        xmlNodePtr constant;
        constant = xmlNewNode(NULL, BAD_CAST "IntegerLiteral");
        xmlNewTextChild(constant, NULL, BAD_CAST "value",
                        BAD_CAST to_string(apsInt.getExtValue()).c_str());
        Expr *expr = implicitcastexpr->getSubExpr();
        if (DeclRefExpr *decl = dyn_cast<DeclRefExpr>(expr))
        {
          xmlNodePtr declnode = WalkDeclRefExpr(decl);
          xmlAddChild(constant, declnode);
        }
        return constant;
      }
      for (ImplicitCastExpr::child_iterator iter =
               implicitcastexpr->child_begin();
           iter != implicitcastexpr->child_end(); iter++)
      {
        tmpnode = NULL;
        if (Expr *expr = dyn_cast<Expr>(*iter))
        {
          tmpnode = WalkCommonExpr(expr);
        }
        else
        {
          DebugInfo("ImplicitCastExpr is not completed!");
          (*iter)->dump();
          exit(0);
        }
        if (tmpnode)
          nodelist.push_back(tmpnode);
      }
      if (nodelist.size() == 0)
      {
        return NULL;
      }
      else if (nodelist.size() == 1)
      {
        return nodelist[0];
      }
      else
      {
        currnode = xmlNewNode(NULL, BAD_CAST "ImplicitCastExpr");
        for (int i = 0; i < nodelist.size(); ++i)
        {
          xmlAddChild(currnode, nodelist[i]);
        }
      }
      return currnode;
    }

    xmlNodePtr WalkCXXBindTemporaryExpr(CXXBindTemporaryExpr *btexpr)
    {
      xmlNodePtr currnode = NULL;
      // currnode = xmlNewNode(NULL, BAD_CAST "CXXBindTemporaryExpr");
      Expr *subexpr = btexpr->getSubExpr();
      currnode = WalkCommonExpr(subexpr);
      return currnode;
    }
    xmlNodePtr WalkCXXStaticCastExpr(CXXStaticCastExpr *cxxstaticcastexpr)
    {
      xmlNodePtr currnode = NULL;
      // currnode = xmlNewNode(NULL, BAD_CAST "CXXStaticCastExpr");
      for (CXXStaticCastExpr::child_iterator iter =
               cxxstaticcastexpr->child_begin();
           iter != cxxstaticcastexpr->child_end(); iter++)
      {
        if (Expr *expr = dyn_cast<Expr>(*iter))
        {
          currnode = WalkCommonExpr(expr);
        }
        else
        {
          DebugInfo("WalkCXXStatisCastExpr is not completed!");
          (*iter)->dump();
          exit(0);
        }
      }
      return currnode;
    }
    xmlNodePtr WalkMaterializeTemporaryExpr(MaterializeTemporaryExpr *mtexpr)
    {
      xmlNodePtr currnode, tmpnode;
      vector<xmlNodePtr> nodelist;
      string type = getType(mtexpr->getType());
      // DebugInfo(type);
      for (MaterializeTemporaryExpr::child_iterator iter = mtexpr->child_begin();
           iter != mtexpr->child_end(); ++iter)
      {
        tmpnode = NULL;
        if (Expr *expr = dyn_cast<Expr>(*iter))
        {
          tmpnode = WalkCommonExpr(expr);
        }
        else
        {
          DebugInfo("MaterializeTemporaryExpr is not completed!");
          mtexpr->dump();
          exit(0);
        }
        if (tmpnode)
          nodelist.push_back(tmpnode);
      }
      if (nodelist.size() == 0)
      {
        return NULL;
      }
      else if (nodelist.size() == 1)
      {
        return nodelist[0];
      }
      else
      {
        currnode = xmlNewNode(NULL, BAD_CAST "MaterializeTemporaryExpr");
        for (int i = 0; i < nodelist.size(); ++i)
        {
          xmlAddChild(currnode, nodelist[i]);
        }
      }
      return currnode;
    }

    xmlNodePtr WalkCStyleCastExpr(CStyleCastExpr *cexpr)
    {
      xmlNodePtr currnode; //, tmpnode;
      //currnode = xmlNewNode(NULL, BAD_CAST "CStyleCastExpr");
      Expr *expr = cexpr->getSubExpr();
      currnode = WalkCommonExpr(expr);
      return currnode;
    }
    xmlNodePtr WalkExprWithCleanups(ExprWithCleanups *exprwithcleanups)
    {
      xmlNodePtr currnode, tmpnode;
      // exprwithcleanups->dump();
      vector<xmlNodePtr> nodelist;
      for (ExprWithCleanups::child_iterator iter =
               exprwithcleanups->child_begin();
           iter != exprwithcleanups->child_end(); iter++)
      {
        tmpnode = NULL;
        if (Expr *expr = dyn_cast<Expr>(*iter))
        {
          tmpnode = WalkCommonExpr(expr);
        }
        else
        {
          DebugInfo("The WalkExprWithCleanups is not completed!");
          (*iter)->dump();
          exit(0);
        }
        if (tmpnode)
          nodelist.push_back(tmpnode);
      }
      if (nodelist.size() == 0)
      {
        return NULL;
      }
      else if (nodelist.size() == 1)
      {
        return nodelist[0];
      }
      else
      {
        currnode = xmlNewNode(NULL, BAD_CAST "ExprWithCleanups");
        for (int i = 0; i < nodelist.size(); ++i)
        {
          xmlAddChild(currnode, nodelist[i]);
        }
      }
      return currnode;
    }
    xmlNodePtr WalkUnaryExprOrTypeTraitExpr(UnaryExprOrTypeTraitExpr *uexpr)
    {
      xmlNodePtr currnode;
      currnode = xmlNewNode(NULL, BAD_CAST "UnaryExprOrTypeTraitExpr");
      llvm::APSInt apsInt;
      if (uexpr->isIntegerConstantExpr(apsInt, *this->context))
      {
        xmlNewTextChild(currnode, NULL, BAD_CAST "type", BAD_CAST "IntegerLiteral");
        xmlNewTextChild(currnode, NULL, BAD_CAST "value", BAD_CAST to_string(apsInt.getExtValue()).c_str());
        if (uexpr->getKind() == clang::UnaryExprOrTypeTrait::UETT_SizeOf)
        {
          string argtype = getType(uexpr->getTypeOfArgument());
          xmlNewTextChild(currnode, NULL, BAD_CAST "sizeof", BAD_CAST argtype.c_str());
        }
        return currnode;
      }
      if (uexpr->getKind() == clang::UnaryExprOrTypeTrait::UETT_SizeOf)
      {
        xmlNewTextChild(currnode, NULL, BAD_CAST "op", BAD_CAST "sizeof");
      }
      else
      {
        DebugInfo("WalkUnaryExprOr is not completed!");
        exit(0);
      }
      bool istype = uexpr->isArgumentType();
      if (istype)
      {
        string argtype = getType(uexpr->getArgumentType());
        xmlNewTextChild(currnode, NULL, BAD_CAST "ArgType",
                        BAD_CAST argtype.c_str());
      }
      else
      {
        Expr *expr = uexpr->getArgumentExpr();
        xmlNodePtr argnode = NULL;
        argnode = WalkCommonExpr(expr);
        xmlAddChild(currnode, argnode);
      }

      // xmlNewTextChild(currnode, NULL, BAD_CAST "op", BAD_CAST "to_be_done");
      return currnode;
    }
    xmlNodePtr WalkBinaryExpr(Expr *binaryexpr, string root)
    {
      xmlNodePtr currnode = NULL, tmpnode = NULL;
      currnode = xmlNewNode(NULL, BAD_CAST root.c_str());
      tmpnode = WalkCommonExpr(binaryexpr);
      xmlAddChild(currnode, tmpnode);
      return currnode;
    }
    xmlNodePtr WalkCXXReinterpretCastExpr(CXXReinterpretCastExpr *cxxreinterpret)
    {
      xmlNodePtr currnode, tmpnode;
      vector<xmlNodePtr> nodelist;
      string decl, type, name;
      vector<string> vdecl;
      bool callExists = false;
      llvm::APSInt apsInt;
      for (CXXReinterpretCastExpr::child_iterator iter =
               cxxreinterpret->child_begin();
           iter != cxxreinterpret->child_end(); iter++)
      {
        tmpnode = NULL;
        if (Expr *expr = dyn_cast<Expr>(*iter))
        {
          tmpnode = WalkCommonExpr(expr);
        }
        else
        {
          DebugInfo("CXXReinterpretCastExpr is not completed!");
          (*iter)->dump();
          exit(0);
        }
        if (tmpnode)
          nodelist.push_back(tmpnode);
      }
      if (nodelist.size() == 0)
      {
        return NULL;
      }
      else if (nodelist.size() == 1)
      {
        return nodelist[0];
      }
      else
      {
        currnode = xmlNewNode(NULL, BAD_CAST "CXXReinterpretCastExpr");
        for (int i = 0; i < nodelist.size(); ++i)
        {
          xmlAddChild(currnode, nodelist[i]);
        }
      }
      return currnode;
    }
    xmlNodePtr WalkBinaryConditionalOperator(
        BinaryConditionalOperator *binaryconditionaloperator)
    {
      xmlNodePtr currnode, condnode, truenode, falsenode;

      currnode = xmlNewNode(NULL, BAD_CAST "BinaryConditionalOperator");
      condnode =
          WalkCond(binaryconditionaloperator->getCond(), "BinaryCondition");
      truenode =
          WalkBinaryExpr(binaryconditionaloperator->getTrueExpr(), "TrueNode");
      if (condnode == NULL)
      {
        xmlAddChild(currnode, truenode);
      }
      else
      {
        xmlAddChild(currnode, condnode);
      }
      xmlAddChild(currnode, truenode);
      falsenode =
          WalkBinaryExpr(binaryconditionaloperator->getFalseExpr(), "FalseNode");
      xmlAddChild(currnode, falsenode);
      return currnode;
    }
    xmlNodePtr WalkBinaryOperator(BinaryOperator *bop)
    {
      xmlNodePtr currnode, lhs, rhs, tmpnode;
      int flag = 0;
      string decl;
      currnode = xmlNewNode(NULL, BAD_CAST "BinaryOperator");
      string opcode;
      opcode = bop->getOpcodeStr(bop->getOpcode()).str();
      // cout << "The opcode is " << opcode << endl;
      // bop->dump();

      xmlNewTextChild(currnode, NULL, BAD_CAST "opcode", BAD_CAST opcode.c_str());
      lhs = xmlNewNode(NULL, BAD_CAST "LHS");
      rhs = xmlNewNode(NULL, BAD_CAST "RHS");
      for (BinaryOperator::child_iterator iter = bop->child_begin();
           iter != bop->child_end(); iter++)
      {
        tmpnode = NULL;
        if (Expr *expr = dyn_cast<Expr>(*iter))
        {
          tmpnode = WalkCommonExpr(expr);
        }
        else
        {
          DebugInfo("WalkBinaryOperator is not completed!");
          (*iter)->dump();
          exit(0);
        }
        if (tmpnode)
        {
          if (flag == 0)
          {
            xmlAddChild(lhs, tmpnode);
          }
          else
          {
            xmlAddChild(rhs, tmpnode);
          }
        }
        flag += 1;
        if (flag == 2)
          break;
      }
      xmlAddChild(currnode, lhs);
      xmlAddChild(currnode, rhs);
      return currnode;
    }

    xmlNodePtr WalkCXXDefaultArgExpr(CXXDefaultArgExpr *cxxdefaultargexpr)
    {
      return NULL;
    }
    /************Call Related Part******************/
    xmlNodePtr WalkMemberExpr(MemberExpr *memexpr)
    {
      string funcname = memexpr->getMemberDecl()->getQualifiedNameAsString();

      string type = getType(memexpr->getType());
      string decl;
      xmlNodePtr currnode, tmpnode;
      currnode = xmlNewNode(NULL, BAD_CAST "MemberExpr");

      // memexpr->dump();
      // cout << "Value Decl: " << endl;
      // memexpr->getMemberDecl()->dump();
      // cout << "getType: " << endl;
      // cout << getType(memexpr->getMemberDecl()->getType()) << endl;
      // cout << memexpr->getMemberDecl()->getQualifiedNameAsString() << endl;
      // getchar();

      xmlNewTextChild(currnode, NULL, BAD_CAST "name", BAD_CAST funcname.c_str());
      xmlNewTextChild(currnode, NULL, BAD_CAST "type", BAD_CAST type.c_str());
      xmlNewTextChild(currnode, NULL, BAD_CAST "signature", BAD_CAST memexpr->getMemberDecl()->getType().getAsString().c_str()); //clang::TypeName::getFullyQualifiedName(memexpr->getMemberDecl()->getType(), *this->context).c_str());
      for (MemberExpr::child_iterator iter = memexpr->child_begin();
           iter != memexpr->child_end(); iter++)
      {
        if (Expr *expr = dyn_cast<Expr>(*iter))
        {
          tmpnode = WalkCommonExpr(expr);
        }
        else
        {
          DebugInfo("WalkMemberExpr is not completed!");
          memexpr->dump();
          exit(0);
        }
        if (tmpnode)
          xmlAddChild(currnode, tmpnode);
      }
      return currnode;
    }

    xmlNodePtr WalkCXXMemberCallExpr(CXXMemberCallExpr *mcallexpr)
    {
      string funccalls = "";
      vector<string> tmp;
      string member, ownername, ownertype, funcname, decl;
      xmlNodePtr currnode, tmpnode, argvnode = NULL, funcnode = NULL,
                                    funccont = NULL;
      currnode = xmlNewNode(NULL, BAD_CAST "CXXMemberCallExpr");
      // DebugInfo("Starting Visiting CXXMemberCallExpr");
      // mcallexpr->dump();
      Expr *expr = mcallexpr->getCallee();
      string returntype = getType(mcallexpr->getType());
      xmlNewTextChild(currnode, NULL, BAD_CAST "ReturnType",
                      BAD_CAST returntype.c_str());
      // DebugInfo("Return Type Info: " + returntype);
      // expr->dump();
      if (MemberExpr *memexpr = dyn_cast<MemberExpr>(expr))
      {
        DebugInfo("Start Walking MemberExpr");
        funcnode = WalkMemberExpr(memexpr);
        xmlAddChild(currnode, funcnode);
      }
      else
      {
        DebugInfo("MemberExpr should exists....");
        mcallexpr->dump();
        exit(0);
      }
      DebugInfo("Member call argument: ");
      argvnode = xmlNewNode(NULL, BAD_CAST "argv");
      for (CXXMemberCallExpr::arg_iterator iter = mcallexpr->arg_begin();
           iter != mcallexpr->arg_end(); iter++)
      {
        tmpnode = NULL;
        if (Expr *expr = dyn_cast<Expr>(*iter))
        {
          tmpnode = WalkCommonExpr(expr);
        }
        else
        {
          DebugInfo("CXXMemberCallExpr is not completed!");
          (*iter)->dump();
          exit(0);
        }
        if (tmpnode)
          xmlAddChild(argvnode, tmpnode);
        // DebugInfo(funccalls);
      }
      xmlAddChild(currnode, argvnode);
      // xmlAddChild(currnode, funcnode);
      return currnode;
    }
    xmlNodePtr WalkCXXDependentScopeMemberExpr(CXXDependentScopeMemberExpr *cxxdepend)
    {
      string name = cxxdepend->getMember().getAsString();
      string type = getType(cxxdepend->getType());
      xmlNodePtr currnode, tmpnode;
      currnode = xmlNewNode(NULL, BAD_CAST "MemberExpr");
      xmlNewTextChild(currnode, NULL, BAD_CAST "name", BAD_CAST name.c_str());
      xmlNewTextChild(currnode, NULL, BAD_CAST "type", BAD_CAST type.c_str());
      return currnode;
    }
    xmlNodePtr WalkUnresolvedMemberExpr(UnresolvedMemberExpr *unresolve)
    {
      unresolve->getBase()->dump();
      xmlNodePtr currnode = xmlNewNode(NULL, BAD_CAST "MemberExpr");
      if (Expr *expr = dyn_cast<Expr>(unresolve->getBase()))
      {
        xmlNodePtr tmpnode = WalkCommonExpr(expr);
        xmlAddChild(currnode, tmpnode);
      }
      else
      {
        cout << "WalkUnresolvedMemberExpr is not completed!" << endl;
        exit(0);
      }
      return currnode;
    }
    xmlNodePtr WalkCallExpr(CallExpr *callexpr)
    {
      string funccalls = "", funcname = "", decl;

      // DebugInfo("Starting Visiting CallExpr");
      Expr *expr = callexpr->getCallee();
      string type = getType(callexpr->getType());

      // DebugInfo("Return Type Info: " + type);
      // expr->dump();
      xmlNodePtr currnode, tmpnode, argvnode = NULL, funcnode = NULL,
                                    funccont = NULL;
      currnode = xmlNewNode(NULL, BAD_CAST "CallExpr");

      xmlNewTextChild(currnode, NULL, BAD_CAST "ReturnType",
                      BAD_CAST type.c_str());
      if (MemberExpr *memexpr = dyn_cast<MemberExpr>(expr))
      {
        funcnode = WalkMemberExpr(memexpr);
        xmlAddChild(currnode, funcnode);
      }
      else if (ImplicitCastExpr *implicitcastexpr =
                   dyn_cast<ImplicitCastExpr>(expr))
      {
        tmpnode = WalkImplicitCastExpr(implicitcastexpr);
        funcnode = xmlNewNode(NULL, BAD_CAST "MemberExpr");
        xmlAddChild(funcnode, tmpnode);
        xmlAddChild(currnode, funcnode);
      }
      else if (CXXDependentScopeMemberExpr *cxxdepend = dyn_cast<CXXDependentScopeMemberExpr>(expr))
      {
        funcnode = WalkCXXDependentScopeMemberExpr(cxxdepend);
        xmlAddChild(currnode, funcnode);
      }
      else if (UnresolvedMemberExpr *unresolved = dyn_cast<UnresolvedMemberExpr>(expr))
      {
        funcnode = WalkUnresolvedMemberExpr(unresolved);
        xmlAddChild(currnode, funcnode);
      }
      else if (ParenExpr *parenexpr = dyn_cast<ParenExpr>(expr))
      {
        funcnode = WalkParenExpr(parenexpr);
        xmlAddChild(currnode, funcnode);
      }
      else
      {
        DebugInfo("WalkCallExpr membercall  is not completed!");
        callexpr->dump();
        exit(0);
      }
      // DebugInfo("Member call argument: ");
      argvnode = xmlNewNode(NULL, BAD_CAST "argv");
      for (CallExpr::arg_iterator iter = callexpr->arg_begin();
           iter != callexpr->arg_end(); iter++)
      {
        tmpnode = NULL;
        if (Expr *expr = dyn_cast<Expr>(*iter))
        {
          tmpnode = WalkCommonExpr(expr);
        }
        else
        {
          DebugInfo("Call Expr Argv is not completed!");
          (*iter)->dump();
          exit(0);
        }
        if (tmpnode)
          xmlAddChild(argvnode, tmpnode);
      }
      // DebugInfo("Ending Visit Call Expr");

      xmlAddChild(currnode, argvnode);
      return currnode;
    }

    xmlNodePtr WalkVar(VarDecl *var)
    {
      xmlNodePtr currnode = NULL, lhs = NULL, rhs = NULL, tmpnode = NULL;
      currnode = xmlNewNode(NULL, BAD_CAST "Var");
      // var->getInit()->dump();
      // var->getAnyInitializer()->dump();
      lhs = xmlNewNode(NULL, BAD_CAST "lhs");
      xmlNewTextChild(lhs, NULL, BAD_CAST "name",
                      BAD_CAST var->getQualifiedNameAsString().c_str());
      xmlNewTextChild(lhs, NULL, BAD_CAST "type",
                      BAD_CAST getType(var->getType()).c_str());

      xmlAddChild(currnode, lhs);

      Expr *expr = var->getInit();
      tmpnode = WalkCommonExpr(expr);
      if (tmpnode)
      {
        rhs = xmlNewNode(NULL, BAD_CAST "rhs");
        xmlAddChild(rhs, tmpnode);
        xmlAddChild(currnode, rhs);
      }
      return currnode;
    }

    xmlNodePtr WalkDeclStmt(DeclStmt *stmt)
    {
      xmlNodePtr currnode, tmpnode;
      vector<xmlNodePtr> nodelist;
      for (DeclStmt::decl_iterator iter = stmt->decl_begin();
           iter != stmt->decl_end(); iter++)
      {
        tmpnode = NULL;
        if (VarDecl *var = dyn_cast<VarDecl>(*iter))
        {
          tmpnode = WalkVar(var);
        }
        else
        {
          DebugInfo("Decl Stmt is not completed!");
          (*iter)->dump();
          exit(0);
        }
        if (tmpnode)
          nodelist.push_back(tmpnode);
      }
      if (nodelist.size() == 0)
      {
        return NULL;
      }
      else if (nodelist.size() == 1)
      {
        return nodelist[0];
      }
      else
      {
        currnode = xmlNewNode(NULL, BAD_CAST "DeclStmt");
        for (int i = 0; i < nodelist.size(); ++i)
        {
          xmlAddChild(currnode, nodelist[i]);
        }
      }
      return currnode;
    }

    xmlNodePtr WalkForInit(Stmt *init)
    {
      xmlNodePtr currnode, tmpnode;
      currnode = xmlNewNode(NULL, BAD_CAST "ForInit");
      if (init == NULL)
      {
        return currnode;
      }
      if (DeclStmt *declstmt = dyn_cast<DeclStmt>(init))
      {
        tmpnode = WalkDeclStmt(declstmt);
      }
      else
      {
        DebugInfo("WalkForInit is not completed!");
        init->dump();
        exit(0);
      }
      xmlAddChild(currnode, tmpnode);
      return currnode;
    }
    xmlNodePtr WalkForCond(Expr *cond)
    {
      xmlNodePtr currnode, tmpnode;
      currnode = xmlNewNode(NULL, BAD_CAST "ForCond");
      tmpnode = WalkCommonExpr(cond);
      xmlAddChild(currnode, tmpnode);
      return currnode;
    }
    xmlNodePtr WalkForInc(Expr *inc)
    {
      xmlNodePtr currnode, tmpnode;
      currnode = xmlNewNode(NULL, BAD_CAST "ForInc");
      tmpnode = WalkCommonExpr(inc);
      xmlAddChild(currnode, tmpnode);
      return currnode;
    }
    xmlNodePtr WalkForBody(Stmt *body)
    {
      // body->dump();

      xmlNodePtr currnode, tmpnode;
      currnode = xmlNewNode(NULL, BAD_CAST "ForBody");
      if (CompoundStmt *cstmt = dyn_cast<CompoundStmt>(body))
      {
        tmpnode = WalkCompoundStmt(cstmt);
      }
      else
      {
        DebugInfo("WalkForBody is not completed!");
        body->dump();
        exit(0);
      }
      xmlAddChild(currnode, tmpnode);
      return currnode;
    }
    xmlNodePtr WalkForStmt(ForStmt *forstmt)
    {
      xmlNodePtr currnode, initnode, condnode, incnode, bodynode;
      // forstmt->dump();
      currnode = xmlNewNode(NULL, BAD_CAST "ForStmt");
      Stmt *init = forstmt->getInit();
      initnode = WalkForInit(init);
      xmlAddChild(currnode, initnode);
      // DebugInfo("init: ");
      // init->dump();
      Expr *cond = forstmt->getCond();
      condnode = WalkForCond(cond);
      xmlAddChild(currnode, condnode);
      // DebugInfo("Cond: ");
      //  cond->dump();
      Expr *inc = forstmt->getInc();
      incnode = WalkForInc(inc);
      xmlAddChild(currnode, incnode);
      // DebugInfo("Inc:");
      // inc->dump();
      // DebugInfo("-------------------------------------");
      Stmt *body = forstmt->getBody();
      bodynode = WalkForBody(body);
      xmlAddChild(currnode, bodynode);
      return currnode;
    }
    xmlNodePtr WalkCXXForRangeStmt(CXXForRangeStmt *cxxforrangestmt)
    {
      xmlNodePtr currnode, rangenode, beginnode, endnode, condnode, incnode,
          bodynode, tmpnode;
      currnode = xmlNewNode(NULL, BAD_CAST "CXXForRangeStmt");
      // cxxforrangestmt->dump();
      DeclStmt *range = cxxforrangestmt->getRangeStmt();
      // range->dump();
      rangenode = WalkDeclStmt(range);
      xmlAddChild(currnode, rangenode);

      DeclStmt *begin = cxxforrangestmt->getBeginStmt();
      if (begin != NULL)
      {
        beginnode = WalkDeclStmt(begin);
        xmlAddChild(currnode, beginnode);
      }

      DeclStmt *end = cxxforrangestmt->getEndStmt();
      if (end != NULL)
      {
        endnode = WalkDeclStmt(end);
        xmlAddChild(currnode, endnode);
      }

      // DeclStmt *loopvar = cxxforrangestmt->getLoopVarStmt();

      Expr *cond = cxxforrangestmt->getCond();
      if (cond != NULL)
      {
        condnode = WalkForCond(cond);
        xmlAddChild(currnode, condnode);
      }

      Expr *inc = cxxforrangestmt->getInc();
      if (inc != NULL)
      {
        incnode = WalkForInc(inc);
        xmlAddChild(currnode, incnode);
      }

      Stmt *body = cxxforrangestmt->getBody();
      bodynode = WalkForBody(body);
      xmlAddChild(currnode, bodynode);

      return currnode;
    }

    xmlNodePtr WalkConditionalOperator(ConditionalOperator *coperator)
    {
      xmlNodePtr currnode, tmpnode;
      vector<xmlNodePtr> nodelist;
      for (ConditionalOperator::child_iterator iter = coperator->child_begin();
           iter != coperator->child_end(); iter++)
      {
        tmpnode = NULL;
        if (Expr *expr = dyn_cast<Expr>(*iter))
        {
          tmpnode = WalkCommonExpr(expr);
        }
        else
        {
          DebugInfo("WalklConditionalOperator is not completed!");
          (*iter)->dump();
          exit(0);
        }
        if (tmpnode)
          nodelist.push_back(tmpnode);
      }
      if (nodelist.size() == 0)
      {
        return NULL;
      }
      else if (nodelist.size() == 1)
      {
        return nodelist[0];
      }
      else
      {
        currnode = xmlNewNode(NULL, BAD_CAST "ConditionalOperator");
        for (int i = 0; i < nodelist.size(); ++i)
        {
          xmlAddChild(currnode, nodelist[i]);
        }
      }
      return currnode;
    }
    /***If Stmt***/
    /** Walk Condition  **/
    xmlNodePtr WalkCond(Expr *cond, string root)
    {
      xmlNodePtr currnode, tmpnode = NULL;
      currnode = xmlNewNode(NULL, BAD_CAST root.c_str());
      cout << root << endl;
      // cond->dump();
      tmpnode = WalkCommonExpr(cond);
      if (tmpnode)
        xmlAddChild(currnode, tmpnode);
      return currnode;
    }
    xmlNodePtr WalkIfThen(Stmt *then)
    {
      xmlNodePtr currnode, tmpnode = NULL;
      currnode = xmlNewNode(NULL, BAD_CAST "Then");
      if (ReturnStmt *rstmt = dyn_cast<ReturnStmt>(then))
      {
        ;
      }
      else if (CompoundStmt *cstmt = dyn_cast<CompoundStmt>(then))
      {
        tmpnode = WalkCompoundStmt(cstmt);
      }
      else if (BreakStmt *stmt = dyn_cast<BreakStmt>(then))
      {
        // TODO:
        ;
      }
      else if (BinaryOperator *bop = dyn_cast<BinaryOperator>(then))
      {
        tmpnode = WalkBinaryOperator(bop);
      }
      else if (CXXOperatorCallExpr *bop = dyn_cast<CXXOperatorCallExpr>(then))
      {
        tmpnode = WalkCXXOperatorCallExpr(bop);
      }
      else
      {
        DebugInfo("WalkIfThen is not completed!");
        then->dump();
        exit(0);
      }
      if (tmpnode)
        xmlAddChild(currnode, tmpnode);
      return currnode;
    }
    xmlNodePtr WalkIfElse(Stmt *elsestmt)
    {
      xmlNodePtr currnode, tmpnode = NULL;
      currnode = xmlNewNode(NULL, BAD_CAST "Else");

      if (CompoundStmt *cstmt = dyn_cast<CompoundStmt>(elsestmt))
      {
        tmpnode = WalkCompoundStmt(cstmt);
      }
      else if (IfStmt *ifstmt = dyn_cast<IfStmt>(elsestmt))
      {
        tmpnode = WalkIfStmt(ifstmt);
      }
      else if (CXXOperatorCallExpr *cxxopcall = dyn_cast<CXXOperatorCallExpr>(elsestmt))
      {
        tmpnode = WalkCXXOperatorCallExpr(cxxopcall);
      }
      else
      {
        DebugInfo("IfElse is not completed!");
        elsestmt->dump();
        exit(0);
      }
      if (tmpnode)
        xmlAddChild(currnode, tmpnode);
      return currnode;
    }
    xmlNodePtr WalkIfStmt(IfStmt *ifstmt)
    {
      xmlNodePtr currnode, condnode, thennode, elsenode;
      DebugInfo("Walk If Stmt");
      // ifstmt->dump();
      currnode = xmlNewNode(NULL, BAD_CAST "IfStmt");
      if (Stmt *init = ifstmt->getInit())
      {
        init->dump();
        xmlNodePtr initnode = NULL;
        if (DeclStmt *decl = dyn_cast<DeclStmt>(init))
        {
          initnode = WalkDeclStmt(decl);
        }
        else
        {
          DebugInfo("If Stmt init is not completed!");
          init->dump();
          exit(0);
        }
        if (initnode != NULL)
        {
          xmlAddChild(currnode, initnode);
        }
      }
      if (const DeclStmt *decl = ifstmt->getConditionVariableDeclStmt())
      {
        xmlNodePtr varnode = NULL;
        DeclStmt stmt1 = *decl;
        varnode = WalkDeclStmt(&stmt1);
        xmlAddChild(currnode, varnode);
      }
      Expr *cond = ifstmt->getCond();
      condnode = WalkCond(cond, "IfCond");
      xmlAddChild(currnode, condnode);
      Stmt *then = ifstmt->getThen();
      thennode = WalkIfThen(then);
      xmlAddChild(currnode, thennode);
      Stmt *elsestmt = ifstmt->getElse();
      if (elsestmt)
      {
        elsenode = WalkIfElse(elsestmt);
        xmlAddChild(currnode, elsenode);
      }
      return currnode;
    }

    xmlNodePtr WalkWhileStmt(WhileStmt *whilestmt)
    {
      xmlNodePtr currnode, condnode, bodynode, tmpnode;
      currnode = xmlNewNode(NULL, BAD_CAST "WhileStmt");
      Expr *expr = whilestmt->getCond();
      condnode = WalkCond(expr, "WhileCond");
      xmlAddChild(currnode, condnode);
      Stmt *stmt = whilestmt->getBody();
      if (CompoundStmt *cstmt = dyn_cast<CompoundStmt>(stmt))
      {
        tmpnode = WalkCompoundStmt(cstmt);
      }
      else
      {
        DebugInfo("WhileStmt body is not completed!");
        stmt->dump();
        exit(0);
      }
      bodynode = xmlNewNode(NULL, BAD_CAST "WhileBody");
      if (tmpnode)
        xmlAddChild(bodynode, tmpnode);
      xmlAddChild(currnode, bodynode);
      return currnode;
    }
    xmlNodePtr WalkDoStmt(DoStmt *dostmt)
    {
      xmlNodePtr currnode, condnode, bodynode, tmpnode;
      currnode = xmlNewNode(NULL, BAD_CAST "DoStmt");
      Expr *expr = dostmt->getCond();
      condnode = WalkCond(expr, "DoCond");
      xmlAddChild(currnode, condnode);
      Stmt *stmt = dostmt->getBody();
      if (CompoundStmt *cstmt = dyn_cast<CompoundStmt>(stmt))
      {
        tmpnode = WalkCompoundStmt(cstmt);
      }
      else
      {
        DebugInfo("DoStmt body is not completed!");
        stmt->dump();
        exit(0);
      }
      bodynode = xmlNewNode(NULL, BAD_CAST "DoBody");
      if (tmpnode)
        xmlAddChild(bodynode, tmpnode);
      xmlAddChild(currnode, bodynode);
      return currnode;
    }
    xmlNodePtr WalkReturnStmt(ReturnStmt *returnstmt)
    {
      xmlNodePtr currnode, tmpnode;
      vector<xmlNodePtr> nodelist;

      for (ReturnStmt::child_iterator iter = returnstmt->child_begin();
           iter != returnstmt->child_end(); ++iter)
      {
        tmpnode = NULL;
        if (Expr *expr = dyn_cast<Expr>(*iter))
        {
          tmpnode = WalkCommonExpr(expr);
        }
        else
        {
          DebugInfo("Walk ReturnStmt is not completed!");
          returnstmt->dump();
          exit(0);
        }
        if (tmpnode)
          nodelist.push_back(tmpnode);
      }
      if (nodelist.size() == 0)
      {
        return NULL;
      }
      else
      {
        currnode = xmlNewNode(NULL, BAD_CAST "ReturnStmt");
        for (int i = 0; i < nodelist.size(); ++i)
        {
          xmlAddChild(currnode, nodelist[i]);
        }
      }
      return currnode;
    }
    /***CompoundStmt Part***/
    xmlNodePtr WalkCompoundStmt(CompoundStmt *cstmt)
    {
      string funccalls = "";
      xmlNodePtr currnode, tmpnode;
      vector<xmlNodePtr> nodelist;

      vector<string> tmp;
      // cout << "dump CompoundStmt" << endl;
      // cstmt->dump();
      for (CompoundStmt::body_iterator iter = cstmt->body_begin();
           iter != cstmt->body_end(); iter++)
      {
        // (*iter)->dump();
        if (ReturnStmt *returnstmt = dyn_cast<ReturnStmt>(*iter))
        {
          tmpnode = WalkReturnStmt(returnstmt);
        }
        else if (!isDataReplyExists<Stmt *>(*iter))
        {
          continue;
        }
        else if (dyn_cast<NullStmt>(*iter))
        {
          continue;
        }
        else if (DeclStmt *stmt = dyn_cast<DeclStmt>(*iter))
        {
          tmpnode = WalkDeclStmt(stmt);
        }
        else if (CXXMemberCallExpr *cxxmembercallexpr =
                     dyn_cast<CXXMemberCallExpr>(*iter))
        {
          tmpnode = WalkCXXMemberCallExpr(cxxmembercallexpr);
        }
        else if (CXXOperatorCallExpr *cxxcall = dyn_cast<CXXOperatorCallExpr>(*iter))
        {
          tmpnode = WalkCXXOperatorCallExpr(cxxcall);
        }
        else if (CallExpr *callexpr = dyn_cast<CallExpr>(*iter))
        {
          tmpnode = WalkCallExpr(callexpr);
        }
        else if (ExprWithCleanups *exprwithcleanups =
                     dyn_cast<ExprWithCleanups>(*iter))
        {
          tmpnode = WalkExprWithCleanups(exprwithcleanups);
        }
        else if (ForStmt *forstmt = dyn_cast<ForStmt>(*iter))
        {
          tmpnode = WalkForStmt(forstmt);
        }
        else if (IfStmt *ifstmt = dyn_cast<IfStmt>(*iter))
        {
          tmpnode = WalkIfStmt(ifstmt);
        }
        else if (BinaryOperator *bop = dyn_cast<BinaryOperator>(*iter))
        {
          tmpnode = WalkBinaryOperator(bop);
        }
        else if (CXXForRangeStmt *cxxforrangestmt =
                     dyn_cast<CXXForRangeStmt>(*iter))
        {
          tmpnode = WalkCXXForRangeStmt(cxxforrangestmt);
        }
        else if (WhileStmt *whilestmt = dyn_cast<WhileStmt>(*iter))
        {
          tmpnode = WalkWhileStmt(whilestmt);
        }
        else if (CStyleCastExpr *cstyleexpr = dyn_cast<CStyleCastExpr>(*iter))
        {
          tmpnode = WalkCStyleCastExpr(cstyleexpr);
        }
        else if (SwitchStmt *switchStmt = dyn_cast<SwitchStmt>(*iter))
        {
          tmpnode = WalkSwitch(switchStmt);
        }
        else if (LabelStmt *labelStmt = dyn_cast<LabelStmt>(*iter))
        {
          continue;
        }
        else if (ParenExpr *parenexpr = dyn_cast<ParenExpr>(*iter))
        {
          tmpnode = WalkParenExpr(parenexpr);
        }
        else if (DoStmt *dostmt = dyn_cast<DoStmt>(*iter))
        {
          tmpnode = WalkDoStmt(dostmt);
        }
        else if (CompoundStmt *cstmt1 = dyn_cast<CompoundStmt>(*iter))
        {
          tmpnode = WalkCompoundStmt(cstmt1);
        }
        else if (CXXDeleteExpr *deleteExpr = dyn_cast<CXXDeleteExpr>(*iter))
        {
          tmpnode = NULL;
        }
        else
        {
          DebugInfo("CompoundStmt is not completed!");
          (*iter)->dump();
          exit(0);
        }
        if (tmpnode)
          nodelist.push_back(tmpnode);
      }
      if (nodelist.size() == 0)
      {
        return NULL;
      }
      // else if (nodelist.size() == 1)
      // {
      //   return nodelist[0];
      // }
      else
      {
        currnode = xmlNewNode(NULL, BAD_CAST "CompoundStmt");
        for (int i = 0; i < nodelist.size(); ++i)
        {
          xmlAddChild(currnode, nodelist[i]);
        }
      }
      return currnode;
    }

    /****Case Part***/

    string GetCaseValue(Expr *expr)
    {
      string ret = "";
      if (expr)
      {
        // expr->dump();
        if (ImplicitCastExpr *icastexpr = dyn_cast<ImplicitCastExpr>(expr))
        {
          APSInt apsInt;
          if (icastexpr->EvaluateAsInt(apsInt, *(this->context)))
          {
            return to_string(apsInt.getExtValue());
          }
          else
          {
            cout << "GetCaseValue  meet some strange implicitcast expr." << endl;
            expr->dump();
            exit(0);
          }
        }
        else if (DeclRefExpr *declref = dyn_cast<DeclRefExpr>(expr))
        {

          EnumConstantDecl *enumc = dyn_cast<EnumConstantDecl>(declref->getDecl());
          const APSInt apsint = enumc->getInitVal();
          ret = apsint.toString(10);
        }
        else
        {
          cout << "GetCaseValue is not completed!" << endl;
          expr->dump();
          exit(0);
        }
      }
      return ret;
    }
    xmlNodePtr WalkCase(CaseStmt *casestmt)
    {
      xmlNodePtr casenode = xmlNewNode(NULL, BAD_CAST "code"), tmpnode = NULL;
      // casestmt->dump();
      string left, right, casevalue;
      left = GetCaseValue(casestmt->getLHS());
      right = GetCaseValue(casestmt->getRHS());
      xmlNewProp(casenode, BAD_CAST "left", BAD_CAST left.c_str());
      xmlNewProp(casenode, BAD_CAST "right", BAD_CAST right.c_str());
      if (CompoundStmt *cstmt = dyn_cast<CompoundStmt>(casestmt->getSubStmt()))
      {
        tmpnode = WalkCompoundStmt(cstmt);
      }
      else if (WhileStmt *whilestmt =
                   dyn_cast<WhileStmt>(casestmt->getSubStmt()))
      {
        tmpnode = WalkWhileStmt(whilestmt);
      }
      else if (CaseStmt *follow_case =
                   dyn_cast<CaseStmt>(casestmt->getSubStmt()))
      {
        // special cases, e.g.,
        // http://androidxref.com/9.0.0_r3/xref/frameworks/av/media/utils/ISchedulingPolicyService.cpp#91
        // case REQUEST_PRIORITY_TRANSACTION:
        // case REQUEST_CPUSET_BOOST :
        tmpnode = WalkCase(follow_case);
      }
      else if (ReturnStmt *returnstmt =
                   dyn_cast<ReturnStmt>(casestmt->getSubStmt()))
      {
        tmpnode = WalkReturnStmt(returnstmt);
      }
      else if (ParenExpr *parenexpr = dyn_cast<ParenExpr>(casestmt->getSubStmt()))
      {
        if (isDataReplyExists<ParenExpr *>(parenexpr))
        {
          tmpnode = WalkParenExpr(parenexpr);
        }
      }
      else if (CXXMemberCallExpr *cxxmembercallexpr = dyn_cast<CXXMemberCallExpr>(casestmt->getSubStmt()))
      {
        if (isDataReplyExists<CXXMemberCallExpr *>(cxxmembercallexpr))
        {
          tmpnode = WalkCXXMemberCallExpr(cxxmembercallexpr);
        }
      }
      else if (IfStmt *ifStmt = dyn_cast<IfStmt>(casestmt->getSubStmt()))
      {
        if (isDataReplyExists<IfStmt *>(ifStmt))
        {
          tmpnode = WalkIfStmt(ifStmt);
        }
      }
      else if (BinaryOperator *binary = dyn_cast<BinaryOperator>(casestmt->getSubStmt()))
      {
        if (isDataReplyExists<BinaryOperator *>(binary))
        {
          tmpnode = WalkBinaryOperator(binary);
        }
      }
      else if (ExprWithCleanups *exprclean = dyn_cast<ExprWithCleanups>(casestmt->getSubStmt()))
      {
        if (isDataReplyExists<ExprWithCleanups *>(exprclean))
        {
          tmpnode = WalkExprWithCleanups(exprclean);
        }
      }
      else if (BreakStmt *breakstmt = dyn_cast<BreakStmt>(casestmt->getSubStmt()))
      {
        // TODO:
        ;
      }
      else
      {
        DebugInfo("Case is not completed!");
        casestmt->getSubStmt()->dump();
        exit(0);
      }
      if (tmpnode)
        xmlAddChild(casenode, tmpnode);
      return casenode;
    }

    /***Switch Statement Part***/

    void FindSwitch(FunctionDecl *func, vector<SwitchStmt *> &switchs)
    {
      Stmt *funcBody;
      // func->getBody()->dump();
      if (funcBody = dyn_cast<Stmt>(func->getBody()))
        ;
      else
        ;
      // funcBody->dump();
      if (CompoundStmt *cstmt = dyn_cast<CompoundStmt>(funcBody))
      {
        for (CompoundStmt::body_iterator iter = cstmt->body_begin();
             iter != cstmt->body_end(); iter++)
        {
          if (SwitchStmt *switchstmt = dyn_cast<SwitchStmt>(*iter))
          {
            // switchstmt->dump();
            switchs.push_back(switchstmt);
          }
        }
      }
      return;
    }
    xmlNodePtr WalkDefaultStmt(DefaultStmt *defaultstmt)
    {
      xmlNodePtr currnode = NULL;
      Stmt *stmt;
      stmt = defaultstmt->getSubStmt();

      if (ReturnStmt *returnstmt = dyn_cast<ReturnStmt>(stmt))
      {
        currnode = WalkReturnStmt(returnstmt);
      }
      else if (CompoundStmt *compoundstmt = dyn_cast<CompoundStmt>(stmt))
      {
        currnode = WalkCompoundStmt(compoundstmt);
      }
      else if (BreakStmt *breakstmt = dyn_cast<BreakStmt>(stmt))
      {
        ;
      }
      else if (ParenExpr *parenexpr = dyn_cast<ParenExpr>(stmt))
      {
        ;
      }
      else if (DeclStmt *decl = dyn_cast<DeclStmt>(stmt))
      {
        currnode = WalkDeclStmt(decl);
      }
      else if (BinaryOperator *bop = dyn_cast<BinaryOperator>(stmt))
      {
        currnode = WalkBinaryOperator(bop);
      }
      else if (CXXMemberCallExpr *cxxcall = dyn_cast<CXXMemberCallExpr>(stmt))
      {
        currnode = WalkCXXMemberCallExpr(cxxcall);
      }
      else
      {
        DebugInfo("WalkDefaultStmt is not completed!");
        stmt->dump();
        exit(0);
      }
      return currnode;
    }
    xmlNodePtr WalkSwitch(SwitchStmt *switchstmt)
    {
      string test;
      xmlNodePtr switchNode = xmlNewNode(NULL, BAD_CAST "switch"), tmpnode = NULL;
      Expr *expr = switchstmt->getCond();
      if (ImplicitCastExpr *implicitExpr = dyn_cast<ImplicitCastExpr>(expr))
      {
        xmlAddChild(switchNode, WalkImplicitCastExpr(implicitExpr));
      }
      else if (CallExpr *callexpr = dyn_cast<CallExpr>(expr))
      {
        xmlAddChild(switchNode, WalkCallExpr(callexpr));
      }
      else
      {
        DebugInfo("Walk Switch Cond is not completed!");
        expr->dump();
        exit(0);
      }

      CompoundStmt *cstmt = dyn_cast<CompoundStmt>(switchstmt->getBody());
      if (cstmt == NULL)
        return NULL;
      for (CompoundStmt::body_iterator iter = cstmt->body_begin();
           iter != cstmt->body_end(); iter++)
      {
        if (CaseStmt *casestmt = dyn_cast<CaseStmt>(*iter))
        {
          xmlAddChild(switchNode, WalkCase(casestmt));
        }
        else if (DefaultStmt *defaultstmt = dyn_cast<DefaultStmt>(*iter))
        {
          xmlAddChild(switchNode, WalkDefaultStmt(defaultstmt));
          DebugInfo("DefaultStmt is ending");
        }
        else if (CXXMemberCallExpr *cxxcall = dyn_cast<CXXMemberCallExpr>(*iter))
        {
          xmlAddChild(switchNode, WalkCXXMemberCallExpr(cxxcall));
        }
        else if (BreakStmt *breakstmt = dyn_cast<BreakStmt>(*iter))
        {
          continue;
        }
        else if (SwitchStmt *switch1 = dyn_cast<SwitchStmt>(*iter))
        {
          continue;
        }
        else if (ReturnStmt *returnstmt = dyn_cast<ReturnStmt>(*iter))
        {
          continue;
        }
        else if (BinaryOperator *bop = dyn_cast<BinaryOperator>(*iter))
        {
          xmlAddChild(switchNode, WalkBinaryOperator(bop));
        }
        else if (IfStmt *ifstmt = dyn_cast<IfStmt>(*iter))
        {
          xmlAddChild(switchNode, WalkIfStmt(ifstmt));
        }
        else
        {
          DebugInfo("WalkSwitch is not completed!");
          (*iter)->dump();
          exit(0);
        }
      }
      return switchNode;
    }
    vector<int64_t> WalkEnumDecl(EnumDecl *ed)
    {
      vector<int64_t> value;
      xmlNodePtr enumNode = xmlNewNode(NULL, BAD_CAST "enum"), tmpnode = NULL;
      for (clang::EnumDecl::enumerator_iterator iter = ed->enumerator_begin(); iter != ed->enumerator_end(); ++iter)
      {
        EnumConstantDecl *enumc = dyn_cast<EnumConstantDecl>(*iter);
        value.push_back(enumc->getInitVal().getSExtValue());
        // cout << "enum: " << enumc->getInitVal().toString(10) << " type:" << enumc->getType().getAsString() << endl;
      }
      return value;
    }
    bool VisitEnumDecl(EnumDecl *ed)
    {
      string filename = this->context->getSourceManager()
                            .getFilename((ed->getLocation()))
                            .str();
      cout << "filename: " << filename << endl;
      if (filename == "" || filename.find(".h") == string::npos && filename.find(".cpp") == string::npos)
      {
        return true;
      }
      // cout << "Enum Decl" << endl;
      // ed->dump();
      string typeName, promotionType;
      typeName = getType(ed->getTypeForDecl()->getCanonicalTypeInternal());
      // cout << "enum type: " << typeName << endl;
      vector<int64_t> value = WalkEnumDecl(ed);
      promotionType = getType(ed->getPromotionType());
      // 1. enum a{...}; typedef enum a a;
      if (typeName.find("(anonymous") != string::npos)
      {
        // this means there no explicit type during the enum defination.
        Decl *nextDecl = ed->getNextDeclInContext();
        if (nextDecl != NULL)
        {
          // nextDecl->dump();
          if (TypedefDecl *tdecl = dyn_cast<TypedefDecl>(nextDecl))
          {
            string under = getType(tdecl->getUnderlyingType());
            // under.rfind("class ", 0) to elimite some error
            // e.g. http://androidxref.com/9.0.0_r3/xref/frameworks/native/include/gui/BufferQueue.h#43
            if (under.find("*") == string::npos &&
                under.rfind("class ", 0) != 0 &&
                under.find("Vector<") == string::npos &&
                under.find("vector<") == string::npos &&
                under.rfind("struct ", 0) != 0)
            {
              promotionType = under;
              typeName = tdecl->getQualifiedNameAsString();
              // cout << "modified Name: " << typeName << endl;
            }
          }
        }
        if (typeName.find("(anonymous") != string::npos)
        {
          string toSearch = "/", toReplace = ".";
          FindAndReplaceAll(typeName, toSearch, toReplace);
        }
      }
      // 2. typedef enum {}a;
      else if (typeName.find("enum ") == string::npos)
      {
        Decl *nextDecl = ed->getNextDeclInContext();
        if (nextDecl != NULL)
        {
          // nextDecl->dump();
          if (TypedefDecl *tdecl = dyn_cast<TypedefDecl>(nextDecl))
          {
            typeName = getType(tdecl->getUnderlyingType());
          }
          else
          {
            cout << "Unexpected enum meeted..." << endl;
            exit(0);
          }
        }
      }

      string path = "data/enumeration/" + typeName;
      ofstream outfile(path);
      outfile << promotionType << "\n";
      for (auto i : value)
      {
        outfile << i << "\n";
      }
      return true;
    }
    bool VisitTypedefDecl(TypedefDecl *typedefDecl)
    {
      // cout << "TypedefDecl" << endl;
      // typedefDecl->dump();

      string under = getType(typedefDecl->getUnderlyingType());
      string name = typedefDecl->getQualifiedNameAsString();

      // cout << "name: " << name << endl;
      // cout << "Underlying Type: " << under << endl;
      // if (name.find("std::") != string::npos || under.find("std::") != string::npos)
      // {
      //   return true;
      // }
      if (name == under)
      {
        string test;
        raw_string_ostream out(test);
        typedefDecl->dump(out);
        out.str();
        if (test.find("struct") != string::npos)
        {
          under = "struct " + under;
        }
      }
      typeMap.push_back(make_pair(name, under));
      return true;
    }
    string getCXXRecordName(CXXRecordDecl *recordDecl)
    {
      string rootName = recordDecl->getQualifiedNameAsString();
      if (recordDecl->isUnion())
      {
        return "union " + rootName;
      }
      else if (recordDecl->isStruct())
      {
        return "struct " + rootName;
      }
      else if (recordDecl->isClass())
      {
        return "class " + rootName;
      }
      else
      {
        cout << "VisitCXXRecordDecl is not completed.." << endl;
        recordDecl->dump();
        exit(0);
      }
    }
    bool VisitCXXRecordDecl(CXXRecordDecl *recordDecl)
    {
      string rootName = getCXXRecordName(recordDecl);
      if (!recordDecl->isThisDeclarationADefinition())
      {
        return true;
      }

      string filename = this->context->getSourceManager()
                            .getFilename((recordDecl->getLocation()))
                            .str();
      cout << filename << endl;
      if (filename.find("prebuilts/clang/") != string::npos || filename == "" || filename.find(".h") == string::npos && filename.find(".cpp") == string::npos)
      {
        return true;
      }
      if (rootName.rfind("std::", 0) == 0)
      {
        return true;
      }

      // cout << rootName << endl;
      recordDecl->dump();
      string anony = "(anonymous)";
      if (EndsWith(rootName, anony))
      {
        Decl *nextDecl = recordDecl->getNextDeclInContext();
        if (nextDecl != NULL)
        {
          // nextDecl->dump();
          if (TypedefDecl *tdecl = dyn_cast<TypedefDecl>(nextDecl))
          {
            rootName = tdecl->getQualifiedNameAsString();
            cout << rootName << endl;
          }
          else if (FieldDecl *field = dyn_cast<FieldDecl>(nextDecl))
          {
            rootName = getType(field->getCanonicalDecl()->getType());
          }
          else
          {
            cout << "strange things..." << endl;
            exit(0);
          }
        }
        else
        {
          cout << "strange things..." << endl;
          exit(0);
        }
      }
      xmlNodePtr root;
      if (recordDecl->isUnion())
      {
        string tmp = "union";
        root = xmlNewNode(NULL, BAD_CAST tmp.c_str());
      }
      else if (recordDecl->isStruct())
      {
        string tmp = "struct";
        root = xmlNewNode(NULL, BAD_CAST tmp.c_str());
      }
      else if (recordDecl->isClass())
      {
        string tmp = "class";
        root = xmlNewNode(NULL, BAD_CAST tmp.c_str());
      }
      else
      {
        cout << "VisitCXXRecordDecl is not completed.." << endl;
        recordDecl->dump();
        exit(0);
      }
      xmlDocSetRootElement(doc, root);

      for (CXXRecordDecl::field_iterator iter = recordDecl->field_begin(); iter != recordDecl->field_end(); ++iter)
      {
        xmlNodePtr tmp = xmlNewNode(NULL, BAD_CAST "Field");
        string name = iter->getQualifiedNameAsString();
        string type = getType(iter->getType());
        // cout << "name: " << name << endl;
        // cout << "type: " << type << endl;
        // cout << "-------" << endl;
        xmlNewTextChild(tmp, NULL, BAD_CAST "name",
                        BAD_CAST name.c_str());
        xmlNewTextChild(tmp, NULL, BAD_CAST "type", BAD_CAST type.c_str());
        xmlAddChild(root, tmp);
      }
      string output = "data/raw_structure/" + rootName + ".xml";
      xmlSaveFile(output.c_str(), doc);
      // getchar();
      return true;
    }
    BinderFunctionType getBinderFunctionType(string funcName, string funcSignature)
    {
      string full = funcName + "+" + funcSignature;
      cout << "full: " << full << endl;
      if (EndsWith(funcName, "::readFromParcel") ||
          EndsWith(funcName, "::writeToParcel"))
      {
        if (funcSignature.find(",") != string::npos) // more than one argument.
        {
          return DEAL_PARCEL_FUNCTION;
        }
        else
        {
          return STRUCTURE_FUNCTION;
        }
      }
      else if (std::count(misc_parcel_related_function.begin(), misc_parcel_related_function.end(), full) == 1)
      {
        return DEAL_PARCEL_FUNCTION;
      }
      else if (std::count(special_parcelable_function.begin(), special_parcelable_function.end(), full) == 1)
      {
        return STRUCTURE_FUNCTION;
      }
      else
      {
        vector<string> tmp = split(funcName, "::");
        int32_t len = tmp.size();
        string last = tmp[len - 1];
        cout << "misc parcel related function size: " << misc_parcel_related_function.size() << endl;
        for (int i = 0; i < misc_parcel_related_function.size(); ++i)
        {
          if (misc_parcel_related_function[i].find(last) != string::npos && misc_parcel_related_function[i].find(funcSignature) != string::npos)
          {
            return DEAL_PARCEL_FUNCTION;
          }
        }
        for (int i = 0; i < special_parcelable_function.size(); ++i)
        {
          if (special_parcelable_function[i].find(last) != string::npos && special_parcelable_function[i].find(funcSignature) != string::npos)
          {
            return STRUCTURE_FUNCTION;
          }
        }
        return COMMON_FUNCTION;
      }
    }
    bool VisitFunctionDecl(FunctionDecl *func)
    {
      if (func->isVirtualAsWritten())
        return true;

      string funcName = func->getNameAsString();
      string funcSignature = func->getType().getAsString();

      string filename = this->context->getSourceManager()
                            .getFilename((func->getLocation()))
                            .str();

      if (filename == "" || filename.find(".h") == string::npos && filename.find(".cpp") == string::npos && filename.find(".cc") == string::npos)
      {
        return true;
      }
      if (func->isThisDeclarationADefinition() == false)
      {
        // cout << "This is Declaration.." << endl;
        return true;
      }
      // string rootName = func->getCanonicalDecl()->getQualifiedNameAsString();
      string rootName;
      if (CXXMethodDecl *cxxmethod = dyn_cast<CXXMethodDecl>(func))
      {
        CXXRecordDecl *record = cxxmethod->getParent();
        if (record->isStruct())
        {
          rootName = "struct " + record->getQualifiedNameAsString();
        }
        else
        {
          rootName = "class " + record->getQualifiedNameAsString();
        }
        rootName = rootName + "::" + funcName;
      }
      else
      {
        rootName = func->getCanonicalDecl()->getQualifiedNameAsString();
      }
      string outputFilename;
      if (funcNameCount.count(rootName) != 0)
      {
        funcNameCount[rootName] += 1;
        outputFilename = rootName + "-" + to_string(funcNameCount[rootName] - 1);
      }
      else
      {
        outputFilename = rootName;
        funcNameCount[rootName] = 1;
      }
      cout << "rootName     : " << rootName << endl;
      cout << "funcName     : " << funcName << endl;
      cout << "funcSignature: " << funcSignature << endl;

      if (rootName == "android::writeToParcel" || rootName == "android::readFromParcel")
      {
        return true;
      }
      if (rootName == "android::H2BConverter::onTransact")
      {
        func->dump();
      }

      vector<string> namelist = split(rootName, "::");
      if (funcName == "onTransact")
      {
        string a;
        root = xmlNewNode(NULL, BAD_CAST "onTransact");
        xmlDocSetRootElement(doc, root);
        DebugInfo("Start to parse!");
        func->dump();
        vector<SwitchStmt *> switchs;
        FindSwitch(func, switchs);
        if (switchs.size() != 0)
        {
          xmlNodePtr tmp;
          for (uint32_t i = 0; i < switchs.size(); ++i)
          {
            tmp = WalkSwitch(switchs[i]);
            if (tmp)
              xmlAddChild(root, tmp);
          }
          string output = "data/interface/" + outputFilename + ".xml";
          xmlSaveFile(output.c_str(), doc);
        }
      }
      else
      {
        BinderFunctionType type = getBinderFunctionType(rootName, funcSignature);
        cout << "type:" << type << endl;
        if (type == STRUCTURE_FUNCTION)
        {
          func->dump();
          inStructure = true;
          CompoundStmt *cstmt = dyn_cast<CompoundStmt>(func->getBody());
          xmlNodePtr tmp = WalkCompoundStmt(cstmt);
          xmlDocSetRootElement(doc, tmp);
          string output = "data/structure/" + outputFilename + ".xml";
          xmlSaveFile(output.c_str(), doc);
          inStructure = false;
        }
        else if (type == DEAL_PARCEL_FUNCTION)
        {
          func->dump();
          inStructure = true;
          CompoundStmt *cstmt = dyn_cast<CompoundStmt>(func->getBody());
          xmlNodePtr tmp = WalkCompoundStmt(cstmt);
          xmlNodePtr funcInfo = xmlNewNode(NULL, BAD_CAST "function");
          xmlNewTextChild(funcInfo, NULL, BAD_CAST "funcName", BAD_CAST rootName.c_str());
          xmlNewTextChild(funcInfo, NULL, BAD_CAST "signature", BAD_CAST funcSignature.c_str());
          xmlAddChild(funcInfo, tmp);
          xmlNodePtr argv = getFuncArgv(func);
          xmlAddChild(funcInfo, argv);
          xmlDocSetRootElement(doc, funcInfo);
          string output = "data/function/" + outputFilename + ".xml";
          xmlSaveFile(output.c_str(), doc);
          inStructure = false;
        }
      }
      // puts("Done!");
      return true;
    }
  }; // namespace BinderIface
  class BinderIfaceConsumer : public ASTConsumer
  {
    CompilerInstance &Instance;
    std::set<std::string> ParsedTemplates;

  public:
    BinderIfaceConsumer(CompilerInstance &Instance,
                        std::set<std::string> ParsedTemplates)
        : Instance(Instance), ParsedTemplates(ParsedTemplates) {}

  private:
    BinderIfaceASTVisitor visitor;
    // bool HandleTopLevelDecl(DeclGroupRef DG) override {
    //   for (DeclGroupRef::iterator i = DG.begin(), e = DG.end(); i != e; ++i)
    //   {
    //     const Decl *D = *i;
    //     if (const NamedDecl *ND = dyn_cast<NamedDecl>(D))

    //       llvm::errs() << "top-level-decl: \"" << ND->getNameAsString() <<
    //       "\"\n";
    //   }

    //   return true;
    // }

    void HandleTranslationUnit(ASTContext &context) override
    {
      setvbuf(stdin, NULL, _IONBF, 0);
      setvbuf(stdout, NULL, _IONBF, 0);
      ifstream misc_parcel_related_function_in("misc_parcel_related_function.txt", ios::in);
      while (!misc_parcel_related_function_in.eof())
      {
        string tmp;
        getline(misc_parcel_related_function_in, tmp);
        if (tmp.length() < 2)
        {
          continue;
        }
        vector<string> out = split(tmp, "+");
        string funcName = out[0];
        string funcSignature = out[1];
        misc_parcel_related_function.push_back(funcName + "+" + funcSignature);
      }
      ifstream special_parcelable_function_in("special_parcelable_function.txt", ios::in);
      while (!special_parcelable_function_in.eof())
      {
        string tmp;
        getline(special_parcelable_function_in, tmp);
        if (tmp.length() < 2)
        {
          continue;
        }
        vector<string> out = split(tmp, "+");
        string funcName = out[0];
        string funcSignature = out[1];
        special_parcelable_function.push_back(funcName + "+" + funcSignature);
      }

      visitor.setContext(context);
      visitor.TraverseDecl(context.getTranslationUnitDecl());
      std::ifstream t("data/typemap.txt");
      std::stringstream buffer;
      buffer << t.rdbuf();
      std::string contents(buffer.str());
      t.close();
      ofstream out("data/typemap.txt", ios::out | ios::app);
      for (auto item : typeMap)
      {
        string tmp = item.first + "+" + item.second;
        if (contents.find(tmp) != string::npos)
        {
          continue;
        }
        out << tmp << endl;
      }
      out.close();
      cout << "finish......." << endl;
    }
  };

  class BinderIfaceAction : public PluginASTAction
  {
    std::set<std::string> ParsedTemplates;

  protected:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                   llvm::StringRef) override
    {
      return llvm::make_unique<BinderIfaceConsumer>(CI, ParsedTemplates);
    }

    bool ParseArgs(const CompilerInstance &CI,
                   const std::vector<std::string> &args) override
    {
      return true;
    }
  };
} // namespace BinderIface

static FrontendPluginRegistry::Add<BinderIface::BinderIfaceAction>
    X("-extract-binder-iface", "extract binder iface");