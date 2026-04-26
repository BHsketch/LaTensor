#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/ASTMatchers/ASTMatchers.h"

using namespace clang;
using namespace clang::ast_matchers;

class ForLoopCallback : public MatchFinder::MatchCallback {
public:
    std::map<const VarDecl*, bool>* saved_loop_vars;

    void run(const MatchFinder::MatchResult &Result) override {
        if (const ForStmt *FS = Result.Nodes.getNodeAs<ForStmt>("forLoop")) {

            const VarDecl *LoopVar = Result.Nodes.getNodeAs<VarDecl>("loopVar");
            llvm::errs() << "Loop variable: " << LoopVar->getName() << "\n";
            
            if (!LoopVar) {
                llvm::errs() << "ERROR: loopVar not found\n";
                return;
            }

            if (const DeclRefExpr *LHS =
                    Result.Nodes.getNodeAs<DeclRefExpr>("lhsVar")) {

                const VarDecl *VD = dyn_cast<VarDecl>(LHS->getDecl());
                llvm::errs() << "Scalar LHS: " << VD->getName() << "\n";
            }
            else if (const ArraySubscriptExpr *ASE =
                        Result.Nodes.getNodeAs<ArraySubscriptExpr>("lhsArray")) {

                bool contains = containsLoopVar(ASE, LoopVar);
                llvm::errs() << "Contains loop var: " << contains << "\n";

                bool reduction = false;
                if ((*saved_loop_vars).count(LoopVar) > 0) {
                    reduction = (*saved_loop_vars)[LoopVar];
                }
                (*saved_loop_vars)[LoopVar] = !contains || (*saved_loop_vars)[LoopVar];
            }
            else {
                llvm::errs() << "oh noes\n";
            }
        }
    }

    bool containsLoopVar(const Expr *E, const VarDecl *LoopVar) {
        E = E->IgnoreParenImpCasts();

        if (const auto *DRE = dyn_cast<DeclRefExpr>(E)) {
            return DRE->getDecl() == LoopVar;
        }

        for (const Stmt *Child : E->children()) {
            if (const Expr *CE = dyn_cast_or_null<Expr>(Child)) {
                if (containsLoopVar(CE, LoopVar))
                    return true;
            }
        }

        return false;
    }

    void collectIndices(const ArraySubscriptExpr *ASE) {
        const Expr *base = ASE->getBase()->IgnoreParenImpCasts();
        const Expr *idx  = ASE->getIdx()->IgnoreParenImpCasts();

        llvm::errs() << "  Index: \n";
        idx->dump();

        // If the base is another array subscript, recurse
        if (const auto *inner = dyn_cast<ArraySubscriptExpr>(base)) {
            collectIndices(inner);
        }
        else if (const auto *DRE = dyn_cast<DeclRefExpr>(base)) {
            llvm::errs() << "  Base array: " << DRE->getDecl()->getName() << "\n";
        }
    }
};

class ForLoopDetectorConsumer : public ASTConsumer {
public:
    std::map<const VarDecl*, bool> saved_loop_vars {};

    ForLoopDetectorConsumer() {
        Callback.saved_loop_vars = &saved_loop_vars;
        Matcher.addMatcher(SimpleForLoopMatcher, &Callback);
    }

    void HandleTranslationUnit(ASTContext &Context) override {
        Matcher.matchAST(Context);

        llvm::errs() << "\n=== Final LoopVar Map ===\n";
        for (auto &entry : saved_loop_vars) {
            const VarDecl* VD = entry.first;
            bool value = entry.second;

            llvm::errs() << "LoopVar " << VD->getName()
                        << " -> " << value << "\n";
        }
    }

private:
    ForLoopCallback Callback;
    MatchFinder Matcher;
    
    StatementMatcher LHSExpr =
        anyOf(
            // scalar: sum = ...
            declRefExpr().bind("lhsVar"),

            // array: C[i][j] = ...
            arraySubscriptExpr().bind("lhsArray")
        );
    
    StatementMatcher CompoundReduction =
    binaryOperator(
            anyOf(hasOperatorName("+="),
                  hasOperatorName("-="),
                  hasOperatorName("*="),
                  hasOperatorName("/="),
                  hasOperatorName("%=")
            ),
            hasLHS(ignoringParenImpCasts(LHSExpr))
        ).bind("compoundReduction");

    StatementMatcher SimpleReduction =
        binaryOperator(
            hasOperatorName("="),
            hasLHS(ignoringParenImpCasts(LHSExpr)),
            hasRHS(
                binaryOperator(
                    anyOf(
                        hasOperatorName("+"),
                        hasOperatorName("*")
                    ),
                    anyOf(
                        // a = a + b
                        hasLHS(
                            ignoringParenImpCasts(
                                declRefExpr(
                                    to(varDecl(equalsBoundNode("lhsVar")))
                                )
                            )
                        ),
                        // a = b + a
                        hasRHS(
                            ignoringParenImpCasts(
                                declRefExpr(
                                    to(varDecl(equalsBoundNode("lhsVar")))
                                )
                            )
                        )
                    )
                )
            )
        ).bind("simpleReduction");
    
    StatementMatcher AnyReduction =
        stmt(
            anyOf(
                CompoundReduction,
                SimpleReduction
            )
        ).bind("reduction");
    
    StatementMatcher SimpleForLoopMatcher =
        forStmt(
            forEachDescendant(AnyReduction),
            hasLoopInit(
                declStmt(
                    hasSingleDecl(
                        varDecl(
                            hasInitializer(integerLiteral(equals(0)))
                        ).bind("loopVar")
                    )
                )
            ),
            hasCondition(
                binaryOperator(
                    hasOperatorName("<"),
                    hasLHS(ignoringParenImpCasts(
                        declRefExpr(to(
                            varDecl(equalsBoundNode("loopVar"))
                        ))
                    ))
                ).bind("cond")
            ),
            hasIncrement(
                unaryOperator(
                    hasOperatorName("++"),
                    hasUnaryOperand(
                        declRefExpr(to(
                            varDecl(equalsBoundNode("loopVar"))
                        ))
                    )
                ).bind("inc")
            )
        ).bind("forLoop");
};

class ForLoopDetectorAction : public PluginASTAction {
protected:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(
        CompilerInstance &, llvm::StringRef) override {
        return std::make_unique<ForLoopDetectorConsumer>();
    }

    bool ParseArgs(const CompilerInstance &,
                   const std::vector<std::string> &) override {
        return true;
    }
};

static FrontendPluginRegistry::Add<ForLoopDetectorAction>
X("detect-for-loops", "Detects for loops in the AST");
