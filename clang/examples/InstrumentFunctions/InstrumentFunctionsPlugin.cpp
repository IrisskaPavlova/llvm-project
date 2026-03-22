#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <set>

using namespace clang;

namespace {

int currentOption = 1;

// ============================================================
// Вариант 1: Вывод информации о пользовательском типе данных
// ============================================================
class TypeInfoVisitor : public RecursiveASTVisitor<TypeInfoVisitor> {
public:
    TypeInfoVisitor(ASTContext &Context) : Context(Context) {}
    
    bool VisitCXXRecordDecl(CXXRecordDecl *RD) {
        if (RD->isImplicit() || !RD->isThisDeclarationADefinition())
            return true;
        
        llvm::errs() << "\n" << RD->getNameAsString() << "\n";
        
        if (RD->getNumBases() > 0) {
            for (const auto &Base : RD->bases()) {
                llvm::errs() << "  inherits from " << Base.getType().getAsString() << "\n";
            }
        }
        
        llvm::errs() << "  Fields:\n";
        for (FieldDecl *Field : RD->fields()) {
            llvm::errs() << "    - " << Field->getNameAsString() 
                        << " : " << Field->getType().getAsString();
            if (Field->getAccess() == AS_public)
                llvm::errs() << " (public)";
            else if (Field->getAccess() == AS_protected)
                llvm::errs() << " (protected)";
            else
                llvm::errs() << " (private)";
            llvm::errs() << "\n";
        }
        
        llvm::errs() << "  Methods:\n";
        for (CXXMethodDecl *Method : RD->methods()) {
            if (Method->isImplicit()) continue;
            llvm::errs() << "    - " << Method->getNameAsString() << "()";
            if (Method->isVirtual())
                llvm::errs() << " virtual";
            if (Method->isPureVirtual())
                llvm::errs() << " pure";
            llvm::errs() << "\n";
        }
        
        return true;
    }
    
private:
    ASTContext &Context;
};

// ============================================================
// Вариант 2: Поиск неиспользуемых переменных
// ============================================================
class UnusedVarVisitor : public RecursiveASTVisitor<UnusedVarVisitor> {
public:
    UnusedVarVisitor(ASTContext &Context) : Context(Context) {}
    
    bool VisitFunctionDecl(FunctionDecl *FD) {
        if (!FD->hasBody()) return true;
        
        std::set<const VarDecl*> allVars;
        std::set<const VarDecl*> usedVars;
        
        for (auto *Param : FD->parameters()) {
            allVars.insert(Param);
        }
        
        FindUsedVars(FD->getBody(), usedVars);
        
        for (auto *Var : allVars) {
            if (usedVars.find(Var) == usedVars.end()) {
                llvm::errs() << "Unused: " << Var->getNameAsString() 
                            << " in function " << FD->getNameAsString() << "\n";
            }
        }
        
        return true;
    }
    
    void FindUsedVars(Stmt *S, std::set<const VarDecl*> &usedVars) {
        if (!S) return;
        if (DeclRefExpr *DR = dyn_cast<DeclRefExpr>(S)) {
            if (VarDecl *VD = dyn_cast<VarDecl>(DR->getDecl())) {
                usedVars.insert(VD);
            }
        }
        for (Stmt *Child : S->children()) {
            FindUsedVars(Child, usedVars);
        }
    }
    
private:
    ASTContext &Context;
};

// ============================================================
// Вариант 3: Подсчет неявных преобразований
// ============================================================
class ImplicitCastVisitor : public RecursiveASTVisitor<ImplicitCastVisitor> {
public:
    ImplicitCastVisitor(ASTContext &Context) : Context(Context) {}
    
    bool VisitFunctionDecl(FunctionDecl *FD) {
        if (!FD->hasBody()) return true;
        currentFunction = FD->getNameAsString();
        return true;
    }
    
    bool VisitImplicitCastExpr(ImplicitCastExpr *ICE) {
        QualType FromType = ICE->getSubExpr()->getType();
        QualType ToType = ICE->getType();
        if (FromType != ToType) {
            casts[currentFunction]++;
            llvm::errs() << "  " << FromType.getAsString() << " -> " << ToType.getAsString() << "\n";
        }
        return true;
    }
    
    void printResults() {
        llvm::errs() << "\n=== Implicit Casts Summary ===\n";
        for (auto &pair : casts) {
            llvm::errs() << "Function '" << pair.first << "': " << pair.second << " casts\n";
        }
    }
    
private:
    ASTContext &Context;
    std::string currentFunction;
    std::map<std::string, int> casts;
};

// ============================================================
// Вариант 4: Добавление префиксов к именам переменных (упрощенная версия)
// ============================================================
class VariablePrefixVisitor : public RecursiveASTVisitor<VariablePrefixVisitor> {
public:
    VariablePrefixVisitor(ASTContext &Context) : Context(Context) {}
    
    bool VisitVarDecl(VarDecl *VD) {
        if (VD->isImplicit()) return true;
        
        std::string name = VD->getNameAsString();
        std::string prefix;
        
        if (VD->isFileVarDecl() && !VD->isStaticLocal()) {
            prefix = "global_";
        }
        else if (VD->isStaticLocal()) {
            prefix = "static_";
        }
        else if (VD->isLocalVarDecl() && !VD->isStaticLocal()) {
            prefix = "local_";
        }
        
        if (!prefix.empty() && name.find(prefix) != 0) {
            llvm::errs() << "Renaming: " << name << " -> " << prefix << name << "\n";
        }
        return true;
    }
    
    bool VisitParmVarDecl(ParmVarDecl *PVD) {
        std::string name = PVD->getNameAsString();
        if (name.find("param_") != 0) {
            llvm::errs() << "Renaming param: " << name << " -> param_" << name << "\n";
        }
        return true;
    }
    
private:
    ASTContext &Context;
};

// ============================================================
// AST Consumer
// ============================================================
class PluginASTConsumer : public ASTConsumer {
public:
    PluginASTConsumer(ASTContext &Context, int Option) 
        : Context(Context), Option(Option) {}
    
    void HandleTranslationUnit(ASTContext &Context) override {
        llvm::errs() << "\n=== Lab1 Plugin - Option " << Option << " ===\n";
        
        switch (Option) {
            case 1: {
                TypeInfoVisitor Visitor(Context);
                Visitor.TraverseDecl(Context.getTranslationUnitDecl());
                break;
            }
            case 2: {
                UnusedVarVisitor Visitor(Context);
                Visitor.TraverseDecl(Context.getTranslationUnitDecl());
                break;
            }
            case 3: {
                ImplicitCastVisitor Visitor(Context);
                Visitor.TraverseDecl(Context.getTranslationUnitDecl());
                Visitor.printResults();
                break;
            }
            case 4: {
                VariablePrefixVisitor Visitor(Context);
                Visitor.TraverseDecl(Context.getTranslationUnitDecl());
                break;
            }
        }
        llvm::errs() << "====================\n\n";
        llvm::errs().flush();
    }
    
private:
    ASTContext &Context;
    int Option;
};

// ============================================================
// Plugin Registration
// ============================================================
class Plugin : public PluginASTAction {
public:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef File) override {
        return std::make_unique<PluginASTConsumer>(CI.getASTContext(), currentOption);
    }
    
    bool ParseArgs(const CompilerInstance &CI, const std::vector<std::string> &args) override {
        for (const auto &arg : args) {
            if (arg == "option1") currentOption = 1;
            else if (arg == "option2") currentOption = 2;
            else if (arg == "option3") currentOption = 3;
            else if (arg == "option4") currentOption = 4;
        }
        return true;
    }
    
    PluginASTAction::ActionType getActionType() override {
        return AddBeforeMainAction;
    }
    
private:
    int currentOption = 1;
};

}

static FrontendPluginRegistry::Add<Plugin>
    X("instrument-functions", "Lab1 plugin with 4 options");
